/* 
  _   _                           _       ______ ______   _____                    _                     _      _        _      _ 
 | \ | |                         | |      | ___ \|  ___| /  ___|                  (_)                   | |    (_)      | |    | |
 |  \| |  __ _  _ __    ___    __| |  ___ | |_/ /| |_    \ `--.   ___  _ __   ___  _  _ __    __ _  ___ | |__   _   ___ | |  __| |
 | . ` | / _` || '_ \  / _ \  / _` | / _ \|    / |  _|    `--. \ / _ \| '_ \ / __|| || '_ \  / _` |/ __|| '_ \ | | / _ \| | / _` |
 | |\  || (_| || | | || (_) || (_| ||  __/| |\ \ | |     /\__/ /|  __/| | | |\__ \| || | | || (_| |\__ \| | | || ||  __/| || (_| |
 \_| \_/ \__,_||_| |_| \___/  \__,_| \___|\_| \_|\_|     \____/  \___||_| |_||___/|_||_| |_| \__, ||___/|_| |_||_| \___||_| \__,_|
                                                                                              __/ |                               
                                                                                            |___/                                
(art: http://patorjk.com/software/taag/)
And fork portal from the orgininal emonbase and emontx firmwares for the openenergymonitor projects.
Arduino hardware shield designed by Martin Harizanov
Firmware modifycations by: Alco van Neck

*****************************************
Orginal documentation: 
emonBase Documentation: http://openenergymonitor.org/emon/emonbase
emonBase Documentation: http://openenergymonitor.org/emon/emontxt
Authors: Trystan Lea and Glyn Hudson
Part of the: openenergymonitor.org project
Licenced under GNU GPL V3

http://openenergymonitor.org/emon/license
EtherCard Library by Jean-Claude Wippler and Andrew Lindsay
JeeLib Library by Jean-Claude Wippler
****************************************/

// *****************************************
// ================== PARAMETERS ===========
// *****************************************
// ================== EMONTX ===============

//CT 1 is always enabled
const int CT2 = 0;                                                      // Set to 1 to enable CT channel 2
const int CT3 = 0;                                                      // Set to 1 to enable CT channel 3

#define freq RF12_868MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
const int nodeID = 30;                                                  // emonTx RFM12B node ID = 10 emonbase = 15 emonglcd = 20 sensingbase = 30
const int NetworkGroup = 210;                                           // emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD needs to be same as emonBase and emonGLCD
const int UNO = 1;                                                      // Set to 0 if your not using the UNO bootloader (i.e using Duemilanove) - All Atmega's shipped from OpenEnergyMonitor come with Arduino Uno bootloader

#include <avr/wdt.h>                                                    // the UNO bootloader 
#include <JeeLib.h>                                                     // Download JeeLib: http://github.com/jcw/jeelib
ISR(WDT_vect) { Sleepy::watchdogEvent(); }								// this is added as we're using the watchdog for low-power waiting

#include "EmonLib.h"
EnergyMonitor ct1,ct2,ct3;                                              // Create  instances for each CT channel

// ================== EMONBASE ===========

#define DEBUG     //comment out to disable serial printing to increase long term stability 
//#define UNO       //anti crash wachdog reset only works with Uno (optiboot) bootloader, comment out the line if using delianuova

//---------------------------------------------------
// Data structures for transfering data between units
//---------------------------------------------------

typedef struct { int power1, power2, power3, Vrms; } PayloadTX;
PayloadTX emontx;

typedef struct { int temperature; } PayloadGLCD;
PayloadGLCD emonglcd;

typedef struct { int hour, mins;} PayloadBase;
PayloadBase emonbase;
//---------------------------------------------------

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
PacketBuffer str;

//--------------------------------------------------------------------------
// Ethernet
//--------------------------------------------------------------------------
#include <EtherCard.h>		//https://github.com/jcw/ethercard 

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x42,0x31,0x42,0x21,0x30,0x31 };

//IP address of remote sever, only needed when posting to a server that has not got a dns domain name (staticIP e.g local server) 
byte Ethernet::buffer[700];
static uint32_t timer;

//Domain name of remote webserver - leave blank if posting to IP address 
// char website[] PROGMEM = "vis.openenergymonitor.org";
char website[] PROGMEM = "";
static byte hisip[] = { 192,168,1,123 };    // un-comment for posting to static IP server (no domain name)            

const int redLED = 6;                     // NanodeRF RED indicator LED
const int greenLED = 5;                   // NanodeRF GREEN indicator LED

int ethernet_error = 0;                   // Etherent (controller/DHCP) error flag
int rf_error = 0;                         // RF error flag - high when no data received 
int ethernet_requests = 0;                // count ethernet requests without reply                 

int dhcp_status = 0;
int dns_status = 0;
int emonglcd_rx = 0;                      // Used to indicate that emonglcd data is available
int data_ready=0;                         // Used to signal that emontx data is ready to be sent
unsigned long last_rf;                    // Used to check for regular emontx data - otherwise error

char line_buf[50];                        // Used to store line of http reply header


 // ********************************************************************************************************************
 // =================== SETUP ==========================================================================================
 // ********************************************************************************************************************
 
 // ================== EMONTX SETUP ===========
 
void setup() 
{
  Serial.begin(9600);
  Serial.println("emonTX CT123 Voltage example");
  Serial.println("OpenEnergyMonitor.org");
  Serial.print("Node: "); 
  Serial.print(nodeID); 
  Serial.print(" Freq: "); 
  if (freq == RF12_433MHZ) Serial.print("433Mhz");
  if (freq == RF12_868MHZ) Serial.print("868Mhz");
  if (freq == RF12_915MHZ) Serial.print("915Mhz"); 
  Serial.print(" Network: "); 
  Serial.println(NetworkGroup);
  
  ct1.voltageTX(264.0, 1.7);                                         // ct.voltageTX(calibration, phase_shift) - make sure to select correct calibration for AC-AC adapter  http://openenergymonitor.org/emon/modules/emontx/firmware/calibration (Mascot 9580 = 234.26)
  ct1.currentTX(1, 60.60);                                            // Setup emonTX CT channel (channel (1,2 or 3), calibration)
                                                                      // CT Calibration factor = CT ratio / burden resistance
  ct2.voltageTX(234.26, 1.7);                                         // CT Calibration factor = (100A / 0.05A) x 18 Ohms
  ct2.currentTX(2, 74.07);											  // CT Calibration factor = (100A / 0.05A) x 27 Ohms
																	  // CT Calibration factor = (100A / 0.05A) x 33 Ohms
  ct3.voltageTX(234.26, 1.7);
  ct3.currentTX(3, 60.60);
  
  rf12_initialize(nodeID, freq, NetworkGroup);                          // initialize RF
  rf12_sleep(RF12_SLEEP);

  //pinMode(greenLED, OUTPUT);                                              // Setup indicator LED
  //digitalWrite(greenLED, HIGH);
  
  if (UNO) {
    wdt_enable(WDTO_8S); 
  }                                        // Enable anti crash (restart) watchdog if UNO bootloader is selected. Watchdog does not work with duemilanove bootloader                                                             // Restarts emonTx if sketch hangs for more than 10s

  // ================== EMONBASE SETUP ===========
   
  //Nanode RF LED indictor  setup - green flashing means good - red on for a long time means bad! 
  //High means off since NanodeRF tri-state buffer inverts signal 
  pinMode(redLED, OUTPUT); digitalWrite(redLED,LOW);            
  pinMode(greenLED, OUTPUT); digitalWrite(greenLED,LOW);       
  delay(100); digitalWrite(redLED,HIGH);                          // turn off redLED
  
  Serial.begin(9600);
  Serial.println("\n[webClient]");

  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) {
    Serial.println( "Failed to access Ethernet controller");
    ethernet_error = 1;  
  }

  dhcp_status = 0;
  dns_status = 0;
  ethernet_requests = 0;
  ethernet_error=0;
  rf_error=0;
 
  rf12_initialize(nodeID,freq,NetworkGroup);
  last_rf = millis()-40000;                                       // setting lastRF back 40s is useful as it forces the ethernet code to run straight away
   
  digitalWrite(greenLED,HIGH);                                    // Green LED off - indicate that setup has finished 
   }
 
 // ***************************************************************************************************************************
 // =================== LOOP ======================================LOOP========================================LOOP============
 // ***************************************************************************************************************************
 
 
 // ================== EMONTX LOOP===========
 
void loop() 
{ 
  ct1.calcVI(20,2000);                                                  // Calculate all. No.of crossings, time-out 
  emontx.power1 = ct1.realPower;
  Serial.print("CT1  "); Serial.print(emontx.power1); 
  
  emontx.Vrms = ct1.Vrms*100;                                          // AC Mains rms voltage 
  
  if (CT2) {  
    ct2.calcVI(20,2000);                                               //ct.calcVI(number of crossings to sample, time out (ms) if no waveform is detected)                                         
    emontx.power2 = ct2.realPower;
    Serial.print("CT2 "); Serial.print(emontx.power2);
  }

  if (CT3) {
    ct3.calcVI(20,2000);
    emontx.power3 = ct3.realPower;
    Serial.print("CT3 "); Serial.print(emontx.power3);
  }
  
  Serial.print("VRMS "); Serial.print(ct1.Vrms);

  Serial.println(); delay(100);
 
  //send_rf_data();                                                       // *SEND RF DATA* - see emontx_lib
  digitalWrite(greenLED, HIGH); delay(200); digitalWrite(greenLED, LOW);      // flash LED
  //emontx_sleep(5);                                                      // sleep or delay in seconds - see emontx_lib

// ================== EMONBASE LOOP===========
  
 
  if (UNO) {
    wdt_reset();
  }														// Reset the wathdog timer
  dhcp_dns();   															// handle dhcp and dns setup - ** see dhcp_dns sketch
  
  // Display error states on status LED
  // if (ethernet_error==1 || rf_error==1 || ethernet_requests > 0) digitalWrite(redLED,LOW);
    // else digitalWrite(redLED,HIGH);

  //-----------------------------------------------------------------------------------------------------------------
  // 1) On RF recieve
  //-----------------------------------------------------------------------------------------------------------------
  // if (rf12_recvDone()){      
      // if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)
      // {
        // int node_id = (rf12_hdr & 0x1F);
        
        // if (node_id == 10)                                               // EMONTX
        // {
          emontx = *(PayloadTX*) rf12_data;                              // get emontx data
	// emontx = (PayloadTX);                              // get emontx data
          Serial.println();                                              // print emontx data to serial
          Serial.print("emonTx data rx");
          last_rf = millis();                                            // reset lastRF timer
          
          delay(50);                                                     // make sure serial printing finished
                               
          // JSON creation: JSON sent are of the format: {key1:value1,key2:value2} and so on
          
          str.reset();                                                   // Reset json string      
          str.print("{rf_fail:0");                                       // RF recieved so no failure
          str.print(",power1:");        str.print(emontx.power1);          // Add power reading
          str.print(",voltage:");      str.print(emontx.Vrms);        // Add emontx battery voltage reading
    
          data_ready = 1;                                                // data is ready
          rf_error = 0;
        // }
        
        // if (node_id == 20)                                               // EMONGLCD 
        // {
          // emonglcd = *(PayloadGLCD*) rf12_data;                          // get emonglcd data
          // Serial.print("emonGLCD temp recv: ");                          // print output
          // Serial.println(emonglcd.temperature);  
          // emonglcd_rx = 1;        
        // }
      // }
    // }

  //-----------------------------------------------------------------------------------------------------------------
  // 2) If no data is recieved from rf12 module the server is updated every 30s with RFfail = 1 indicator for debugging
  //-----------------------------------------------------------------------------------------------------------------
  if ((millis()-last_rf)>30000)
  {
    last_rf = millis();                                                 // reset lastRF timer
    str.reset();                                                        // reset json string
    str.print("{rf_fail:1");                                            // No RF received in 30 seconds so send failure 
    data_ready = 1;                                                     // Ok, data is ready
    rf_error=1;
  }


  //-----------------------------------------------------------------------------------------------------------------
  // 3) Send data via ethernet
  //-----------------------------------------------------------------------------------------------------------------
  ether.packetLoop(ether.packetReceive());
  
  if (data_ready) {
    
    // include temperature data from emonglcd if it has been recieved
    if (emonglcd_rx) {
      str.print(",temperature:");  
      str.print(emonglcd.temperature/100.0);
      emonglcd_rx = 0;
    }
    
    str.print("}\0");  //  End of json string
    
    Serial.print("2 "); Serial.println(str.buf); // print to serial json string

    // Example of posting to emoncms v3 demo account goto http://vis.openenergymonitor.org/emoncms3 
    // and login with sandbox:sandbox
    // To point to your account just enter your WRITE APIKEY 
	// server van site: 258a74e3d96a4925a6159b73b2af0835
	//eigen server:f5f30b19af093131553e21ea00b6e36b
    ethernet_requests ++;
    ether.browseUrl(PSTR("/emoncms/api/post.json?apikey=f5f30b19af093131553e21ea00b6e36b&json="),str.buf, website, my_callback);
    data_ready =0;
  }
  
  // if (ethernet_requests > 10) delay(10000); // Reset the nanode if more than 10 request attempts have been tried without a reply

}
//**********************************************************************************************************************

//-----------------------------------------------------------------------------------
// Ethernet callback
// recieve reply and decode
//-----------------------------------------------------------------------------------
static void my_callback (byte status, word off, word len) {
  
  get_header_line(2,off);      // Get the date and time from the header
  Serial.print("ok recv from server | ");    // Print out the date and time
  Serial.println(line_buf);    // Print out the date and time
  
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
    
  if (hour>0 || mins>0 || sec>0) {  //don't send all zeros, happens when server failes to returns reponce to avoide GLCD getting mistakenly set to midnight
	emonbase.hour = hour;              //add current date and time to payload ready to be sent to emonGLCD
  	emonbase.mins = mins;
  }
  //-----------------------------------------------------------------------------
  
  delay(100);
  
  // Send time data
  int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}    // if can send - exit if it gets stuck, as it seems too
  rf12_sendStart(0, &emonbase, sizeof emonbase);                        // send payload
  rf12_sendWait(0);
  
  Serial.println("time sent to emonGLCD");
  
  get_reply_data(off);
  if (strcmp(line_buf,"ok")) {ethernet_requests = 0; ethernet_error = 0;}  // check for ok reply from emoncms to verify data post request
}



