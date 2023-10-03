#ifndef SMTP_DIALOGUE_H
#define SMTP_DIALOGUE_H

Enter a charcter to start...
Connecting to SSID: ***************
Connected to wifi
SSID: ***************
IP Address: 192.168.1.205
signal strength (RSSI):-51 dBm

Connecting to server: smtp.gmail.com:465
Connected to server
response: <start>220 smtp.gmail.com ESMTP q143sm3908750wme.28 - gsmtp
<end>
Sending Extended Hello: <start>EHLO yourDomain.org<end>
response: <start>250-smtp.gmail.com at your service, [78.218.194.4]
250-SIZE 35882577
250-8BITMIME
250-AUTH LOGIN PLAIN XOAUTH2 PLAIN-CLIENTTOKEN OAUTHBEARER XOAUTH
250-ENHANCEDSTATUSCODES
250-PIPELINING
250-CHUNKING
250 SMTPUTF8
<end>
Sending auth login: <start>AUTH LOGIN<end>
response: <start>334 VXNlcm5hbWU6
<end>
Sending account: <start>***************************<end>
response: <start>334 UGFzc3dvcmQ6
<end>
Sending Password: <start>*************************<end>
response: <start>235 2.7.0 Accepted
<end>
Sending From: <start>MAIL FROM: <somebody@gmail.com><end>
response: <start>250 2.1.0 OK q143sm3908750wme.28 - gsmtp
<end>
Sending To: <start>RCPT To: <somebodyelse@gmail.com><end>
response: <start>250 2.1.5 OK q143sm3908750wme.28 - gsmtp
<end>
Sending DATA: <start>DATA<end>
response: <start>354  Go ahead q143sm3908750wme.28 - gsmtp
<end>
Sending email
response: <start>250 2.0.0 OK  1607521808 q143sm3908750wme.28 - gsmtp
<end>
Sending QUIT
response: <start>221 2.0.0 closing connection q143sm3908750wme.28 - gsmtp
<end>
Done.

#endif
