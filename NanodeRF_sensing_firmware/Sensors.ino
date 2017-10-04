#include <Arduino.h>


//--------------------------------------------------------CURRENT SENSORS-----------------------------------------------------------------------------------
void CT_sensors()
{
    ct1.calcVI(20,2000);                                       // Calculate all. No.of crossings, time-out 
    delay(200); // settle ac-ac adapter
    emontx.realPower         =   ct1.realPower;                //rename sensors to emontx
    emontx.apparentPower     =   ct1.apparentPower;
    emontx.voltage           =   ct1.Vrms; 

    ct2.calcVI(20,2000);                                       // Calculate all. No.of crossings, time-out 
    delay(200); // settle ac-ac adapter
   
    emontx.realPower2        =   ct2.realPower;                //rename sensors to emontx
    emontx.apparentPower2    =   ct2.apparentPower;
    emontx.voltage           =   ct2.Vrms; 

    ct3.calcVI(20,2000);                                       // Calculate all. No.of crossings, time-out 
    delay(200); // settle ac-ac adapter
    emontx.voltage           =    ct3.Vrms; 
    emontx.realPower3         =   ct3.realPower;                //rename sensors to emontx
    emontx.apparentPower3     =   ct3.apparentPower;

         
  if (!settled && millis() > FILTERSETTLETIME) settled = true;
  if (settled) { 
          str.reset();          // Reset json string      
          str.print("/input/post.json?");
          str.print("apikey=");    str.print(apikey);
          str.print("&node=");    str.print(20);      //basestation ID = 20
      
          str.print(F("&json={RP1:"));     str.print(emontx.realPower);           // CT sensor 1 realpower 
          str.print(F(",AP1:"));           str.print(emontx.apparentPower);       // CT sensor 1 apparentPower
          str.print(F(",RP2:"));           str.print(emontx.realPower2);           // CT sensor 2 realpower 
          str.print(F(",AP2:"));           str.print(emontx.apparentPower2);       // CT sensor 2 apparentPower

          str.print(F(",RP3:"));           str.print(emontx.realPower3);           // CT sensor 3 realpower 
          str.print(F(",AP3:"));           str.print(emontx.apparentPower3);       // CT sensor 3 apparentPower

          str.print(F(",VT:"));            str.print(emontx.voltage);             // Arduino voltage
          
          str.print(F(",TAR1:"));     str.print(emontx.Temp1/100.0);           // Arduino temperature
          str.print(F(",TWI:"));      str.print(emontx.Temp2/100.0);           //waterinlet
          str.print(F(",TBM:"));      str.print(emontx.Temp3/100.0);           // basement
                   
          str.print("}\0");  //  End of json string
  } // end settled

    #ifdef DEBUG
    Serial.println(F("print: ")); Serial.print(str.buf); // print to serial json string 
    Serial.print("Amp: "); Serial.print(ct1.Irms);Serial.print(" 2: ");Serial.print(ct2.Irms);Serial.print(" 3: ");Serial.println(ct3.Irms);
    #endif

flashLed_green(20); flashLed_green(20); // flash the green led a few times
} // end CT_sensors

//--------------------------------------------------------TEMPERATURE SENSORS-----------------------------------------------------------------------------------
void T_sensors()
{
sensors.requestTemperatures();                                       // Send the command to get temperature
delay(50);
emontx.Temp1             = sensors.getTempC(address_Temp1)*100;   // read sensor Temp1 calibration (11-2012) =5 degrees and multiply with 100 for resolution of 2 decimals
emontx.Temp2             = sensors.getTempC(address_Temp2)*100;      // read sensor Temp1 and multiply with 100 for resolution of 2 decimals
emontx.Temp3             = sensors.getTempC(address_Temp3)*100;      // read sensor Temp1 and multiply with 100 for resolution of 2 decimals
//flashLed_green(20);  // flash the green led a few times 
 
 } // end temp sensors

//------------------------- pulscounter-------------------------------------------------------------------------------------------------------------------------
void Pulse_sensor() 
{
  if (((millis() - timeLast) > SENSORtimer) || (gasThis >= maxPulse)) {
    emontx.Pulse = gasPulses;
    
   #ifdef SERIAL
      Serial.print(F("pulse: "));     
      Serial.print(emontx.Pulse);              
    #endif;

   
    timeLast = millis();
    gasThis = 0;
  }

// read the pulse sensor
  magState = digitalRead(magPin);
  // Check for change. No need for de-bounce as the sensor latches.
  if (magState != magLast && magLast != -1) {
    if (magState == 1 && magLast != -1) {  
      gasPulses++;
      gasThis++;
      flashLed(200);
      Serial.print(F("On"));
    } else {
      flashLed(100);
      Serial.print(F("Off"));
    }
    Serial.println (gasThis);
  } 
  magLast = magState;
} //end sensor

//------------------------- RADIO RECEIVE-------------------------------------------------------------------------------------------------------------------------
void rf_node ()
{
//-----------------------------------------------------------------------------------------------------------------
  // 1) On RF recieve
  //-----------------------------------------------------------------------------------------------------------------
        
if (rf12_recvDone()){      
      if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)
      {
        int node_id = (rf12_hdr & 0x1F);
         delay(150);                                                    // !Need proper delay after receiving else no good ACK send!
         if (RF12_WANTS_ACK) 
         {
         rf12_sendStart(RF12_ACK_REPLY, &node_id, 2);                     // send ACK (auto reply on receiving wireless data packet) to Jeeheater   
         rf12_sendWait(RADIOSYNCMODE);                                    // powermode of radio
      
         }
         delay(30);                                                       // make sure ACK sending is  finished


         //----------JEEHEATHER------------------
        if (node_id == 10)                                               // JEEHEATHER
        {
          Jeeheater = *(PayloadJH*) rf12_data;                              // get emontx data
         #ifdef DEBUG
          Serial.println();                                              // print emontx data to serial
          Serial.print("JHTR");
          #endif;
          flashLed(30); flashLed(40); flashLed(20);// flash the blue led a few times
                               
          // JSON creation: JSON sent are of the format: {key1:value1,key2:value2} and so on
         
          str.reset();          // Reset json string      
          str.print("/input/post.json?");
          str.print(F("apikey="));   str.print(apikey);
          str.print(F("&node="));    str.print(node_id);
          str.print(F("&json={RF:"));            str.print(0);                            // RF recieved so no failure
         
          str.print(F(",THW:"));      str.print(Jeeheater.hotwater/100.0);     // hotwater tap
          str.print(F(",THO:"));      str.print(Jeeheater.heater_out/100.0);   
          str.print(F(",THI:"));      str.print(Jeeheater.heater_in/100.0);    // heater watertemp in
          str.print(F(",JHB:"));      str.print(Jeeheater.battery);            // Jeeheater battery voltage reading
          str.print("}\0");  //  End of json string
                   
          ether.browseUrl(PSTR(""),str.buf, website, my_callback );
        } // end if node 10 JEEHEATER
      
         
         if (node_id > 10)
      {
        byte n = rf12_len;
         
        str.reset();
        str.print("/input/post.json?");
        str.print(F("apikey="));   str.print(apikey);
        str.print(F("&node="));    str.print(node_id);
        str.print("&csv=");
        for (byte i=0; i<n; i+=2)
        {
          int num = ((unsigned char)rf12_data[i+1] << 8 | (unsigned char)rf12_data[i]);
          if (i) str.print(",");
          str.print(num);
        }

        str.print("\0");  //  End of json string
         flashLed(30); flashLed(40); flashLed(20);// flash the blue led a few times
          ether.browseUrl(PSTR(""),str.buf, website, my_callback );

         
      } // end if other nodes
         
             
       
    }
      
}  // end receive radio
  } // END void radio
  
// ----------------------------
void flashLed (int i) {
  digitalWrite(BlueLED, !1);
  delay(i);
  digitalWrite(BlueLED, !0);
  delay(i);
  digitalWrite(BlueLED, !1);
  delay(i);
  digitalWrite(BlueLED, !0);
}

void flashLed_green (int i) {
  digitalWrite(greenLED, !1);
  delay(i);
  digitalWrite(greenLED, !0);
  delay(i);
  digitalWrite(greenLED, !1);
  delay(i);
  digitalWrite(greenLED, !0);
}


