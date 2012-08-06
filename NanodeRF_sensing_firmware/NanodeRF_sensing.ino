/*
 _   _                           _       ______ ______                                            _      _        _      _ 
| \ | |                         | |      | ___ \|  ___|                                          | |    (_)      | |    | |
|  \| |  __ _  _ __    ___    __| |  ___ | |_/ /| |_     ___   ___  _ __   ___   ___   _ __  ___ | |__   _   ___ | |  __| |
| . ` | / _` || '_ \  / _ \  / _` | / _ \|    / |  _|   / __| / _ \| '_ \ / __| / _ \ | '__|/ __|| '_ \ | | / _ \| | / _` |
| |\  || (_| || | | || (_) || (_| ||  __/| |\ \ | |     \__ \|  __/| | | |\__ \| (_) || |   \__ \| | | || ||  __/| || (_| |
\_| \_/ \__,_||_| |_| \___/  \__,_| \___|\_| \_|\_|     |___/ \___||_| |_||___/ \___/ |_|   |___/|_| |_||_| \___||_| \__,_|
*/ 
//--------------------------------------------------------------------------------------
// 
// Relay's data recieved from the local sensorshield to COSM and EMONCMS3
// Relay's temperature data recieved from emonGLCD 
// Decodes reply from COSM server to set software real time clock
// Relay's time data to emonglcd - and any other listening nodes.
// Looks for 'ok' reply from http request to verify data reached COSM

// emonBase Documentation: http://openenergymonitor.org/emon/emonbase

// Authors: Trystan Lea and Glyn Hudson
// COSM modification by Roger James

// Arduino hardware sensorshield designed by Martin Harizanov
// Sensorshield readings by Alco van Neck read http://www.bluemotica.nl/en/arduino-projects for details about this sensorshield build

// Part of the: openenergymonitor.org project
// Licenced under GNU GPL V3
//http://openenergymonitor.org/emon/license

// EtherCard Library by Jean-Claude Wippler and Andrew Lindsay
// JeeLib Library by Jean-Claude Wippler
//
// N.B. Thise version needs an edited version of the ethercard library (date: 2010-05-20) with tcp_client_state made non static in tcpip.cpp.
// alter the tcip.cpp file line 32 with an // command out [//static byte tcp_client_state;]
////--------------------------------------------------------------------------------------
// NO EMONGLCD OR RM12B clients at my setup..so didn't test that part!
//  Default setup with 1 CT-sensor, 1 Voltage AC-AC adapter , 3 DS18B20 temp.sensors, LDR and Hall-sensor (magnetic) are optional (command-out now)
//--------------------------------------------------------------------------------------

#define DEBUG           //comment out to disable serial printing to increase long term stability 
const int UNO = 0;      //anti crash wachdog reset only works with Uno (optiboot) bootloader, comment out the line if using delianuova

#include <JeeLib.h>	                                                //https://github.com/jcw/jeelib
#include <avr/wdt.h>                                                    // the UNO bootloader
#include "EmonLib.h"							// Openenergymonitor sleepfunctions for RFM12B
#include <OneWire.h>							// 1-WIRE communication
#include <DallasTemperature.h>						// DALLAS TEMP sensor DS18B20 lib.
#include <EtherCard.h>		                                        // https://github.com/jcw/ethercard 

#define MYNODE 15                                                       // sensTX RFM12B node ID: emontx = 10 emonbase = 15 emonglcd = 20 NanodeRF sensorshield = 30
#define freq RF12_868MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. SET YOUR FREQ RIGHT!
#define group 210                                                       // sensTX RFM12B wireless network group - needs to be same as emonBase, emontx and emonGLCD

// !!!!!!!!!!!!!!!!!! TODO - put the correct values in here !!!!!!!!!!!!!!!!!!!!!!
#define COSM_FEED_ID "FEEDID"                                          // like"64091" 
#define COSM_API_KEY "APIKEY"                                          // like "Y3xIpC7E622Db9fd03s2OWW4R7ySAKxDKzM1MzV3MzByST0g"
#define EMON_API_KEY "EMONAPI"                                         // like "06AAc3cd3337f02d62272f7b663d4a70d"
#define HTTP_TIMEOUT 10000                                             // Time before HTTP reply gets a time-out (default: 10 sec)

// website adresses are defined in dhcp_dns.ino
extern char website_cosm[];
extern char website_emon[];
extern byte cosmip[];
extern byte emonip[];

// defined in ethercard library
extern byte tcp_client_state;



//---------------------------------------------------
// Data structures for transfering data between units
//---------------------------------------------------

typedef struct { int realPower, apparentPower, voltage, Temp1, Temp2, Temp3; } PayloadTX;
PayloadTX sensTX;

typedef struct { int glcd_temperature; } PayloadGLCD;
PayloadGLCD emonglcd;

typedef struct { int hour, mins;} PayloadBase;
PayloadBase emonbase;


//---------------------------------------------------------------------
// The PacketBuffer class is used to generate the json string that is send via ethernet - JeeLabs
//---------------------------------------------------------------------
class PacketBuffer : public Print {
public:
    PacketBuffer () : fill (0) {}
    const char* buffer() { return buf; }
    byte length() { return fill; }
    void reset()
    { 
      memset(buf,NULL,sizeof(buf));
      fill = 0; 
    }
    virtual size_t write (uint8_t ch)
        { if (fill < sizeof buf) buf[fill++] = ch; }
    byte fill;
    char buf[150];
    private:
};
PacketBuffer cosm_str;
PacketBuffer emon_str;

//--------------------------------------------------------------------------
// Ethernet
//--------------------------------------------------------------------------

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0xF2,0x31,0x42,0x21,0x30,0x03 };              // SET YOUR OWN MAC ADRES (OR LEAVE THIS RANDOM DEFAULT)

//IP address of remote sever, only needed when posting to a server that has not got a dns domain name (staticIP e.g local server) 
byte Ethernet::buffer[700];
//static uint32_t timer;                 // general timer
static uint32_t Mtimer;                  // timer for sending packages to COSM and EMONCMS3

const int redLED = 6;                     // NanodeRF RED indicator LED
const int greenLED = 5;                   // NanodeRF GREEN indicator LED

int ethernet_error = 0;                   // Etherent (controller/DHCP) error flag
int rf_error = 0;                         // RF error flag - high when no data received 
int ethernet_requests = 0;                // count ethernet requests without reply                 
static boolean httpHaveReply;
static boolean httpTimeOut;
static MilliTimer httpReply;		  // Set to 1 
int emonglcd_rx = 0;                      // Used to indicate that emonglcd data is available
int data_ready=0;                         // Used to signal that sensTX data is ready to be sent
unsigned long last_rf;                    // Used to check for regular sensTX data - otherwise error

char line_buf[50];                        // Used to store line of http reply header


// ********************************************** SENSORS *****************************************************************************************
//Current sensors
//CT 1 is always enabled
const int CT2 = 0;                                                      // Set to 1 to enable CT channel 2
const int CT3 = 0;                                                      // Set to 1 to enable CT channel 3

EnergyMonitor ct1,ct2,ct3;                                              // Create  instances for each CT channel

// TEMPERATURE sensor (Dallas 1-wire)
#define ONE_WIRE_BUS 4							// Data wire is plugged into pin 4 on the Arduino
OneWire oneWire(ONE_WIRE_BUS);						// Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);					// Pass our oneWire reference to Dallas Temperature.

// Assign the addresses of your 1-Wire temp sensors.
// See the tutorial on how to obtain these addresses: http://www.hacktronics.com/Tutorials/arduino-1-wire-address-finder.html
DeviceAddress address_Temp1 = { 0x28, 0xD3, 0x7E, 0x11, 0x04, 0x00, 0x00, 0x65};
DeviceAddress address_Temp2 = {0x28, 0x2D, 0x94, 0x11, 0x04, 0x00, 0x00, 0x63};
DeviceAddress address_Temp3 = {0x28, 0xE3, 0x4F, 0x11, 0x04, 0x00, 0x00, 0xC1};

// MAGNETIC HALL sensor
//const int magPin = 7;                                                          // Magnetic sensor pin digital 7

// LDR (lux) sensor
//const int LDRpin = A4;			                                 // Light dependent resistor sensor pin analog 4


//**********************************************************************************************************************
// SETUP
//**********************************************************************************************************************
void setup () {
  
  //Nanode RF LED indictor  setup - green flashing means good - red on for a long time means bad! 
  //High means off since NanodeRF tri-state buffer inverts signal 
  pinMode(redLED, OUTPUT); digitalWrite(redLED,LOW);            
  pinMode(greenLED, OUTPUT); digitalWrite(greenLED,LOW);       
  delay(100); digitalWrite(greenLED,HIGH);                          // turn off greenLED
  
  Serial.begin(9600);
  Serial.println(F("\n[Emonbase COSM/EMON ALCO]"));

  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) {
    Serial.println(F("Failed to access Ethernet controller"));
    ethernet_error = 1;  
  }

  initialise_dhcp_dns();
  ethernet_requests = 0;
  ethernet_error=0;
  rf_error=0;
 
  //rf12_initialize(MYNODE, freq,group);
  last_rf = millis()-40000;                                // LastRadio received timer, setting lastRF back 40s is useful as it forces the ethernet code to run straight away
  
  //***** SENSOR SETUP *****

// Voltages sensor
ct1.voltageTX(260, 1.7);                                   // ct.voltageTX(calibration, phase_shift) - make sure to select correct calibration for AC-AC adapter (Mascot 9580 = 234.26)
ct2.voltageTX(234.26, 1.7);                                // for use with current sensor 2
ct3.voltageTX(234.26, 1.7);                                // for use with current sensor 3

// Setup  Current sensor channels and calibration)
ct1.currentTX(1, 60.60);                                  // current sensor 1 (near voltage input, up) 
ct2.currentTX(2, 74.07);                                  // current sensor 2 
ct3.currentTX(3, 60.60);                                  // current sensor 3 


/* CT Calibration factor = CT ratio / burden resistance
|---------------------------------------------------------|
|CT Calibration factor = 60.60  (100A / 0.05A) x 18 Ohms  |
|CT Calibration factor = 74.07  (100A / 0.05A) x 27 Ohms  |
|CT Calibration factor = 111,11 (100A / 0.05A) x 33 Ohms  |
|---------------------------------------------------------|
*/

delay(250);                                               // make sure setup sensors has ended
digitalWrite(redLED,HIGH);                                    // RED off - indicate that setup has finished 
 
  
  if (UNO) wdt_enable(WDTO_8S); 

  
//END SETUP
}

// ***************************************************************************************************************************
// =================== LOOP ======================================LOOP========================================LOOP============
// ***************************************************************************************************************************
 
void loop () {
  
 if (UNO) wdt_reset();
  dhcp_dns();                                              // handle dhcp and dns setup - see dhcp_dns tab
  
  // Display error states on status LED
  if (ethernet_error==1 || rf_error==1 || ethernet_requests > 0) digitalWrite(redLED,LOW);
    else digitalWrite(redLED,HIGH);

// ***************************************************************************************************************************
// === // read the sensors === 

ct1.calcVI(20,2000);                                       // Calculate all. No.of crossings, time-out 
sensTX.realPower         =   ct1.realPower;                //rename sensors to sensTX
sensTX.apparentPower     =   ct1.apparentPower;
sensTX.voltage           =   ct1.Vrms; 
//sensTX.LDR_reading       =   analogRead(LDRpin);

sensors.requestTemperatures();                                       // Send the command to get temperature
sensTX.Temp1             = sensors.getTempC(address_Temp1)*100;      // read sensor Temp1 and multiply with 100 for resolution of 2 decimals
sensTX.Temp2             = sensors.getTempC(address_Temp2)*100;      // read sensor Temp1 and multiply with 100 for resolution of 2 decimals
sensTX.Temp3             = sensors.getTempC(address_Temp3)*100;      // read sensor Temp1 and multiply with 100 for resolution of 2 decimals

// ***************************************************************************************************************************
 

if (millis() > Mtimer) {
    Mtimer = millis() + 30000;                  // set timer for how much datapulls have to be posted to COSM within X seconds (default: 30 sec)
    
          //last_rf = millis();                                            // reset lastRF timer
         

// PUT string to JSON , store it at the cosm_str buffer (remember the "" signs, and comma)
          cosm_str.reset();                                                                   // Reset PUT string  
          //cosm_str.println(F("rf_fail,0"));                                                  // RF received so no failure 
          cosm_str.print(F("power1,"));           cosm_str.println(sensTX.realPower);          // Add power reading
          cosm_str.print(F("voltage,"));          cosm_str.println(sensTX.voltage);            // Add AC-AC adapter voltage reading
          cosm_str.print(F("temp_AR1,"));         cosm_str.println(sensTX.Temp1/100.0);        // Arduino onboard temperature
          cosm_str.print(F("temp_WI,"));          cosm_str.println(sensTX.Temp2/100.0);        // tempsensor 2 temperature
          cosm_str.print(F("temp_BM,"));          cosm_str.println(sensTX.Temp3/100.0);        // tempsensor 3 temperature

          // JSON data for EmonCMS (remember the start { sign, and ending with } \0 )
          emon_str.reset();                                                                   // Reset PUT string  
          emon_str.print(F("{rf_fail:0"));                                                    // RF received so no failure
          emon_str.print(F(",power1:"));           emon_str.print(sensTX.realPower);          // Add power reading
          emon_str.print(F(",voltage:"));          emon_str.print(sensTX.voltage);            // Add AC-AC adapter voltage reading
          emon_str.print(F(",temp_AR1:"));         emon_str.print(sensTX.Temp1/100.0);        // Arduino temperature
          emon_str.print(F(",temp_WI:"));          emon_str.print(sensTX.Temp2/100.0);        //waterinlet
          emon_str.print(F(",temp_BM:"));          emon_str.print(sensTX.Temp3/100.0);        // basement

          data_ready = 1;                                                // data is ready
              
      
   

  //-----------------------------------------------------------------------------------------------------------------
  // 2) If no data is recieved from rf12 module the server is updated every 30s with RFfail = 1 indicator for debugging
  //-----------------------------------------------------------------------------------------------------------------
 /* if ((millis()-last_rf)>30000)
  {
    last_rf = millis();                                                 // reset lastRF timer
    cosm_str.reset();                                                   // reset csv string
    cosm_str.println(F("rf_fail,1"));                                      // No RF received in 30 seconds so send failure 
    emon_str.reset();                                                   // reset csv string
    emon_str.print(F("{rf_fail:1"));                                      // No RF received in 30 seconds so send failure 
    data_ready = 1;                                                     // Ok, data is ready
    rf_error=1;
  }
*/

  //-----------------------------------------------------------------------------------------------------------------
  // 3) Send data via ethernet
  //-----------------------------------------------------------------------------------------------------------------
  
  if (data_ready) {
    
    // include temperature data from emonglcd if it has been received
   // if (emonglcd_rx) {
     // cosm_str.print(F("GLCD_Temp,"));  
    //  cosm_str.println(emonglcd.temperature/100.0);
    //  emon_str.print(F(",GLCD_Temp:"));  
    //  emon_str.print(emonglcd.temperature/100.0);
   //   emonglcd_rx = 0;
   // }

    emon_str.print(F("}\0")); // End of json string, don't know why the \0 is needed is seems superfluous to me
       
#ifdef DEBUG
    Serial.println(F("COSM data")); Serial.println(cosm_str.buf); // print to serial cosm string
    Serial.println(F("EMON data")); Serial.println(emon_str.buf); // print to serial emon string
#endif

  }
delay(100);                                                         // make sure the serial printing to the buffer is done

// *************************POST TO COSM WEBSITE*******************************************************

#ifdef DEBUG
    Serial.println(F("About to post to COSM"));
#endif
    digitalWrite(greenLED,LOW);                                    // Green LED off - indicate that send is start
    httpHaveReply = 0;
    ether.copyIp(ether.hisip, cosmip);
    ethernet_requests ++;
 // example with-out the defines:   ether.httpPost(PSTR("/v2/feeds/67796.csv?_method=put"), website_cosm, PSTR("X-ApiKey: Y3xIpC7E622Db9fd03s2OWW4R7ySAKxDKzM1MzV3MzByST0g"), cosm_str.buf, cosm_callback);
    ether.httpPost(PSTR("/v2/feeds/" COSM_FEED_ID ".csv?_method=put"), website_cosm, PSTR("X-ApiKey: " COSM_API_KEY), cosm_str.buf, cosm_callback);
    httpReply.set(HTTP_TIMEOUT);
    httpTimeOut = false;
      if (UNO)  wdt_disable();
     while (5 != tcp_client_state) {
       ether.packetLoop(ether.packetReceive());
       if (httpReply.poll()) {
           httpTimeOut = true;
           break;
       }
    }
     if (UNO) wdt_enable(WDTO_8S);
       if (httpTimeOut)
      Serial.println(F("Http timed out"));
#ifdef DEBUG
    else
      Serial.println(F("Finished posting to COSM"));
#endif



// *************************POST TO EMONCMS WEBSITE/SERVER ***********************************************

#ifdef DEBUG
    Serial.println(F("About to post to EMON"));
#endif
    httpHaveReply = 0;
    ether.copyIp(ether.hisip, emonip);
    ethernet_requests ++;
    ether.browseUrl(PSTR("/api/post.json?apikey=" EMON_API_KEY "&json="), emon_str.buf, website_emon, emon_callback);
    httpReply.set(HTTP_TIMEOUT);
    httpTimeOut = false;
     if (UNO) wdt_disable();
       while (5 != tcp_client_state) {
       ether.packetLoop(ether.packetReceive());
       if (httpReply.poll()) {
           httpTimeOut = true;
           break;
       }
    }
    if (UNO)  wdt_enable(WDTO_8S);
        if (httpTimeOut)
      Serial.println(F("Http timed out"));
#ifdef DEBUG
    else
      Serial.println(F("Finished posting to EMON"));
#endif
    digitalWrite(greenLED,HIGH);                                    // Green LED off - indicate that send has finished 
    data_ready =0;
  }
  
  if (ethernet_requests > 10) delay(10000); // Reset the nanode if more than 10 request attempts have been tried without a reply
 } // end timer
 
 
//***************Ethernet callback*********************************************************************************
// recieve reply from COSM and EMONCMS and decode it

static void cosm_callback (byte status, word off, word len) {

#ifdef DEBUG
  Serial.println(F("Cosm server reply"));
  Serial.print(F("status ")); Serial.print(status); Serial.print(F(" length ")); Serial.println(len);
  get_header_line(1, off);
  Serial.println(line_buf);
#endif
  
  if (0 == status) {
    
    httpHaveReply = 1;
    get_header_line(2,off);      // Get the date and time from the header
#ifdef DEBUG
    Serial.print(F("Date recv from cosm server "));    // Print out the date and time
    Serial.println(line_buf);    // Print out the date and time
#endif
    
    // Decode date time string to get integers for hour, min, sec, day
    // We just search for the characters and hope they are in the right place
    char val[2];
    val[0] = line_buf[23]; val[1] = line_buf[24];
    int hour = atoi(val);
    val[0] = line_buf[26]; val[1] = line_buf[27];
    int mins = atoi(val);
    val[0] = line_buf[29]; val[1] = line_buf[30];
    int sec = atoi(val);
    val[0] = line_buf[11]; val[1] = line_buf[12];
    int day = atoi(val);
      
    if (hour>0 || mins>0 || sec>0) {  //don't send all zeros, happens when server failes to returns reponce to avoide GLCD getting mistakenly set to midnight
  	emonbase.hour = hour+2;              //add current date and time to payload ready to be sent to emonGLCD
    	emonbase.mins = mins;
    }
    //-----------------------------------------------------------------------------
    
    delay(100);
    
    // Send time dat NOT WORKING HERE!!
   // int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}    // if can send - exit if it gets stuck, as it seems too
   // rf12_sendStart(0, &emonbase, sizeof emonbase);                        // send payload
  //  rf12_sendWait(0);
    
    #ifdef DEBUG
     Serial.println(F("time sent to emonGLCD"));
     Serial.print("time "); Serial.print(emonbase.hour);     Serial.print(":");   Serial.print(emonbase.mins); 
    #endif
    
    ethernet_requests = 0; ethernet_error = 0;
  }
}
//-----------------------------------------------------------------------------
    
static void emon_callback (byte status, word off, word len) {

#ifdef DEBUG
  Serial.println(F("Emon server reply"));
  Serial.print(F("status ")); Serial.print(status); Serial.print(F(" length ")); Serial.println(len);
  get_header_line(1, off);
  Serial.println(line_buf);
#endif
  if (0 == status) {
    httpHaveReply = 1;
    ethernet_requests = 0; ethernet_error = 0;
  } 

} // END of sketch
