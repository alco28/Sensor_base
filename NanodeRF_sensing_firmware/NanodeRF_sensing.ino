// 
  // _   _                           _       ______ ______   _____                    _                     _      _        _      _ 
 // | \ | |                         | |      | ___ \|  ___| /  ___|                  (_)                   | |    (_)      | |    | |
 // |  \| |  __ _  _ __    ___    __| |  ___ | |_/ /| |_    \ `--.   ___  _ __   ___  _  _ __    __ _  ___ | |__   _   ___ | |  __| |
 // | . ` | / _` || '_ \  / _ \  / _` | / _ \|    / |  _|    `--. \ / _ \| '_ \ / __|| || '_ \  / _` |/ __|| '_ \ | | / _ \| | / _` |
 // | |\  || (_| || | | || (_) || (_| ||  __/| |\ \ | |     /\__/ /|  __/| | | |\__ \| || | | || (_| |\__ \| | | || ||  __/| || (_| |
 // \_| \_/ \__,_||_| |_| \___/  \__,_| \___|\_| \_|\_|     \____/  \___||_| |_||___/|_||_| |_| \__, ||___/|_| |_||_| \___||_| \__,_|
                                                                                              // __/ |                               
                                                                                            // |___/                                
// (art: http://patorjk.com/software/taag/)
// And fork portal from the orgininal emonbase and emontx firmwares for the openenergymonitor projects.
// Arduino hardware shield designed by Martin Harizanov
// Firmware modifycations by: Alco van Neck

// Orginal documentation: 
// emonBase Documentation: http://openenergymonitor.org/emon/emonbase
// emonTX Documentation: http://openenergymonitor.org/emon/emontxt
// Orignal Authors: Trystan Lea and Glyn Hudson
// Part of the: openenergymonitor.org project
// Licenced under GNU GPL V3
// http://openenergymonitor.org/emon/license

//LIBS:
// EtherCard Library by Jean-Claude Wippler and Andrew Lindsay
// JeeLib Library by Jean-Claude Wippler


// =============================== LIBARY ==============================================

#include <avr/wdt.h>                                                    // the UNO bootloader
#include <JeeLib.h>                                                     // Download JeeLib: http://github.com/jcw/jeelib
#include "EmonLib.h"							// Openenergymonitor sleepfunctions for RFM12B
#include <OneWire.h>							// 1-WIRE communication
#include <DallasTemperature.h>						// DALLAS TEMP sensor DS18B20 lib.
#include <EtherCard.h>							// http://github.com/jcw/ethercard

// ********** SERIAL COMMUNICATION & DEFINES*********************

#define DEBUG  							        // Uncomment to set serial print communiction for debuging.
#define test                                                            // Sets Serial print-line with sensor readings
//#define JSON                                                          // sets JSON and ethernet send part on/off

// =============================== PARAMETERS ==============================================

// **** SENSORS *****
//Current sensors
//CT 1 is always enabled
const int CT2 = 0;                                                      // Set to 1 to enable CT channel 2
const int CT3 = 0;                                                      // Set to 1 to enable CT channel 3

EnergyMonitor ct1,ct2,ct3;                                              // Create  instances for each CT channel

// VOLTAGE sensor
//	none

// TEMPERATURE sensor (Dallas 1-wire)
#define ONE_WIRE_BUS 4							// Data wire is plugged into pin 4 on the Arduino
OneWire oneWire(ONE_WIRE_BUS);						// Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);					// Pass our oneWire reference to Dallas Temperature.

// Assign the addresses of your 1-Wire temp sensors.
// See the tutorial on how to obtain these addresses: http://www.hacktronics.com/Tutorials/arduino-1-wire-address-finder.html

DeviceAddress address_Temp1 = { 0x28, 0xD3, 0x7E, 0x11, 0x04, 0x00, 0x00, 0x65};

// MAGNETIC HALL sensor
//const int magPin = 7;                                                // Magnetic sensor pin

// LDR (lux) sensor
const int LDRpin = A4;			                                 // Light dependent resistor sensor pin


// ******************************************************************************
// LED's
const int greenLED = 5;                                                  // NanodeRF GREEN indicator LED
const int redLED = 6;                                                    // NanodeRF RED indicator LED

// Wireless devices
#define freq RF12_868MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. SET YOUR FREQ RIGHT!
const int nodeID = 30;                                                  // emonTx RFM12B node ID = 10 emonbase = 15 emonglcd = 20 sensingbase
const int NetworkGroup = 210;                                           // emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD needs to be same as emonBase and emonGLCD
unsigned long last_rf;                    				// Used to check for regular RF data - otherwise error

// Watchdog anti-crash system
// NOT USED IN THIS TRIAL VERSION
const int UNO = 1;                                                      // Set to 0 if your NOT using the UNO bootloader (i.e using Duemilanove) - All Atmega's shipped from OpenEnergyMonitor come with Arduino Uno bootloader
ISR(WDT_vect) { Sleepy::watchdogEvent(); }				// this is added as we're using the watchdog for low-power waiting

// ***** The PacketBuffer class is used to generate the json string that is send via ethernet - JeeLabs *****
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
PacketBuffer str;

//---------------------------------------------------
// Data structures for transfering data between units
//---------------------------------------------------
//typedef struct { int power1, power2, power3, Vrms; } PayloadTX;
//PayloadTX emontx;

typedef struct {
  	  int realPower;	                                // RealPower
          int apparentPower;	                                // ApparentPower
	  int Vrms;						// voltage
  	  int Temp1;		                                // temperature sensor 1
          int LDR_reading;                                 // the analog reading from the sensor divider
          int hour, mins;                                       // time

	} PayloadTX;
PayloadTX emontx;

//typedef struct { int hour, mins;} PayloadBase;
//PayloadBase emonbase;

//--------------------------------------------------------------------------
// Ethernet
//--------------------------------------------------------------------------
// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0xF2,0x31,0x42,0x21,0x30,0x03 };

//IP address of remote sever, only needed when posting to a server that has not got a dns domain name (staticIP e.g local server)
byte Ethernet::buffer[700];
static uint32_t timer;

//Domain name of remote webserver - leave blank if posting to IP address
 char website[] PROGMEM = "energy.bluemotica.nl";
//char website[] PROGMEM = "";
//static byte hisip[] = { 192,168,1,115 };    // un-comment for posting to static IP server (no domain name)

int ethernet_error = 0;                                       // Etherent (controller/DHCP) error flag
int rf_error = 0;                                             // RF error flag - high when no data received
int ethernet_requests = 0;                                    // count ethernet requests without reply
int dhcp_status = 0;
int dns_status = 0;
int data_ready=0;                                             // Used to signal that emontx data is ready to be sent
char line_buf[50];                                            // Used to store line of http reply header



// ********************************************************************************************************************
// =================== SETUP =========================SETUP===========SETUP====================SETUP===================
// ********************************************************************************************************************

void setup() {

// PINMODE defines
//High means off since NanodeRF tri-state buffer (schmitt-triger chip 74HC124) inverts the signal
pinMode(greenLED, OUTPUT); digitalWrite(redLED,LOW);         // Setup indicator green LED
pinMode(greenLED, OUTPUT); digitalWrite(greenLED,LOW);       // Setup indicator red LED

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
CT Calibration factor = 60.60  (100A / 0.05A) x 18 Ohms
CT Calibration factor = 74.07  (100A / 0.05A) x 27 Ohms
CT Calibration factor = 111,11 (100A / 0.05A) x 33 Ohms
*/


delay(100); digitalWrite(redLED,HIGH);                     // turn off redLED after setting the sensors

// WIRELESS DATA SETUP
rf12_initialize(nodeID, freq, NetworkGroup);              // initialize RF
rf12_sleep(RF12_SLEEP);

// ANTI-CRASH SETUP
//  if (UNO) {
//    wdt_enable(WDTO_8S); 				  // Restarts emonTx if sketch hangs for more than 8s
//}
//


// SERIAL COMMUNICATION
											
Serial.begin(9600);

#ifdef DEBUG                                              // Setup serial communcation if Debug mode  is ON
Serial.println("\n[webClient]");
	if (		ether.begin(sizeof Ethernet::buffer, mymac) == 0) {
	Serial.println( "Failed to access Ethernet controller");
	ethernet_error = 1;  }

Serial.println("NanodeRF testing");
Serial.print("Node: ");
Serial.print(nodeID);
Serial.print(" Freq: ");
	if (freq == RF12_433MHZ) Serial.print("433Mhz");
	if (freq == RF12_868MHZ) Serial.print("868Mhz");
	if (freq == RF12_915MHZ) Serial.print("915Mhz");
Serial.print(" Network: ");
Serial.println(NetworkGroup);

#endif

// Reset the status for variables
dhcp_status = 0;
dns_status = 0;
ethernet_requests = 0;
ethernet_error=0;
rf_error=0;

rf12_initialize(nodeID,freq,NetworkGroup);
last_rf = millis()-40000;                                       // setting lastRF back 40s is useful as it forces the ethernet code to run straight away

delay(100); digitalWrite(greenLED,HIGH);                        // turn off greenLED after good setup communication

//END SETUP
}

 // ***************************************************************************************************************************
 // =================== LOOP ======================================LOOP========================================LOOP============
 // ***************************************************************************************************************************
 
 void loop() { 
  
dhcp_dns();                                                           // handle dhcp and dns setup - see dhcp_dns tab
sensors.requestTemperatures();                                        // Send the command to get temperature
emontx.Temp1 = sensors.getTempC(address_Temp1)*100 ;                  // read sensor Temp1 and multiply with 100 for resolution of 2 decimals

 
ct1.calcVI(20,2000);                                                  // Calculate all. No.of crossings, time-out 
emontx.realPower     =   ct1.realPower;                               // rename sensors to emontx
emontx.apparentPower =   ct1.apparentPower;
emontx.Vrms          =   ct1.Vrms;     
emontx.LDR_reading           =   analogRead(LDRpin);

#ifdef test                                                          // print sensor readings to serial-line in TEST MODE
  Serial.print("realpw ");  Serial.print(" ");  Serial.print(emontx.realPower);
  Serial.print("appPW ");   Serial.print(" ");  Serial.print(emontx.apparentPower);
  Serial.print("Vrms ");    Serial.print(" ");  Serial.print(emontx.Vrms);
  Serial.print("temp ");    Serial.print(" ");  Serial.print(emontx.Temp1);
  Serial.print("LDR ");    Serial.print(" ");  Serial.print(emontx.LDR_reading);
  Serial.print("time ");    Serial.print(" ");  Serial.print(emontx.hour);Serial.print(emontx.mins);
  
  
  Serial.println(" "); 
  delay(500);
#endif 
 
 
 
  // **********************************************************************************************************************
  //Make an JSON String and post it to the server
 #ifdef JSON 			// activated by JSON DEFINE PARAM
 
// emontx = *(PayloadTX*)                                            // NEED IT???
 Serial.println();                                                  // print emontx data to serial
 Serial.print("emonTx data rx");
 str.print(",power1:");           str.print(emontx.realPower);      // Add power reading
 str.print(",voltage:");          str.print(emontx.Vrms);           // Add voltage reading
 str.print(",temperature:");      str.print(emontx.Temp1/100.0);    // Add voltage reading


 
ether.packetLoop(ether.packetReceive());                           // ??? WHATS THIS SYNTAX ?? count packages? OR???
    
str.print("}\0");  //  End of json string
Serial.print("2 "); Serial.println(str.buf); // print to serial json string
 
// To point to your account just enter your WRITE APIKEY 
// server:06cc8cd3437f02d62272f7b663d4a70d (demo/demo)

ethernet_requests ++;                // counter for enternet request
ether.browseUrl(PSTR("/emoncms/api/post.json?apikey=06cc8cd3437f02d62272f7b663d4a70d&json="),str.buf, website, my_callback );

str.reset();                                                   // Reset json string  GOOD POSITION???
//data_ready =0;
  
  
if (ethernet_requests > 10) delay(5000); // Reset the nanode if more than 10 request attempts have been tried without a reply
}

//**********************************************************************************************************************

//-----------------------------------------------------------------------------------
// Ethernet callback
// recieve reply and decode
//-----------------------------------------------------------------------------------

static void my_callback (byte status, word off, word len) {
  
  get_header_line(2,off);                    // Get the date and time from the header
  Serial.print("ok recv from server | ");    // Print out the date and time
  Serial.println(line_buf);                  // Print out the date and time
  
  // Decode date time string to get integers for hour, min, sec, day
  // We just search for the characters and hope they are in the right place
  char val[1];
  val[0] = line_buf[23]; val[1] = line_buf[24];
  int hour = atoi(val);
  val[0] = line_buf[26]; val[1] = line_buf[27];
  int mins = atoi(val);
  val[0] = line_buf[29]; val[1] = line_buf[30];
  int sec = atoi(val);
  val[0] = line_buf[11]; val[1] = line_buf[12];
  int day = atoi(val);
    
  if (hour>0 || mins>0 || sec>0) {        //don't send all zeros, happens when server failes to returns reponce to avoide GLCD getting mistakenly set to midnight
	emontx.hour = hour;              //add current date and time to payload ready to be sent to emonGLCD
  	emontx.mins = mins;
  }
  
  
  // GET Reply from emoncms----------------------------------------------------------------------------
 
  delay(100);
  get_reply_data(off);
  if (strcmp(line_buf,"ok")) {ethernet_requests = 0; ethernet_error = 0;}  // check for ok reply from emoncms to verify data post request
  
 
 #endif 
 
 // END VOID LOOP
} 



