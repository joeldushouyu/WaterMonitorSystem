/*
This example creates an SSLclient object that connects and transfers
data using always SSL.

I then use it to connect to smpt.gmail.com and send an email.

SMTP command reference:  https://www.samlogic.net/articles/smtp-commands-reference.htm

Things to note:
* gmail sending account: must have 2 factor identification enabled and an app specific password created
*                        for this usage
* lots of things are tricky here!
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include "helpers.h"
#include "arduino_secrets.h"
#include <vector>
#include <string>
#include <TimeLib.h>

#define ERROR_LED 12
#define FLOW_PRESENT 11
#define NO_FLOW_PRESENT 10

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
const char ssid[] = SECRET_SSID;  // your network SSID (name)
const char pass[] = SECRET_PASS;  // your network password (use for WPA)
const char username[] = SECRET_USERNAME;

// information for the gmail account used to send alert email
const String gAcc = SECRET_SEND_ACCOUNT,  // "animerecommendationsdu@gmail.com"
  gPass = SECRET_SEND_ACCOUNT_PASSWORD;   // "the_password"

const int port = 465;                    //587 for TLS does not work
const char server[] = "smtp.gmail.com";  // name address for Gmail SMTP (using DNS)
const char numberOfRecipient = 1;
byte mac[6];



char stateStatus = 100;  // 0 means no waterflow, 1 means water flow, 2 is error, 100 means need initalize
char previousStateStatus = 0;
char maxFlowTime = 10;
char maxFlowTimeErrorCounter = 0;
char maxNoFlowTime = 20;
char maxNoFlowTimeErrorCounter = 0;
time_t startFlowTime, endFlowTime;
time_t endFlowRateTime;
time_t prevDisplay = 0;  // when the digital clock was displayed

time_t lastPingGoogleTime = 0;
// WiFiClient timeClient;

WiFiClient client;  //deprecated

bool matchWord(const char *start, const char *end, const String word);
void requestTimeFromWebsite(WiFiClient &client);
time_t findUnixTime(const String &originalString);
void printMacAddress();
void printMacAddress(byte mac[]) ;
void addEmailRecipient(WiFiSSLClient &client);
time_t getWifiTime();
bool sendEmailAlert(char *message);
bool isWaterFlowing();
void printDigits(int digits);
void digitalClockDisplay();
int timeDifferenceInMinute(time_t t);
void requestTimeFromWebsite2();
time_t getTimeFromWebsite();

void controlLEDBaseOnStatus();
void updateStateStatus(char newStatus);
String timeToStr(time_t timeInUnix);
time_t timeDifference(time_t oldTime, time_t newerTime);
void printEncryptionType(int thisType) ;
void reconnectToWifi();
void print2Digits(byte thisByte);
void pingGoogle();

void listNetworks();
void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  Serial1.begin(9600);
  //while (!Serial);
  pinMode(LED_BUILTIN, OUTPUT); //wifi led




  // setup pins for detecing waterFlow
  pinMode(2, INPUT);
  pinMode(ERROR_LED, OUTPUT);
  pinMode(FLOW_PRESENT, OUTPUT);
  pinMode(NO_FLOW_PRESENT, OUTPUT);

  //////////////////////  First Connect to WIFI /////////////////////////
  printMacAddress();
  Serial.println("waiting for sync by connecting to wifi");
  Serial1.println("waiting for sync by connecting to wifi");  
  delay(1000);
  //analogWrite(LED_BUILTIN,100);
  //_connectToWifi(ssid, pass);
  WiFi.lowPowerMode();
  listNetworks();
  //__connectToWifiEnterprise(ssid, username, pass);
  _connectToWifi(ssid, pass);
  // while(timeStatus() == timeNotSet){
  //   setTime(getTimeFromWebsite());
  // }

  // setSyncProvider(getTimeFromWebsite);
  // setSyncInterval(5*60);// every 5 minutes
  setSyncInterval(-1);
  setTime(1357041600);
  digitalWrite(LED_BUILTIN, HIGH);  // light led indicate connected to wifi
  delay(1000);
  pingGoogle();
}



time_t getTimeFromWebsite() {
  requestTimeFromWebsite2();


  time_t res = 0;
  time_t timeNow = now();

  while (client.available()) {
    // char c = client.read();
    // client.readStringUntil()

    String s = client.readString();
    //Serial.println(s);


    if (s == "") {

      res = timeNow;
    } else {
      res = findUnixTime(s);  //TODO: use regex later
    }

    // auto endWithIndex;
    // Serial.println(k);
    //Serial.write(c);
  }
  Serial.println("The final unix timestamp");
  Serial.println(res, DEC);

  return res;
}
void loop() {
  //Serial.println(getWifiTime(),DEC);
  int sensorVal = digitalRead(2);
  time_t timeNow = now();
  Serial.print("Wifi status  ");
  Serial.println(WiFi.status());
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("loss wifi!!!");
    Serial1.println("loss wifi!");
    reconnectToWifi(); // while loop
  }else{
    // means has wifi
  }

  // time_t pingTimeDifference = timeDifference(lastPingGoogleTime, timeNow);
  // Serial.print("Ping time difference: ");
  // Serial.println(pingTimeDifference);
  // if(pingTimeDifference > 5*60){//pin every  minute
  //      pingGoogle();
  //      lastPingGoogleTime=timeNow;
  // } 
  // if (timeStatus() != timeNotSet) {
  //   if (now() != prevDisplay) {  //update the display only if time has changed
  //     prevDisplay = now();
  //     digitalClockDisplay();
  //   }
  // } else {
  //   Serial.println("time status is not set");
  //   return;
  // }


  Serial.print("the curent state is ");
  Serial.println(stateStatus, DEC);
  Serial.print("The previous state is ");
  Serial.println(previousStateStatus, DEC);
  Serial.print("The current sensor reading is ");
  Serial.println(sensorVal, DEC);

  Serial1.print("the curent state is ");
  Serial1.println(stateStatus, DEC);
  Serial1.print("The previous state is ");
  Serial1.println(previousStateStatus, DEC);
  Serial1.print("The current sensor reading is ");
  Serial1.println(sensorVal, DEC);

  if (stateStatus == 100) {
    // means need initialize
    if (sensorVal == 1) {
      startFlowTime = timeNow;
      updateStateStatus(1);
    } else {
      endFlowRateTime = timeNow;
      updateStateStatus(0);
    }
    lastPingGoogleTime=timeNow;

  } else if (stateStatus == 0) {
    // means no water flow

    if (sensorVal == 1) {
      // means flow  start
      startFlowTime = timeNow;
      updateStateStatus(1);
      maxFlowTimeErrorCounter = 0;
      maxNoFlowTimeErrorCounter = 0;

    } else {
      // means no flow
      time_t difference = timeDifference(endFlowRateTime, timeNow);
      int minute = timeDifferenceInMinute(difference);
      Serial.print("The non water flow time difference in second is ");
      Serial.print(difference);

      Serial.print("The max no flowtime error counter is ");
      Serial.println(maxNoFlowTimeErrorCounter, DEC);

      // int temp = (minute - maxNoFlowTime * maxNoFlowTimeErrorCounter);
      // Serial.print("The no water flow differece is ");
      // Serial.println(temp);
      if ((minute - maxNoFlowTime * maxNoFlowTimeErrorCounter) >= maxNoFlowTime) {
        // need to trigger an error state to send email
        updateStateStatus(2);
      } else {
        // let us continue waiting
        // this could mean
        // no error happen, maxNoFlowTimeErrorCounter ==0
        // or error already happened once, and we already send a email about it.
        ;
      }
    }
  } else if (stateStatus == 1) {
    if (sensorVal == 1) {
      // water is flowing;


      time_t difference = timeDifference(startFlowTime, timeNow);
      int min = timeDifferenceInMinute(difference);
      Serial.print("The water flow time difference in second is ");
      Serial.println(difference);

      Serial.print("The max flowtime error counter is ");
      Serial.println(maxFlowTimeErrorCounter, DEC);
      // int temp = min - maxFlowTime * maxFlowTimeErrorCounter;
      // // Serial.print("The water flow difference is ");
      // // Serial.println(temp, DEC);
      if ((min - maxFlowTime * maxFlowTimeErrorCounter) >= maxFlowTime) {
        // trigger into error state
        updateStateStatus(2);
      } else {
        // continue waiting
        // means either error alread happened and already send out a email about it,  maxFlowTimeErrorCounter>1, or no error happened yet.
      }
    } else {
      // means need to switch to state 0
      updateStateStatus(0);
      maxFlowTimeErrorCounter = 0;
      maxNoFlowTimeErrorCounter = 0;
      endFlowRateTime = timeNow;
    }
  } else {

    Serial.println("In error state function");

    if (previousStateStatus == 1) {
      Serial.print("Error for water is flowing: ");
      time_t diff = timeDifference(startFlowTime, timeNow);
      int minutes = timeDifferenceInMinute(diff);
      Serial.println(minutes, DEC);
      //sendEmailAlert("const water flow longer than expected time");
      // stateStatus = 1;// continue to go back
      // maxFlowTimeErrorCounter++;
      // maxNoFlowTimeErrorCounter = 0;

      //TODO: remove later?
      if (maxFlowTimeErrorCounter == 0) {
        bool status = sendEmailAlert("const water flow longer than expected time");
        if (status == false) {
          // means gmail did not send through
          // do not update status, comeback again
          maxFlowTimeErrorCounter = 0;  // means come back later
          maxNoFlowTimeErrorCounter = 0;
          Serial.println("Failed to send out constant waterflow email");

        } else {
          // means able to send warning email
          updateStateStatus(1);
          maxFlowTimeErrorCounter++;
          maxNoFlowTimeErrorCounter = 0;
        }
      } else {
        // error email already send, do not send email, but update the rest.
        updateStateStatus(1);
        maxFlowTimeErrorCounter++;
        maxNoFlowTimeErrorCounter = 0;
      }


    } else {
      Serial.println("Error for no water is flowing");
      time_t diff = timeDifference(endFlowRateTime, timeNow);
      int minutes = timeDifferenceInMinute(diff);
      Serial.println(minutes);
      //sendEmailAlert("No water flow longer than expected time");


      if (maxNoFlowTimeErrorCounter == 0) {
        bool status = sendEmailAlert("No water flow longer than expected time");
        if (status == false) {
          // means gmail did not send through
          // do not update status, comeback again
          maxFlowTimeErrorCounter = 0;  // means come back later
          maxNoFlowTimeErrorCounter = 0;
        } else {
          // means able to send warning email
          updateStateStatus(0);
          maxFlowTimeErrorCounter = 0;
          maxNoFlowTimeErrorCounter++;
        }
      } else {
        // error email already send, do not send email, but update the rest
        updateStateStatus(0);
        maxFlowTimeErrorCounter = 0;
        maxNoFlowTimeErrorCounter++;
      }
    }
  }

  controlLEDBaseOnStatus();

  delay(1000);
}


void reconnectToWifi(){
  //digitalWrite(LED_BUILTIN,LOW);
  analogWrite(LED_BUILTIN,200);
  WiFi.end();
  listNetworks();
  __connectToWifiEnterprise(ssid, username, pass); //while loop that connect to wifi
  // reconnect to wifi
  digitalWrite(LED_BUILTIN, HIGH);
}
void controlLEDBaseOnStatus() {


  // turn all led off first
  digitalWrite(ERROR_LED, LOW);
  digitalWrite(FLOW_PRESENT, LOW);
  digitalWrite(NO_FLOW_PRESENT, LOW);



  // if (stateStatus == 0 || previousStateStatus == 0) {
  //   digitalWrite(NO_FLOW_PRESENT, HIGH);
  // }
  // if (stateStatus == 1 || previousStateStatus == 1) {
  //   digitalWrite(FLOW_PRESENT, HIGH);
  // }
  if (stateStatus == 2 || previousStateStatus == 2) {
    digitalWrite(ERROR_LED, HIGH);
    if (stateStatus == 0 || previousStateStatus == 0) {
      digitalWrite(NO_FLOW_PRESENT, HIGH);
    }
    if (stateStatus == 1 || previousStateStatus == 1) {
      digitalWrite(FLOW_PRESENT, HIGH);
    }
  }else{
    // means it is normal, no red light
    if(stateStatus == 0){
      digitalWrite(NO_FLOW_PRESENT, HIGH);
    }else if(stateStatus == 1){
       digitalWrite(FLOW_PRESENT, HIGH);     
    }
  }
  // if (stateStatus == 100) {
  //   // means at initalize stage
  //   digitalWrite(ERROR_LED, HIGH);
  //   digitalWrite(FLOW_PRESENT, HIGH);
  //   digitalWrite(NO_FLOW_PRESENT, HIGH);
  // }
}

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

// String  timeToStr(time_t timeInUnix){
//   char timeBuf[100];

//   String s = std::to_string(year(timeInUnix)) + "/"+std::to_string(month(timeInUnix)) + "/" + std::to_string(day(timeInUnix)) + ":" + std::to_string(hour(timeInUnix)) + ":" + std::to_string((minute(timeInUnix))) + ":" + std::to_string((second(timeInUnix)));

//   return s;
// }

void printDigits(int digits) {
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}


void updateStateStatus(char newStatus) {

  previousStateStatus = stateStatus;
  stateStatus = newStatus;
}

// bool isWaterFlowing() {
//   int sensorVal = digitalRead(2);
//   if (sensorVal == 1) {
//     return true;
//   } else {
//     return false;
//   }
// }


bool sendEmailAlert(char *message) {


  Serial.println("Sending out email");
  int encodedLength = Base64.encodedLength(gAcc.length());
  char encodedAccount[encodedLength + 1];
  encode64(gAcc, encodedAccount);
  encodedAccount[encodedLength] = '\0';

  encodedLength = Base64.encodedLength(gPass.length());
  char encodedPass[encodedLength + 1];
  encode64(gPass, encodedPass);
  encodedPass[encodedLength] = '\0';
  ///////////////////// then connect to server ///////////////////////////

  Serial.println("\nConnecting to server: " + String(server) + ":" + String(port));

  WiFiSSLClient client;


  if (client.connectSSL(server, port) == 1) {
    Serial.println("Connected to server");
    if (response(client) == -1) {
      String s = server + String(" port:") + String(port);
      Serial.print("no reply on connect to ");
      Serial.println(s);
    }

    /////////////////////// now do the SMTP dance ////////////////////////////

    Serial.println("Sending Extended Hello: <start>EHLO gmail.com<end>");

    //// PLEASE UPDATE /////
    client.println("EHLO gmail.com");
    if (response(client) == -1) {
      Serial.println("no reply EHLO gmail.com");
    }

    Serial.println("Sending auth login: <start>AUTH LOGIN<end>");
    client.println(F("AUTH LOGIN"));
    if (response(client) == -1) {
      Serial.println("no reply AUTH LOGIN");
    }

    Serial.println("Sending account: <start>" + String(encodedAccount) + "<end>");
    client.println(F(encodedAccount));
    if (response(client) == -1) {
      Serial.println("no reply to Sending User");
    }

    Serial.println("Sending Password: <start>" + String(encodedPass) + "<end>");
    client.println(F(encodedPass));
    if (response(client) == -1) {
      Serial.println("no reply Sending Password");
    }

    //// PLEASE UPDATE /////
    Serial.println("Sending From: <start>MAIL FROM: <animerecommendationsdu@gmail.com><end>");
    // your email address (sender) - MUST include angle brackets
    client.println(F("MAIL FROM: <animerecommendationsdu@gmail.com>"));
    if (response(client) == -1) {
      Serial.println("no reply Sending From");
    }

    //// PLEASE UPDATE /////
    // change to recipient address - MUST include angle brackets
    // Serial.println("Sending To: <start>RCPT To: <sdu568@students.bju.edu><end>");
    // client.println(F("RCPT To: <sdu568@students.bju.edu>"));
    // client.println(F("RCPT To: <joeldushouyu@gmail.com>"));
    // if (response(client) ==-1){
    //   Serial.println("no reply Sending To");
    // }
    addEmailRecipient(client);

    Serial.println("Sending DATA: <start>DATA<end>");
    client.println(F("DATA"));
    if (response(client) == -1) {
      Serial.println("no reply Sending DATA");
    }

    //// PLEASE UPDATE /////
    Serial.println("Sending email: <start>");
    // recipient address (include option display name if you want)
    client.println(F("To: Global Challenge Team"));  //client.println(F("To: SomebodyElse <sdu568@students.bju.edu>"));

    // //// PLEASE UPDATE /////
    // const char outgoing[] = "This is a test message!\n";

    //// PLEASE UPDATE /////
    // send from address, subject and message
    client.println(F("From: Aquaponic system montior <animerecommendationsdu@gmail.com>"));
    client.println(F("Subject: Aquaponic system alert !"));
    client.println(message);

    // IMPORTANT you must send a complete line containing just a "." to end the conversation
    // So the PREVIOUS line to this one must be a println not just a print
    client.println(F("."));
    if (response(client) == -1) {
      Serial.println("no reply Sending '.'");
    }

    Serial.println(F("Sending QUIT"));
    client.println(F("QUIT"));
    if (response(client) == -1) {
      Serial.println("no reply Sending QUIT");
    }
    client.stop();
  } else {
    Serial.println("failed to connect to server");
    return false;
  }
  Serial.println("Done.");
  return true;
}


void addEmailRecipient(WiFiSSLClient &client) {

  // char emails[numberOfRecipient][50] = {
  //   "sloef714@students.bju.edu",
  //   "slott467@students.bju.edu",
  //   "dhaas971@students.bju.edu",
  //   "mpark672@students.bju.edu",
  //   "nhard538@students.bju.edu",
  //   "mthom521@students.bju.edu",
  //   "mdalp143@students.bju.edu",
  //   "dwinn758@students.bju.edu",
  //   "kwayc426@students.bju.edu",
  //   "pvosb324@students.bju.edu",
  //   "scass554@students.bju.edu",
  //   "amose658@students.bju.edu",
  //   "blovegro@bju.edu",
  //   "szimm347@students.bju.edu",
  //   "sdu568@students.bju.edu"
  // };
  char emails[numberOfRecipient][40]{
    "sdu568@students.bju.edu"
  };

  // Serial.println("Sending To: <start>RCPT To: <sdu568@students.bju.edu><end>");
  // client.println(F("RCPT To: <sdu568@students.bju.edu>"));
  // client.println(F("RCPT To: <joeldushouyu@gmail.com>"));

  for (size_t i = 0; i < numberOfRecipient; i++) {
    char messageToStmp[100];  //= "RCPT To: <" + listOfRecipient[i] + ">";
    sprintf(messageToStmp, "RCPT To: <%s>", emails[i]);
    Serial.println(messageToStmp);
    client.println(F(messageToStmp));
    if (response(client) == -1) {
      Serial.println("no reply Sending To");
    }
  }
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

void printMacAddress() {
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  Serial.print(mac[5], HEX);
  Serial.print(":");
  Serial.print(mac[4], HEX);
  Serial.print(":");
  Serial.print(mac[3], HEX);
  Serial.print(":");
  Serial.print(mac[2], HEX);
  Serial.print(":");
  Serial.print(mac[1], HEX);
  Serial.print(":");
  Serial.println(mac[0], HEX);
}

void pingGoogle(){
  char server[] = "www.google.com"; // name address for Google (using DNS)
  WiFiClient googleClient;
  if (googleClient.connect(server, 80)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    googleClient.println("GET /search?q=arduino HTTP/1.1");
    googleClient.println("Host: www.google.com");
    googleClient.println("Connection: close");
    googleClient.println();
  }else{
    Serial.print("failed to connect to Google");
  }
  delay(10000);
  while(googleClient.available()){
    char c = googleClient.read();
    Serial.write(c);
  }
  googleClient.stop();


}

void requestTimeFromWebsite2() {
  char server[] = "www.worldclockapi.com";  // name address for Google (using DNS)
  if (client.connect(server, 80)) {
    Serial.println("connected to server for time");
    // Make a HTTP request:
    client.println("GET /api/json/est/now HTTP/1.1");
    client.println("Host: worldclockapi.com");
    client.println("Connection: close");
    client.println();
  }
  delay(10000);
}

bool matchWord(const char *start, const char *end, const String word) {

  for (auto k = start, k2 = word.begin(); k != end && k2 != word.end(); k2++, k++) {
    if (*k != *k2) {
      return false;
    }
  }
  return true;
}

// A function that used to parse and extract the date information from the webresponse
time_t findUnixTime(const String &originalString) {

  bool hasFound = false;
  int j = 0;
  String result = "";
  time_t res = 0;
  Serial.println(originalString);
  for (auto c = originalString.begin(); c != originalString.end(); c++, j++) {
    if (matchWord(c, originalString.end(), "currentDateTime")) {

      //means already found
      hasFound = true;
      c += 18;
      Serial.println("The index");
      Serial.println(j, DEC);
      //get hour
      String year = "";
      for (int i = 0; i < 4; i++) {
        year += *c;
        c++;
      }
      c++;
      // get month
      String month = "";
      for (int i = 0; i < 2; i++) {
        month += *c;
        c++;
      }
      c++;
      // get date
      String date = "";
      for (int i = 0; i < 2; i++) {
        date += *c;
        c++;
      }
      c++;
      String hour = "";
      for (int i = 0; i < 2; i++) {
        hour += *c;
        c++;
      }
      c++;

      String minute = "";
      for (int i = 0; i < 2; i++) {
        minute += *c;
        c++;
      }
      c++;
      String second = "";
      for (int i = 0; i < 2; i++) {
        second += *c;
        c++;
      }

      // Serial.println("year");
      // Serial.println(year);
      // Serial.println("date");
      // Serial.println(date);
      // Serial.println("hour");
      // Serial.println(hour);
      // Serial.println("minute");
      // Serial.println(minute);
      // Serial.println("second");
      // Serial.println(second);



      tmElements_t tm;
      tm.Year = atoi(year.begin()) - 1970;
      tm.Month = atoi(month.begin());
      tm.Day = atoi(date.begin());
      tm.Hour = atoi(hour.begin());
      tm.Minute = atoi(minute.begin());
      tm.Second = atoi(second.begin());
      res = makeTime(tm);
      break;
    } else {
    }
  }

  //return atoll(result.begin());

  return res;
}


int timeDifferenceInMinute(time_t t) {
  return t / 60;
}
time_t timeDifference(time_t oldTime, time_t newerTime) {
  // return the time difference in second
  //

  return newerTime - oldTime;
}


void listNetworks() {
  // scan for nearby networks:
  Serial.println("** Scan Networks **");
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1)
  {
    Serial.println("Couldn't get a WiFi connection");
    while (true);
  }

  // print the list of networks seen:
  Serial.print("number of available networks: ");
  Serial.println(numSsid);

  // print the network number and name for each network found:
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(thisNet + 1);
    Serial.print(") ");
    Serial.print("Signal: ");
    Serial.print(WiFi.RSSI(thisNet));
    Serial.print(" dBm");
    Serial.print("\tChannel: ");
    Serial.print(WiFi.channel(thisNet));
    byte bssid[6];
    Serial.print("\t\tBSSID: ");
    printMacAddress(WiFi.BSSID(thisNet, bssid));
    Serial.print("\tEncryption: ");
    printEncryptionType(WiFi.encryptionType(thisNet));
    Serial.print("\t\tSSID: ");
    Serial.println(WiFi.SSID(thisNet));
    Serial.flush();
  }
  Serial.println();
}

void printEncryptionType(int thisType) {
  // read the encryption type and print out the name:
  switch (thisType) {
    case ENC_TYPE_WEP:
      Serial.print("WEP");
      break;
    case ENC_TYPE_TKIP:
      Serial.print("WPA");
      break;
    case ENC_TYPE_CCMP:
      Serial.print("WPA2");
      break;
    case ENC_TYPE_NONE:
      Serial.print("None");
      break;
    case ENC_TYPE_AUTO:
      Serial.print("Auto");
      break;
    case ENC_TYPE_UNKNOWN:
    default:
      Serial.print("Unknown");
      break;
  }
}

void print2Digits(byte thisByte) {
  if (thisByte < 0xF) {
    Serial.print("0");
  }
  Serial.print(thisByte, HEX);
}




