# Arduino WiFiNina Gmail SMTP

A working program that can send gmail via smtp using the Arduino SAMD WifiNina library.

Credited here:
- Arturo Guadalupi for his Base64 library which I updated: https://github.com/agdl/Base64
- Ralph Bacon for providing the basic file structure https://github.com/RalphBacon/Secure-Gmail
- www.samlogic.net for providng smtp reference documentation: https://www.samlogic.net/articles/smtp-commands-reference.htm

I give my most **sincere credit and respect to all the Arduino creators** and the great community around it!

# How can we connect to the gmail smtp server and send an email using the WifiNina library?

The answer is not as hard as one might expect, given the amount of people who asked this and never got a reply.

All my work is done on an Arduino Wifi 1010, but I assume it will work on any of the SAMD based Arduino boards.

Here's how I did it:
1. Arduino Preparation:
    - use the WifiNina firmware and certificate updater to update your firmware.    
    - use the lower part of the firmware tool window to update your certificates; be sure that google.com:443 is in the list.
    - My list:
        - arduino.cc:443
        - google.com:443
        - microsoft.com:443
        - github.com:443 
2. Gmail preparation:
    - find the gmail security settings page:  https://myaccount.google.com/security
    - enable 2 factor identification
    - create an app password for this application. The password is a 16 character string, copy it  and keep it handy!

3. Do you understand smtp? If not, read about it [here](https://www.samlogic.net/articles/smtp-commands-reference.htm) I cannot explain everything that I hardly understand myself.
4. Have you cloned this repo yet? If not do it.
5. Update the file arduino_secrets.h with
    - your wifi network SSID
    - your wifi network PASWWORD
    - your gmail account
    - your gmail account app specific password that we got in step 2. above

6. Update the email fields appropriately
    - the file `WiFiSSLClient_gmail_smtp.ino` contains a bunch of fields that need updating. They are easily found and updated.

7. Upload to a Wifi equipped SAMD board (tested on Arduino Wifi 1010) and observe the serial monitor at 115200 Baud.
8. Type a character and look at the output. It should be similar to that in the file `SMTP_Dialogue.h`

Good luck!
 