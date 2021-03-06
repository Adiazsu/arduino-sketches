/**************************************************************
This is a simple command station that receive the commands via ethernet.
It base on the protocol from the Z21 of Roco!

- DCC Signal with Timer 2 by modifired CmdrArduino library (Philipp Gahtow)
- Z21 LAN Protokoll mit W5100 Ethernet Shield with z21.h library
- LAN HTTP Website on Port 80 to Configure IP & S88
- ESP8266 WiFi Z21 LAN Untersützung with z21.h library
- fast S88 Bus with Timer 3 on MEGA
- DCC input, to read signal from other Central Station via Interrupt 0 and Timer 4
- LocoNet at MEGA with Timer 5, normal Timer1 with Loconet.h library
- XpressNet via LOOP-Function with XpressNetMaster.h library
- Kehrschleifen output for ROCO Booster 

by Philipp Gahtow, year 2015

Change log Version 4.0:
15.04.2015  Abschaltung S88 Interface per Define (S88N)
16.04.2015  Aktualisierung Z21 LAN Protokoll V1.05 & Firmware-Version 1.26
17.04.2015  LN OPC_INPUT_REP msg von Belegmeldern über LAN_LOCONET_DETECTOR
20.04.2015  kurze/Lange DCC Adressen (1..99 kurz, ab 100 lang)
22.04.2015  Add in DCC Lib Function support F13 - F28
            Add Power Button with Reset (press at startup)
23.04.2015  Add LocoNet set Railpower (OPC_GPOFF, OPC_GPON, OPC_IDLE)
            Add LocoNet Slot Write (OPC_WR_SL_DATA)
            New Broadcast Msg (8 Bit) Z21 Protokoll V1.05 (Include LocoNet)
            Add LocoNet OPC_RQ_SL_DATA, OPC_UHLI_FUN, OPC_SW_REQ, OPC_SW_REP, OPC_SW_ACK, OPC_SW_STATE
28.04.2015  Add DCC CV Write and Decoder Reset Packet before CV-Programming            
04.07.2015  Add Support Sanguino (ATmega644p and ATmega1284p) =>MCU_config.h
10.07.2015  Change Timer for DCC Interface and S88 to support LocoNet for all MCU
            Add second Booster support (intenal/external)
21.07.2015  S88 max Module define hinzu und S88 HTTP Ausgabe angepasst
30.07.2015  Versionsnummer für Debug definiert
02.08.2015  DCC Accessory Befehl korrigiert
            PowerButton Input geändert von Pin 51 nach Pin 47
03.08.2015  DCC Decoder Funktion korrigiert
17.09.2015  S88 Timer Auswahl (MEGA = Timer3)
18.09.2015  ESP8266 WiFi Support; Z21 LAN über externe Library
23.09.2015  Überarbeitung LAN_LOCONET_DETECTOR
            Neues Kommando OPC_MULTI_SENSE
            DCC Dekoder ohne Timer4!
            Optionale Lok-Event-Informationen im LocoNet (reduzierung der Sendedaten)
03.10.2015  S88 verbessert -> Fehler in der S88 Modulanzahl korrigiert (Überlauf der Zählervariale)       
            LocoNet TX/RX Packetverarbeitung verbessert     
----
toDo:
-> Rückmelder via XpressNet
-> Rückmelder via LocoNet
-> Programmieren von CVs im LocoNet
-> store loco adr use via per ip (max 16?)
***************************************************************/
#define Z21mobileVERSIONMSB 4
#define Z21mobileVERSIONLSB 0
/*---------------------------------------------------------------
Command Station Config:
=> uncomment the following lines, if you not need the protokoll
*/
//**************************************************************
#define Debug Serial  //Interface for Debugging
#define DEBUG    //To see DATA on Serial
#define LnDEB    //To see HEX DATA of LocoNet Protokoll
#define Z21DEBUG //to see DATA of Z21 LAN Protokoll

//**************************************************************
//Website to configure IP Address and Number of S88 Bus Module
#define HTTPCONF

//**************************************************************
//Singel S88 Bus Interface (max 62 * 8 Module)
#define S88N

//**************************************************************
//WiFi ESP 8266 Z21 LAN Komunikation via Serial
#define WIFI

//**************************************************************
//LAN W5100 Ethernet Z21 LAN Kommunikation
#define LAN

//**************************************************************
//DCC Decoder (only for MEGA, To decode a DCC-Signal, add this data to command station DCC output)
#define DECODER

//**************************************************************
//XpressNet Master Interface
#define XPRESSNET  
#include <XpressNetMaster.h>

//**************************************************************
//LocoNet Master Interface (Timer1, Timer5 on MEGA)
#define LOCONET  
#include <LocoNet.h>
#define TXAllLokInfoOnLN false    //sende alle Lok-Ereignisse ins LocoNet

//**************************************************************
//Booster external: (zB. ROCO)
#define BOOSTER_EXT
#define BOOSTER_EXT_ON HIGH
#define BOOSTER_EXT_OFF LOW

//**************************************************************
//Booster internal: (zB. TLE5205)
#define BOOSTER_INT
#define BOOSTER_INT_ON LOW
#define BOOSTER_INT_OFF HIGH

//**************************************************************
//Setup up PIN-Configuration for different MCU
#include "MCU_config.h"

//---------------------------------------------------------------
//DCC Master to create a DCC Signal:
#include <DCCPacketScheduler.h>

//Z21 LAN Protokoll:
#include <z21.h> 
z21Class z21;

#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library

#include <EEPROM.h> //EEPROM

#define EES88Moduls 38  //Adresse EEPROM Anzahl der Module für S88
#define EEip 40    //Startddress im EEPROM für die IP

#if defined(LAN)
EthernetUDP Udp;    //UDP for communication with APP/Computer (Port 21105)
//EthernetUDP UdpMT;  //UDP to Z21 Maintenance Tool (Port 34472)
//---------------------------------------------------------------
// The IP address will be dependent on your local network:
// Die MAC Adresse der Z21 beginnt mit „84:2B:BC:..:..:..“!
byte mac[] = { 0x84, 0x2B, 0xBC, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 111);   //Werkseinstellung ist: 192.168.0.111

//---------------------------------------------------------------
#if defined(HTTPCONF)
EthernetServer server(80);  // (port 80 is default for HTTP):
#endif
#endif    //LAN end
//--------------------------------------------------------------
//Z21 Protokoll Typ Spezifikationen
#include "Z21type.h"

//--------------------------------------------------------------
//S88 Singel Bus:
#if defined(S88N)
#include "S88.h"
#endif

//--------------------------------------------------------------
//DCC Master:
#define DetectShortCircuit 0xFF    //to detect short circuit
#define KSRelaisShortCircuit 0x1F  //to detect KS short
byte ShortTime = 0;            //Time Count for Short Detect
unsigned int LEDcount = 0;    //Timer for Status LED

DCCPacketScheduler dcc;

//--------------------------------------------------------------
// certain global XPressnet status indicators
#define csNormal 0x00 // Normal Operation Resumed ist eingeschaltet
#define csEmergencyStop 0x01 // Der Nothalt ist eingeschaltet
#define csTrackVoltageOff 0x02 // Die Gleisspannung ist abgeschaltet
#define csShortCircuit 0x04 // Kurzschluss
#define csServiceMode 0x08 // Der Programmiermodus ist aktiv - Service Mode
byte Railpower = csTrackVoltageOff;
bool Z21ButtonLastState = false;

//--------------------------------------------------------------
//DCC Decoder:
#if defined(DECODER)
#include "DCCDecoder.h"
#endif

//--------------------------------------------------------------
//XpressNet-Bus:
#if defined(XPRESSNET)
#include "XBusInterface.h"
#endif

//--------------------------------------------------------------
//LocoNet-Bus:
#if defined (LOCONET)
#include "LNInterface.h"
#endif

//--------------------------------------------------------------
//Z21 Ethernet:
#include "Z21_LAN.h"

//--------------------------------------------------------------
void setup() {
  pinMode(DCCLed, OUTPUT);      //DCC Status LED
  digitalWrite(DCCLed, LOW);    //DCC LED is in "off" State
  pinMode(ShortLed, OUTPUT);    //Short Status LED
  digitalWrite(ShortLed, LOW);    //Short LED is in "off" State
  pinMode(KSPin, OUTPUT);       //Kehrschleife-Relais
  digitalWrite(KSPin, LOW);     //Kehrschleife
  #if defined(BOOSTER_EXT)    //Booster (ROCO) external: 
    pinMode(ShortExtPin, INPUT);	//set short pin
    digitalWrite(ShortExtPin, HIGH); //Turn on internal Pull-Up Resistor
    pinMode(GoExtPin, OUTPUT);      //GO/STOP Signal
    digitalWrite(GoExtPin, BOOSTER_EXT_OFF);    //set STOP to Booster
  #endif
  #if defined(BOOSTER_INT)    //Booster2 internal:
    pinMode(GoIntPin, OUTPUT);    //GO/STOP2 Signal
    digitalWrite(GoIntPin, BOOSTER_INT_OFF);   //set STOP to Booster2 invertet
    pinMode(ShortIntPin, INPUT);  //set up short2 PIN
    digitalWrite(ShortIntPin, HIGH); //Turn on internal Pull-Up Resistor
  #endif
  pinMode(Z21ResetPin, INPUT);
  digitalWrite(Z21ResetPin, HIGH); //Turn on internal Pull-Up Resistor
  pinMode(Z21ButtonPin, INPUT);
  digitalWrite(Z21ButtonPin, HIGH); //Turn on internal Pull-Up Resistor

  #if defined(DEBUG)
    Debug.begin(115200);
    Debug.print("Z21 ");
    Debug.print(Z21mobileVERSIONMSB);
    Debug.print(".");
    Debug.print(Z21mobileVERSIONLSB);
    #if defined(UNO_MCU)
    Debug.println(" - UNO");
    #elif defined(MEGA_MCU)
    Debug.println(" - MEGA");
    #elif defined(SANGUINO_MCU)
    Debug.println(" - SANGUINO");
    #endif
  #endif

  #if defined(LAN)
  if ((digitalRead(Z21ButtonPin) == LOW) || (EEPROM.read(EEip) == 255)) {
      #if defined(DEBUG)
        Debug.println("RESET"); 
      #endif  
      EEPROM.write(EEip, ip[0]);
      EEPROM.write(EEip+1, ip[1]);
      EEPROM.write(EEip+2, ip[2]);
      EEPROM.write(EEip+3, ip[3]);
  }
  
  ip[0] = EEPROM.read(EEip);
  ip[1] = EEPROM.read(EEip+1);
  ip[2] = EEPROM.read(EEip+2);
  ip[3] = EEPROM.read(EEip+3);
  #endif

  dcc.setup(DCCPin);
  
  #if defined(S88N)
    SetupS88();    //S88 Setup  
  #endif  

  #if defined(DEBUG)
    #if defined(LAN)
    Debug.println(ip);
    #endif
    #if defined(S88N)
      Debug.print("S88: ");
      Debug.println(S88Module);
    #endif
    Debug.print("RAM: ");
    Debug.println(freeRam());  
  #endif  

  #if defined(XPRESSNET)  
    XpressNet.setup(XNetFahrstufen, XNetTxRxPin);    //Initialisierung XNet Serial und Send/Receive-PIN  
  #endif

  #if defined(DECODER)
    DCCDecoder_init();    //DCC Decoder init
  #endif
  
  #if defined(LOCONET)
    LNsetup();      //LocoNet Interface init
  #endif

  #if defined(WIFI)
    WLAN.begin(115200);  //UDP to Serial Kommunikation
  #endif 

  #if defined(LAN)
    // start the Ethernet and UDP:
    delay(100); //wait for ethernet to get up
    Ethernet.begin(mac,ip);  //IP and MAC Festlegung
    Udp.begin(z21Port);  //UDP Z21 Port 21105

  //UdpMT.begin(34472);   //UDP Maintenance Tool
  //0x30 0x80 0x01 0x02

    #if defined(HTTPCONF)
      server.begin();    //HTTP Server
    #endif
  #endif
  
  setPower(Railpower); //send Power state to all Devices!
}

//--------------------------------------------------------------------------------------------
void loop() {
  
  updateLedButton();     //DCC Status LED and Button
  
  ShortDetection();  //handel short on rail => power off
  dcc.update();
  
  #if defined(HTTPCONF) && defined(LAN)
    Webconfig();    //Webserver for Configuration
  #endif
  
  #if defined(S88N)
    notifyS88Data();    //R-Bus geänderte Daten 1. Melden
  #endif  
  
  #if defined(DECODER)
    DCCDecoder_update();    //Daten vom DCC Decoder
  #endif
  
  #if defined(XPRESSNET)  
    XpressNet.update(); //XpressNet Update
  #endif
  
  #if defined(LOCONET)
    LNupdate();      //LocoNet update
  #endif
  
  #if defined(S88N)
    notifyS88Data();    //R-Bus geänderte Daten 2. Melden
  #endif

  #if defined(LAN) || defined(WIFI)
    Z21LANreceive();   //Z21 LAN Dekoding
  #endif
}

//--------------------------------------------------------------------------------------------
//POWER Configuration:
void setPower (byte state) {
  Railpower = state;
  Serial.print("Power: ");
  Serial.println(state);
  switch (state) {
    case csNormal: 
      dcc.setpower(ON);
      #if defined(BOOSTER_EXT)
      digitalWrite(GoExtPin, BOOSTER_EXT_ON);
      #endif
      #if defined(BOOSTER_INT)
      digitalWrite(GoIntPin, BOOSTER_INT_ON);
      #endif
    break;
    case csTrackVoltageOff: 
      dcc.setpower(OFF);
      #if defined(BOOSTER_EXT)
      digitalWrite(GoExtPin, BOOSTER_EXT_OFF);
      #endif
      #if defined(BOOSTER_INT)
      digitalWrite(GoIntPin, BOOSTER_INT_OFF);
      #endif
    break;
    case csServiceMode: 
      dcc.setpower(ON);
      #if defined(BOOSTER_EXT)
      digitalWrite(GoExtPin, BOOSTER_EXT_ON);
      #endif
      #if defined(BOOSTER_INT)
      digitalWrite(GoIntPin, BOOSTER_INT_ON);
      #endif
    break;
    case csShortCircuit: 
      dcc.setpower(ON);  //shut down via GO/STOP just for the Roco Booster
      #if defined(BOOSTER_EXT)
      digitalWrite(GoExtPin, BOOSTER_EXT_OFF);
      #endif
      #if defined(BOOSTER_INT)
      digitalWrite(GoIntPin, BOOSTER_INT_OFF);
      #endif
    break;
    case csEmergencyStop:
      dcc.eStop();  
    break;
  }
  if (Railpower == csShortCircuit)
    digitalWrite(ShortLed, HIGH);   //Short LED show State
  else digitalWrite(ShortLed, LOW);   //Short LED show State  
  #if defined(LAN) || defined(WIFI)
  z21.setPower(Railpower);
  #endif
  #if defined(XPRESSNET)
  XpressNet.setPower(Railpower);  //send to XpressNet
  #endif
  #if defined(LOCONET)
  LNsetpower(); //send to LocoNet
  #endif
}

//--------------------------------------------------------------------------------------------
void ShortDetection() { 
  //Short Circuit?
  //Check BOOSTER extern
  #if defined(BOOSTER_EXT)
  if (digitalRead(ShortExtPin) == LOW && Railpower != csShortCircuit) {  
    ShortTime++;
    if(ShortTime == DetectShortCircuit) {
        setPower(csTrackVoltageOff);
        setPower(csShortCircuit);
        #if defined(DEBUG)
        Debug.println("TRACK_SHORT_CIRCUIT EXT");
        #endif
    }
    //Before Railpower cut out test change polarity:
    else if (ShortTime == KSRelaisShortCircuit) {   
      digitalWrite(KSPin, !digitalRead(KSPin));     //Kehrschleife
      #if defined(DEBUG)
      Debug.print("KS ");
      Debug.println( digitalRead(KSPin) );
      #endif
    }
  }
  else ShortTime = 0;
  #endif;
  //Check BOOSTER2 (z.B. TLE5205)
  #if defined(BOOSTER_INT)
  if (digitalRead(ShortIntPin) == LOW && Railpower != csShortCircuit) {
    setPower(csTrackVoltageOff);
    setPower(csShortCircuit);
    #if defined(DEBUG)
    Debug.println("TRACK_SHORT_CIRCUIT INT");
    #endif
  }
  #endif
}

//--------------------------------------------------------------------------------------------
void updateLedButton() {
  //Button to control Railpower state
  if ((digitalRead(Z21ButtonPin) == LOW) && (Z21ButtonLastState == false)) {  //Button DOWN
    Z21ButtonLastState = true;
    LEDcount = 1;
  }
  else {
    if ((digitalRead(Z21ButtonPin) == HIGH) && (Z21ButtonLastState == true)) {  //Button UP
      Z21ButtonLastState = false;
      if (LEDcount > 0x0300)  //push long?
        setPower(csTrackVoltageOff); 
      else {
        if (Railpower == csNormal)
          setPower(csEmergencyStop);
        else setPower(csNormal);
      }
      LEDcount = 0;
      #if defined(DEBUG)
         Debug.print("BT Power ");
         Debug.println(Railpower);
      #endif
    }
  }
  //Update LED  
  if (LEDcount == 0) {  //flash
    LEDcount = 0xFEFF;  //fast = csShortCircuit
    if (Railpower == csTrackVoltageOff) {
      if (digitalRead(DCCLed) == HIGH)
        LEDcount = 0xF080;    //long OFF
      else LEDcount = 0xFA00;  //short ON
    }
    if (Railpower == csEmergencyStop) {
      if (digitalRead(DCCLed) == HIGH)
        LEDcount = 0xFF00;    //short OFF
      else LEDcount = 0xF080;  //long ON
    }  
    if (Railpower == csNormal)
      digitalWrite(DCCLed, HIGH);
    else digitalWrite(DCCLed, !digitalRead(DCCLed));
  }
  LEDcount++;
}

//--------------------------------------------------------------------------------------------
#if defined(HTTPCONF) && defined(LAN)
void Webconfig() {
  EthernetClient client = server.available();
  if (client) {
    String receivedText = String(50);
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (receivedText.length() < 50) {
          receivedText += c;
        }
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          //client.println("Connection: close");  // the connection will be closed after completion of the response
          //client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          //Website:
          client.println("<html><title>Z21</title><body><h1>Z21</h1>");
          //----------------------------------------------------------------------------------------------------          
          int firstPos = receivedText.indexOf("?");
          if (firstPos > -1) {
            byte lastPos = receivedText.indexOf(" ", firstPos);
            String theText = receivedText.substring(firstPos+3, lastPos); // 10 is the length of "?A="
            byte S88Pos = theText.indexOf("&S88=");
            #if defined(S88N)
              S88Module = theText.substring(S88Pos+5, theText.length()).toInt();
            #endif  
            byte Aip = theText.indexOf("&B=");
            byte Bip = theText.indexOf("&C=", Aip);
            byte Cip = theText.indexOf("&D=", Bip);
            byte Dip = theText.substring(Cip+3, S88Pos).toInt();
            Cip = theText.substring(Bip+3, Cip).toInt();
            Bip = theText.substring(Aip+3, Bip).toInt();
            Aip = theText.substring(0, Aip).toInt();
            ip[0] = Aip;
            ip[1] = Bip;
            ip[2] = Cip;
            ip[3] = Dip;
            client.println("-> RESET Z21");
            #if defined(S88N)
            if (EEPROM.read(EES88Moduls) != S88Module) {
              EEPROM.write(EES88Moduls, S88Module);
              SetupS88();
            }
            #endif
            if (EEPROM.read(EEip) != Aip)  
              EEPROM.write(EEip, Aip);
            if (EEPROM.read(EEip+1) != Bip)  
              EEPROM.write(EEip+1, Bip);
            if (EEPROM.read(EEip+2) != Cip)  
              EEPROM.write(EEip+2, Cip);
            if (EEPROM.read(EEip+3) != Dip)  
              EEPROM.write(EEip+3, Dip);
          }
          //----------------------------------------------------------------------------------------------------          
          client.print("<form method=get>IP:<input type=number min=0 max=254 name=A value=");
          client.println(ip[0]);
          client.print("><input type=number min=0 max=254 name=B value=");
          client.println(ip[1]);
          client.print("><input type=number min=0 max=254 name=C value=");
          client.println(ip[2]);
          client.print("><input type=number min=0 max=254 name=D value=");
          client.println(ip[3]);
          client.print("><br/>8x S88:<input type=number min=0 max=");
          #if defined(S88N)
          client.print(S88MAXMODULE);
          #else
          client.print("0");
          #endif
          client.print(" name=S88 value=");
          #if defined(S88N)
            client.print(S88Module);
          #else
            client.print("-");
          #endif
          client.println("><br/><input type=submit></form></body></html>");
          break;
        }
        if (c == '\n') 
          currentLineIsBlank = true; // you're starting a new line
        else if (c != '\r') 
          currentLineIsBlank = false; // you've gotten a character on the current line
      }
    }
    client.stop();  // close the connection:
  }
}
#endif

//--------------------------------------------------------------
void notifyLokAll(uint16_t Adr, uint8_t Steps, uint8_t Speed, uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3, bool bc)
{     
  #if defined(XPRESSNET)  
  if (XNetReturnLocoInfo == true) {
    XNetReturnLocoInfo = false;
    XpressNet.SetLocoInfo(XNetUserOps, Speed, F0, F1); //UserOps,Speed,F0,F1
    //return;
  }
  if (XNetReturnLocoFkt == true) {
    XNetReturnLocoFkt = false;
    XpressNet.SetFktStatus(XNetUserOps, F2, F3); //Fkt4, Fkt5
    //return;
  }
  #endif
  
  #if defined(LOCONET)
    LNSetLocoStatus(Adr, Speed, F0, F1);
  #endif

  z21.setLocoStateFull (Adr, Steps,Speed, F0, F1, F2, F3, bc);
}
//-------------------------------------------------------------- 
void notifyTrnt(uint16_t Adr, bool State) 
{
  z21.setTrntInfo(Adr, State);
  #if defined(DEBUG)
  Debug.print("DCC Trnt ");
  Debug.print(Adr);
  Debug.print("-");
  Debug.println(State);
  #endif
}

//--------------------------------------------------------------------------------------------
#if defined(DEBUG)
int freeRam () 
{
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#endif

