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
// #include <TimeLib.h>
#include <time.h>
#define ERROR_LED_TANK_1 12
#define FLOW_PRESENT_TANK_1 11
#define NO_FLOW_PRESENT_TANK_1 10
#define SENSOR_PIN_TANK_1 2

#define ERROR_LED_TANK_2 9
#define FLOW_PRESENT_TANK_2 7
#define NO_FLOW_PRESENT_TANK_2 8
#define SENSOR_PIN_TANK_2 5
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







time_t lastPingGoogleTime = 0;

time_t timeCounter = 0;
void printMacAddress();
void printMacAddress(byte mac[]) ;
void addEmailRecipient(WiFiSSLClient &client);
bool sendEmailAlert(char *message);
int timeDifferenceInMinute(time_t t);

time_t timeDifference(time_t oldTime, time_t newerTime);
void printEncryptionType(int thisType) ;
void reconnectToWifi();

void pingGoogle();
void listNetworks();
time_t readTimeCounter();



void controlLEDBaseOnStatus(char stateStatus, char previousStateStatus, char errorLEDPin, char flowPresentLEDPin, char noFlowPresentLEDPin);
void updateStateStatus(volatile char *previousStatus, volatile char *stateStatus,  char newStatus);

//tank 1 specific
int detectFlowSensor1();

volatile char stateStatusTank1 = 100;  // 0 means no waterflow, 1 means water flow, 2 is error, 100 means need initalize
volatile char previousStateStatusTank1 = 0;
const char maxFlowTimeTank1 = 10;
volatile char maxFlowTimeErrorCounterTank1 = 0;
const char maxNoFlowTimeTank1 = 20;
volatile char maxNoFlowTimeErrorCounterTank1 = 0;
volatile time_t startFlowTimeTank1, endFlowTimeTank1;
volatile time_t endFlowRateTimeTank1;


void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  Serial1.begin(9600);
  //while (!Serial);
  pinMode(LED_BUILTIN, OUTPUT); //wifi led




  // setup pins for detecing waterFlow
  pinMode(SENSOR_PIN_TANK_1, INPUT);
  pinMode(ERROR_LED_TANK_1, OUTPUT);
  pinMode(FLOW_PRESENT_TANK_1, OUTPUT);
  pinMode(NO_FLOW_PRESENT_TANK_1, OUTPUT);

  //////////////////////  First Connect to WIFI /////////////////////////
  printMacAddress();
  Serial.println("waiting for sync by connecting to wifi");
  Serial1.println("waiting for sync by connecting to wifi");  
  delay(1000);
  //analogWrite(LED_BUILTIN,100);
  //_connectToWifi(ssid, pass);
  //WiFi.lowPowerMode();
  listNetworks();
  __connectToWifiEnterprise(ssid, username, pass);
  //_connectToWifi(ssid, pass);
  //setSyncInterval(-1);
  //setTime(1357041600);
  digitalWrite(LED_BUILTIN, HIGH);  // light led indicate connected to wifi
  delay(1000);
  pingGoogle();
}


void loop() {
  //Serial.println(getWifiTime(),DEC);
  int sensorVal = detectFlowSensor1();
  time_t timeNow = readTimeCounter();
  Serial.print("Wifi status  ");
  Serial.println(WiFi.status());
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("loss wifi!!!");
    Serial1.println("loss wifi!");
    reconnectToWifi(); // while loop
  }else{
    // means has wifi
  }


  Serial.print("the curent state is ");
  Serial.println(stateStatusTank1, DEC);
  Serial.print("The previous state is ");
  Serial.println(previousStateStatusTank1, DEC);
  Serial.print("The current sensor reading is ");
  Serial.println(sensorVal, DEC);

  Serial1.print("the curent state is ");
  Serial1.println(stateStatusTank1, DEC);
  Serial1.print("The previous state is ");
  Serial1.println(previousStateStatusTank1, DEC);
  Serial1.print("The current sensor reading is ");
  Serial1.println(sensorVal, DEC);

  if (stateStatusTank1 == 100) {
    // means need initialize
    if (sensorVal == 1) {
      startFlowTimeTank1 = timeNow;
      updateStateStatus(&previousStateStatusTank1, &stateStatusTank1, 1);
    } else {
      endFlowRateTimeTank1 = timeNow;
      updateStateStatus( &previousStateStatusTank1, &stateStatusTank1,0);
    }
    lastPingGoogleTime=timeNow;

  } else if (stateStatusTank1 == 0) {
    // means no water flow

    if (sensorVal == 1) {
      // means flow  start
      startFlowTimeTank1 = timeNow;
      updateStateStatus(&previousStateStatusTank1, &stateStatusTank1, 1);
      maxFlowTimeErrorCounterTank1 = 0;
      maxNoFlowTimeErrorCounterTank1 = 0;

    } else {
      // means no flow
      time_t difference = timeDifference(endFlowRateTimeTank1, timeNow);
      int minute = timeDifferenceInMinute(difference);
      Serial.print("The non water flow time difference in second is ");
      Serial.print(difference);

      Serial.print("The max no flowtime error counter is ");
      Serial.println(maxNoFlowTimeErrorCounterTank1, DEC);

      // int temp = (minute - maxNoFlowTimeTank1 * maxNoFlowTimeErrorCounterTank1);
      // Serial.print("The no water flow differece is ");
      // Serial.println(temp);
      if ((minute - maxNoFlowTimeTank1 * maxNoFlowTimeErrorCounterTank1) >= maxNoFlowTimeTank1) {
        // need to trigger an error state to send email
        updateStateStatus(&previousStateStatusTank1, &stateStatusTank1,2);
      } else {
        // let us continue waiting
        // this could mean
        // no error happen, maxNoFlowTimeErrorCounterTank1 ==0
        // or error already happened once, and we already send a email about it.
        ;
      }
    }
  } else if (stateStatusTank1 == 1) {
    if (sensorVal == 1) {
      // water is flowing;


      time_t difference = timeDifference(startFlowTimeTank1, timeNow);
      int min = timeDifferenceInMinute(difference);
      Serial.print("The water flow time difference in second is ");
      Serial.println(difference);

      Serial.print("The max flowtime error counter is ");
      Serial.println(maxFlowTimeErrorCounterTank1, DEC);
      // int temp = min - maxFlowTimeTank1 * maxFlowTimeErrorCounterTank1;
      // // Serial.print("The water flow difference is ");
      // // Serial.println(temp, DEC);
      if ((min - maxFlowTimeTank1 * maxFlowTimeErrorCounterTank1) >= maxFlowTimeTank1) {
        // trigger into error state
        updateStateStatus(&previousStateStatusTank1, &stateStatusTank1,2);
      } else {
        // continue waiting
        // means either error alread happened and already send out a email about it,  maxFlowTimeErrorCounterTank1>1, or no error happened yet.
      }
    } else {
      // means need to switch to state 0
      updateStateStatus(&previousStateStatusTank1, &stateStatusTank1,0);
      maxFlowTimeErrorCounterTank1 = 0;
      maxNoFlowTimeErrorCounterTank1 = 0;
      endFlowRateTimeTank1 = timeNow;
    }
  } else {

    Serial.println("In error state function");

    if (previousStateStatusTank1 == 1) {
      Serial.print("Error for water is flowing: ");
      time_t diff = timeDifference(startFlowTimeTank1, timeNow);
      int minutes = timeDifferenceInMinute(diff);
      Serial.println(minutes, DEC);
      if (maxFlowTimeErrorCounterTank1 == 0) {
        bool status = sendEmailAlert("const water flow longer than expected time");
        if (status == false) {
          // means gmail did not send through
          // do not update status, comeback again
          maxFlowTimeErrorCounterTank1 = 0;  // means come back later
          maxNoFlowTimeErrorCounterTank1 = 0;
          Serial.println("Failed to send out constant waterflow email");

        } else {
          // means able to send warning email
          updateStateStatus(&previousStateStatusTank1, &stateStatusTank1,1);
          maxFlowTimeErrorCounterTank1++;
          maxNoFlowTimeErrorCounterTank1 = 0;
        }
      } else {
        // error email already send, do not send email, but update the rest.
        updateStateStatus(&previousStateStatusTank1, &stateStatusTank1,1);
        maxFlowTimeErrorCounterTank1++;
        maxNoFlowTimeErrorCounterTank1 = 0;
      }


    } else {
      Serial.println("Error for no water is flowing");
      time_t diff = timeDifference(endFlowRateTimeTank1, timeNow);
      int minutes = timeDifferenceInMinute(diff);
      Serial.println(minutes);
      //sendEmailAlert("No water flow longer than expected time");


      if (maxNoFlowTimeErrorCounterTank1 == 0) {
        bool status = sendEmailAlert("No water flow longer than expected time");
        if (status == false) {
          // means gmail did not send through
          // do not update status, comeback again
          maxFlowTimeErrorCounterTank1 = 0;  // means come back later
          maxNoFlowTimeErrorCounterTank1 = 0;
        } else {
          // means able to send warning email
          updateStateStatus(&previousStateStatusTank1, &stateStatusTank1,0);
          maxFlowTimeErrorCounterTank1 = 0;
          maxNoFlowTimeErrorCounterTank1++;
        }
      } else {
        // error email already send, do not send email, but update the rest
        updateStateStatus(&previousStateStatusTank1, &stateStatusTank1,0);
        maxFlowTimeErrorCounterTank1 = 0;
        maxNoFlowTimeErrorCounterTank1++;
      }
    }
  }

  controlLEDBaseOnStatus(stateStatusTank1, previousStateStatusTank1,ERROR_LED_TANK_1, FLOW_PRESENT_TANK_1, NO_FLOW_PRESENT_TANK_1);

  delay(1000);
  timeCounter++;
}

time_t readTimeCounter(){
  return timeCounter;
}

void reconnectToWifi(){
  digitalWrite(LED_BUILTIN,LOW);
  WiFi.end();
  listNetworks();
  //_connectToWifi(ssid, pass);
  __connectToWifiEnterprise(ssid, username, pass); //while loop that connect to wifi
  // reconnect to wifi
  digitalWrite(LED_BUILTIN, HIGH);
  sendEmailAlert("Experience wifi signal lost");
}
void controlLEDBaseOnStatus(char stateStatus, char previousStateStatus, char errorLEDPin, char flowPresentLEDPin, char noFlowPresentLEDPin) {


  // turn all led off first


  digitalWrite(errorLEDPin, LOW);
  digitalWrite(flowPresentLEDPin, LOW);
  digitalWrite(noFlowPresentLEDPin, LOW);

  if (stateStatus == 2 || previousStateStatus == 2) {
    digitalWrite(errorLEDPin, HIGH);
    if (stateStatus == 0 || previousStateStatus == 0) {
      digitalWrite(noFlowPresentLEDPin, HIGH);
    }
    if (stateStatus == 1 || previousStateStatus == 1) {
      digitalWrite(flowPresentLEDPin, HIGH);
    }
  }else{
    // means it is normal, no red light
    if(stateStatus == 0){
      digitalWrite(noFlowPresentLEDPin, HIGH);
    }else if(stateStatus == 1){
       digitalWrite(flowPresentLEDPin, HIGH);     
    }else{
      // means at 100(start), do nothing
    }
  }
}




int detectFlowSensor1(){
  return  digitalRead(SENSOR_PIN_TANK_1);
}

void updateStateStatus(volatile char *previousStatus, volatile char *stateStatus,  char newStatus) {

  *previousStatus = *stateStatus;
  *stateStatus = newStatus;
}




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



