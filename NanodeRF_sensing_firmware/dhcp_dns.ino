
static int dhcp_status;
static int dns_status_cosm;
static int dns_status_emon;
byte cosmip[] = {0, 0, 0, 0 };
byte emonip[] = {0, 0, 0, 0 };

//Domain name of remote webservers 
char website_cosm[] PROGMEM = "api.cosm.com";
char website_emon[] PROGMEM = "vis.openenergymonitor.org";
void initialise_dhcp_dns()
{
  dhcp_status = 0;
  dns_status_cosm = 0;
  dns_status_emon = 0;
}

void dhcp_dns()
{
  //-----------------------------------------------------------------------------------
  // Get DHCP address
  // Putting DHCP setup and DNS lookup in the main loop allows for: 
  // powering nanode before ethernet is connected
  //-----------------------------------------------------------------------------------
 if (!ether.dhcpValid()) dhcp_status = 0;    // if dhcp expired start request for new lease by changing status  
  if (!dhcp_status){
    
    #ifdef UNO
    wdt_disable();
    #endif 
    
    dhcp_status = ether.dhcpSetup();           // DHCP setup
    
    #ifdef UNO
    wdt_enable(WDTO_8S);
    #endif
    
    Serial.print("DHCP status: ");             // print
    Serial.println(dhcp_status);               // dhcp status
    
    if (dhcp_status){                          // on success print out ip's
      ether.printIp("IP:  ", ether.myip);
      ether.printIp("GW:  ", ether.gwip);  
      
      ether.printIp("DNS: ", ether.dnsip);
    } else { ethernet_error = 1; }  
  }
  
  //-----------------------------------------------------------------------------------
  // Get server address via DNS
  //-----------------------------------------------------------------------------------
  if (dhcp_status && !dns_status_cosm){
    
    #ifdef UNO
    wdt_disable();
    #endif 
    
    dns_status_cosm = ether.dnsLookup(website_cosm);    // Attempt DNS lookup
    
    #ifdef UNO
    wdt_enable(WDTO_8S);
    #endif;
    
    Serial.print("DNS status cosm: ");             // print
    Serial.println(dns_status_cosm);               // dns status
    if (dns_status_cosm){
      ether.printIp("SRV: ", ether.hisip);      // server ip
      ether.copyIp(cosmip, ether.hisip);
    } else { ethernet_error = 1; }  
  }
  
  if (dhcp_status && !dns_status_emon){
    
    #ifdef UNO
    wdt_disable();
    #endif 
    
    dns_status_emon = ether.dnsLookup(website_emon);    // Attempt DNS lookup
    
    #ifdef UNO
    wdt_enable(WDTO_8S);
    #endif;
    
    Serial.print("DNS status emon: ");             // print
    Serial.println(dns_status_emon);               // dns status
    if (dns_status_emon){
      ether.printIp("SRV: ", ether.hisip);      // server ip
      ether.copyIp(emonip, ether.hisip);
    } else { ethernet_error = 1; }  
  }

}
