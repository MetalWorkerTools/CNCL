/*
************************************************************************************************************** 
************************************ ESP8266Station **********************************************************
*************************** Transparent WiFI <==> UART bridge ************************************************ 
************************************************************************************************************** 

  Copyright (c) 2018 MetalWorkerTools (MWT) All rights reserved.

  This file is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This file is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
**************************************************************************************************************
************************************* How to use *************************************************************
**************************************************************************************************************
Use this sketch to connect an ESP8266 to an arduino (or any other serial board/port).
This sketch echos all communication from the ESP8266 WiFi connection to the ESP8266 serial connection and back.
Connections can be made based on the given Host Name or by the received IP-address. (MDNS LLMNR supported)
The IP-Address can also be set manual
Enable tracing and activate the serial monitor to see debug and connection information
**************************************************************************************************************
************************************** How to compile ********************************************************
**************************************************************************************************************
To compile you first need to install the ESP8266 library for arduino.
Follow the steps in this link to install the library
https://arduino-esp8266.readthedocs.io/en/latest/installing.html
**************************************************************************************************************
***********************************  Test information  *******************************************************
**************************************************************************************************************
This software is tested on 2 boards with 2 applications on June 12 2018

Arduino IDE version:   1.8.5
ESP8266WIFI library version 1.0.0

Board:  (Arduino R3 with build in wifi) from https://robotdyn.com/ 
  UNO WiFi R3 ATmega328P ESP8266, 32Mb flash, USB-TTL CH340G, Micro-USB
Board: (Arduino R3 with separate WiFi)
  Arduino R3 with ESP-01 module
  
Software: (GRBL CNC lathe software) https://www.microsoft.com/store/apps/9p42tb5t697h
  CNCL version 2.3.2.0
Software: (Grbl-Panel) http://github.com/Gerritv/Grbl-Panel/wiki
  Grbl-Panel version 1.0.9.17  
**************************************************************************************************************
**************************************************************************************************************
**************************************************************************************************************
*/
#include <ESP8266WiFi.h>
#include <ESP8266LLMNR.h>
#include <ESP8266mDNS.h>

//**************************************************************************************************************
//****************************  These settings can/must be changed   *******************************************
//**************************************************************************************************************

//Remove the two slashes from the next line to trace the connection status for debugging/monitoring on serial monitor
#define Trace

////Set your WiFi router SSID and Password, the host name and the port to listen
#define ssid "YourSSID"                //ssid from the router to connect this bridge to
#define Password "YourPasswordr"       //password on the router to login
#define ServerPort 23                  //port at wich this brigde listens (23 = telnet port)
#define HostName"YourHostname"         //the host name of this bridge, you can access this bridge by this name

//Remove the two slashes from the next line to disable DNS and use Static IP
//#define StaticIP
//Fill in for your own IP-address, GateWay and SubnetMask when using Static IP
IPAddress IPaddress(192, 168, 10, 105);
IPAddress Gateway(192, 168, 10, 1);
IPAddress SubnetMask(255, 255, 255, 0);

//**************************************************************************************************************
//****************************  End of settings that can/must be changed   *************************************
//**************************************************************************************************************

WiFiServer server(ServerPort);
WiFiClient serverClient;

void CheckSettingMismatch(String Setting, String SettingRequested, String SettingReceived)
{
  if (SettingRequested != SettingReceived)
    Serial.printf("%s mismatch requested <%s> received: <%s> \n", Setting.c_str() , SettingRequested.c_str(), SettingReceived.c_str());
}
String ConnectionStatus(byte Status)
{
  switch (Status)
  {
    case WL_CONNECTED: return "Connected";
    case WL_NO_SHIELD: return "NoShield";
    case WL_IDLE_STATUS: return "Idle";
    case WL_NO_SSID_AVAIL: return "no SSID available";
    case WL_SCAN_COMPLETED: return "network scan completed";
    case WL_CONNECT_FAILED: return "connection failed";
    case WL_CONNECTION_LOST: return "connectionlost";
    case WL_DISCONNECTED: return "disconnected"; 
  }
}
void SerialPrint(bool PrintLine,String Line)
{
  #ifdef Trace
     Serial.print(Line);
     if (PrintLine) Serial.println();
  #endif
}
void Show(String Info,byte *Data, int Size)
{
  Serial.printf("%s: ",Info.c_str());
  for (byte i=0;i<Size;i++)
   Serial.printf("<%u> ",Data[i]);
   Serial.println();
}

void setup()
{
  Serial.begin(115200);
  #ifdef Trace
     Serial.println("Setup ESP8266");
  #endif
  //WiFi.printDiag(Serial);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);        //only save settings to flash when they are different
  WiFi.hostname(HostName);
  WiFi.begin(ssid,Password);
  WiFi.setAutoReconnect(true);  //when connection is lost, module reconnects automatically
  #ifdef Trace
    //Serial.printf("Connecting to: <%s> HostName: <%s> Port: <%s> \n" ,ssid,HostName,ServerPort);
  #endif
  #ifdef StaticIP
    #ifdef Trace
      Serial.printf("Using static IP: <%s> Subnet Mask: <%s> Gateway: <%s> \n", IPaddress.toString().c_str(), SubnetMask.toString().c_str(), Gateway.toString().c_str());
    #endif 
    WiFi.config(IPaddress, Gateway, SubnetMask);
  #endif
//  #ifdef Trace
//    Serial.println("Reconnecting using previous connection information");
//  #endif
//  WiFi.reconnect();
  while (WiFi.status() != WL_CONNECTED)
  {
    SerialPrint(true,ConnectionStatus(WiFi.status()));
    delay(500);
  }
  //start server
  server.begin();
  server.setNoDelay(true);    //allow small packages to be send whithout delay
  #ifdef Trace
    Serial.println();
    Serial.printf("Wifi Connected IP: <%s> Subnet Mask: <%s> Gateway: <%s> Hostname: <%s> \n", WiFi.localIP().toString().c_str(), WiFi.subnetMask().toString().c_str(), WiFi.gatewayIP().toString().c_str(), WiFi.hostname().c_str());
    CheckSettingMismatch("Hostname", HostName, WiFi.hostname());
    #ifdef StaticIP
      CheckSettingMismatch("IP-address", IPaddress.toString(), WiFi.localIP().toString());
      CheckSettingMismatch("Gateway", Gateway.toString(), WiFi.gatewayIP().toString());
      CheckSettingMismatch("Subnet Mask", SubnetMask.toString(), WiFi.subnetMask().toString());
    #endif
  #endif
  //start MDNS for connecting to Host using hostname:port
  if (!MDNS.begin(HostName))
    SerialPrint(true,"Error setting up MDNS!");
  else
    SerialPrint(true,"MDNS started"); 
  //start LLMNR for connecting to Host using hostname.local:port
  if (!LLMNR.begin(HostName)) 
    SerialPrint(true,"Error setting up LLMNR!");
  else
    SerialPrint(true,"LLMNR started");
  #ifdef Trace
     Serial.println("Setup Done");
  #endif
}
void loop()
{
  //check if there are any new clients
  if (server.hasClient())
  {
      if (!serverClient || !serverClient.connected())
      {
        if (serverClient) serverClient.stop();
        serverClient = server.available();
        #ifdef Trace
          Serial.print("New client:");
        #endif
      }
      else
      {
    //no free/disconnected client so reject
      WiFiClient rejectClient = server.available();
      rejectClient.stop();
      #ifdef Trace
        Serial.println("Connection rejected ");
      #endif
      }
  }
  //check clients for data
    if (serverClient && serverClient.connected())
    {
      if (serverClient.available())
      {
        //get data from the telnet client and push it to the UART
        while (serverClient.available())
        Serial.write(serverClient.read());
      }
    }
  //check UART for data
  if (Serial.available())
  {
    size_t len = Serial.available();
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);
    //push UART data to the client
      if (serverClient && serverClient.connected())
      {
        serverClient.write(sbuf, len);
        delay(1);
      }
  }
}
