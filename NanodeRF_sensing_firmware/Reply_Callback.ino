#include <Arduino.h>

void send_emoncms() {

    // Send sensor data to the server:
    ethernet_requests ++; // count time request pingback from server with no answer
    ether.browseUrl(PSTR(""),str.buf, website, my_callback );
    //ether.browseUrl(PSTR(""),str.buf, website, browseUrlCallback1 );

    #ifdef SERIAL_PRINT
    Serial.println(F("Data: ")); Serial.println(str.buf); // print to serial json string 
    //Serial.print(ct1.Irms);Serial.println(ct2.Irms);
    #endif

 if (ethernet_requests > 25) delay(10000); // Reset the nanode if more than 25 request attempts have been tried without a reply

  } // end void
//**********************************************************************************************************************

//-----------------------------------------------------------------------------------
// Ethernet callback
// recieve reply and decode
//-----------------------------------------------------------------------------------



static void my_callback (byte status, word off, word len) {

  // get_reply_data gets the data part of the reply and puts it in the line_buf char array
  int lsize =   get_reply_data(off);
  
  // The format of the time reply is: t12,43,12 (t for time, 12h 43mins 12seconds)
  // char 0 is the character t (by asking if it is indeed t we can indentify the reply)
    if (strcmp(line_buf,"ok")==0)
  {
    Serial.println(F("OK")); ethernet_requests = 0;  // print OK and reset ethernet request counter
    flashLed(200);  
  }

}

int get_reply_data(word off)
{
  memset(line_buf,NULL,sizeof(line_buf));
  if (off != 0)
  {
    uint16_t pos = off;
    int line_num = 0;
    int line_pos = 0;
    
    // Skip over header until data part is found
    while (Ethernet::buffer[pos]) {
      if (Ethernet::buffer[pos-1]=='\n' && Ethernet::buffer[pos]=='\r') break;
      pos++; 
    }
    pos+=2;
    while (Ethernet::buffer[pos])
    {
      if (line_pos<49) {line_buf[line_pos] = Ethernet::buffer[pos]; line_pos++;} else break;
      pos++; 
    }
    line_buf[line_pos] = '\0';
    return line_pos;
  }
  return 0;
}

///////////**************


static void browseUrlCallback1 (byte status, word off, word len) 
{
   Ethernet::buffer[off+len] = 0;// set the byte after the end of the buffer to zero to act as an end marker (also handy for displaying the buffer as a string)
   
   Serial.println("Callback 1");
   Serial.println((char *)(Ethernet::buffer+off));

 }
