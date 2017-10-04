#include <Arduino.h>



#define UNO       //anti crash wachdog reset only works with Uno (optiboot) bootloader, comment out the line if using delianuova
//#define SERIAL_PRINT    1                  // Uncomment to set serial print communiction for debuging.
//#define DEBUG   1               // Uncomment to set serial print communiction for debuging.


#include <JeeLib.h>      //https://github.com/jcw/jeelib
#include <avr/wdt.h>
ISR(WDT_vect) { Sleepy::watchdogEvent(); }
#include <OneWire.h>              // 1-WIRE communication
#include <DallasTemperature.h>            // DALLAS TEMP sensor DS18B20 lib.
#include "EmonLib.h"              // Openenergymonitor calculation functions

//########################## SET TIMERS ############################################################################################
#define SENSORtimer 5000 //20000         // how often scan the sensors      (ms)
#define SENDtimer 20000                  // how often report the base sensors  (ms)
#define FILTERSETTLETIME 53000           //  Time (ms) to allow the filters to settle before sending data
boolean settled = false;
unsigned long Tsensors;
unsigned long TSend;
//#####################################################################################################################################

///--------------------------------------------------------PULSE COUNTER------------------------------------------------------------------------------------

const int magPin = 3;      // Magnetic sensor 

//-----------------------------------------------------------ETHERNET---------------------------------------------------------------------------------------
#include <EtherCard.h>    //https://github.com/jcw/ethercard 

// ethernet interface mac address, must be unique on the LAN
static byte mymac[]  = { 0xF2,0x31,0x42,0x21,0x30,0x03 };

byte Ethernet::buffer[350];     // orgininal 700 buffer
byte ethernet_requests = 0;                // count ethernet requests without reply   

//const char website[] PROGMEM = "192.168.1.****";
const char website[] PROGMEM = "YOUR LOCAL URL"; //replace with your server url
const char apikey[]  = "045612313453621";		// replace with your EmonCMS key

// This is the char array that holds the reply data
char line_buf[50];

//---------------------------------------------------------DATA STRUCTURE-----------------------------------------------------------------------------------
// Data structures for transfering data between units

typedef struct { float realPower, apparentPower, realPower2, apparentPower2, realPower3, apparentPower3 , voltage; int Temp1, Temp2, Temp3, Pulse; } PayloadTX;
PayloadTX emontx;

typedef struct { int hotwater, heater_out, heater_in, battery; } PayloadJH;
PayloadJH Jeeheater;
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
    char buf[250]; //was default char buf[250];
    private:
};
PacketBuffer str; 


// ********************************************** SENSORS *****************************************************************************************

//--------------------------------------------------------CASE LEDS-----------------------------------------------------------------------------------------
const byte BlueLED = 6;                     // NanodeRF RED indicator LED
const byte greenLED = 5;                   // NanodeRF GREEN indicator LED

///--------------------------------------------------------PULSE COUNTER------------------------------------------------------------------------------------
const int maxPulse = 10;       				// Send before time if more than n Pulses 
// Variables:
int magState = 0;         
int magLast = -1;

unsigned int gasPulses = 0;
unsigned int gasThis = 0;

unsigned long timeLast = 0;        // Sleep


//--------------------------------------------------------CURRENT SENSORS-----------------------------------------------------------------------------------
//CT 1 is always enabled
// const int to byte..

const byte CT2 = 1;                                                      // Set to 1 to enable CT channel 2
const byte CT3 = 1;                                                      // Set to 1 to enable CT channel 3

//EnergyMonitor ct1,ct2;              

EnergyMonitor ct1,ct2,ct3;                                               // Create  instances for each CT channel


///--------------------------------------------------------TEMPERATURE SENSORS-----------------------------------------------------------------------------------

#define ONE_WIRE_BUS 4                             // Data wire is plugged into pin 4 on the Arduino
OneWire oneWire(ONE_WIRE_BUS);                    // Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);              // Pass our oneWire reference to Dallas Temperature.

const byte sensorResolution_T1 = 10;             //DS18B20 resolution 9,10,11 or 12bit corresponding to (0.5, 0.25, 0.125, 0.0625 degrees C LSB), lower resolution means lower power
const byte sensorResolution_T2 = 10;  
const byte sensorResolution_T3 = 10;  

// Assign the addresses of your 1-Wire temp sensors.
DeviceAddress address_Temp1  =  {0x28, 0xD3, 0x7E, 0x11, 0x04, 0x00, 0x00, 0x65};
DeviceAddress address_Temp2  = {0x28, 0x2D, 0x94, 0x11, 0x04, 0x00, 0x00, 0x63};
DeviceAddress address_Temp3  = {0x28, 0xE3, 0x4F, 0x11, 0x04, 0x00, 0x00, 0xC1};



//*****************SETUP*******************************SETUP**************************************SETUP**************************************************************
//*******************************************************************************************************************************************************************
void setup () 
{ //**** START SETUP ****

  //Nanode RF LED indictor  setup - green flashing means good - red on for a long time means bad! 
  //High means off since NanodeRF tri-state buffer inverts signal 
  pinMode(BlueLED, OUTPUT); digitalWrite(BlueLED,HIGH);      // turn off BlueLED          
  pinMode(greenLED, OUTPUT); digitalWrite(greenLED,LOW);    // turn ON greenLED    
                          
  #ifdef SERIAL_PRINT 
  Serial.begin(57600);
  //Serial.println(F("\nNanodeRF"));
  #endif
  
//-----------------------------------------------------------ETHERNET---------------------------------------------------------------------------------------
    #ifdef UNO //disable watchdog at boot
    wdt_disable();
    #endif 
  
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
    Serial.println(F("Fail ENET"));
    
  // DHCP Setup
  if (!ether.dhcpSetup())
    Serial.println(F("DHCP fail"));

  ether.printIp("IP:", ether.myip);
  ether.printIp("GW:", ether.gwip);  
  ether.printIp("DNS:", ether.dnsip);  

  // DNS Setup
  if (!ether.dnsLookup(website))
    Serial.println(F("DNS fail"));
    
  ether.printIp("SRV: ", ether.hisip);
//--------------------------------------------------------RF12 RADIO-----------------------------------------------------------------------------------
  rf12_set_cs(10);
  #define RADIOSYNCMODE 2
  // config uit eeprom of handmatig minder bytes..!!
  rf12_config();
 // rf12_initialize(MYNODE, freq,group);
// last_rf = millis()-40000;                                       // setting lastRF back 40s is useful as it forces the ethernet code to run straight away


//--------------------------------------------------------CURRENT SENSORS-----------------------------------------------------------------------------------
// Voltages sensor
// own value: 239.96 date: 5 juni 2015 1.55 31-3-2016 == 272.55

ct1.voltage(3, 253.24, 1.0);                                   // ct.voltageTX(calibration, phase_shift) - make sure to select correct calibration for AC-AC adapter (Mascot 9580 = 234.26)
ct2.voltage(3, 253.24, 1.0);                                 // for use with current sensor 2
ct3.voltage(3, 253.24, 1.0);                                // for use with current sensor 3

// Setup  Current sensor channels and calibration)
// Calibration factor = CT ratio / burden resistance = (100A / 0.05A) / 33 Ohms = 60.606
// ctX.current(port,calfactor);

ct1.current(1, 57.30);                                  // current sensor 1 (near voltage input, up) 
ct2.current(2, 58.20);                                  // current sensor 2
ct3.current(0, 58.40);                                  // current sensor 3 

/* per medio 2016:
ct1.current(1, 57.30);                                  // current sensor 1 (near voltage input, up) 
ct2.current(2, 58.20);                                  // current sensor 2
ct3.current(0, 58.40);                                  // current sensor 3 
*/

/* CT Calibration factor = CT ratio / burden resistance
|---------------------------------------------------------|
|CT Calibration factor = 60.60  (100A / 0.05A) x 18 Ohms  |
|CT Calibration factor = 74.07  (100A / 0.05A) x 27 Ohms  |
|CT Calibration factor = 111,11 (100A / 0.05A) x 33 Ohms  |
|---------------------------------------------------------|
*/
//--------------------------------------------------------TEMPERATURE SENSORS-----------------------------------------------------------------------------------
  sensors.begin();
  sensors.setResolution(address_Temp1, sensorResolution_T1);
  sensors.setResolution(address_Temp2, sensorResolution_T2);
  sensors.setResolution(address_Temp3, sensorResolution_T3);
//--------------------------------------------------------PULSE SENSORS-----------------------------------------------------------------------------------

  pinMode(magPin, INPUT);
  digitalWrite (magPin, HIGH);
  timeLast = millis() - SENSORtimer;

delay(50);                                                       // make sure setup sensors has ended
digitalWrite(greenLED,HIGH);                                    // Green LED off - indicate that setup has finished 
 
    #ifdef UNO
    wdt_enable(WDTO_8S); //enable the watchdog
    #endif
 
} //**** END SETUP ***********************************************************************************************************


//******************LOOP*******************************LOOP******************************************LOOP********************
//***************************************************************************************************************************
void loop () 

{ //**** START LOOP *****
  ether.packetLoop(ether.packetReceive());

  #ifdef UNO
  wdt_reset(); // looks good so reset the watchdog!
  #endif

void rf_node (); // always listing to RF nodes and send data, no timed receive.


//**** START TIMED EVENTS ****
if ((millis()-Tsensors)>SENSORtimer) // sece sensors every X seconds
  { 
  //  digitalWrite(greenLED,LOW);  //indicator for Radio sending&receiving state
  
Tsensors = millis() ;//reset timer
CT_sensors(); //read out CT sensors + Voltage
T_sensors(); //read out Temperature DS18B20
Pulse_sensor(); // read out pulse sensor
} // end sensor timer loop


if ((millis()-TSend)>SENDtimer) // send sensingshield data every xx seconds to emoncms
  { 
TSend = millis() ;//reset timer    
send_emoncms(); // send base sensor data to emoncms
  }; //end send timer loop
  

 //digitalWrite(greenLED,HIGH);  //indicator for Radio END sending&receiving state
 
 
} // **** END LOOP *****
