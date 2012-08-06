Sensor_base
===========

 _   _                           _       ______ ______                                            _      _        _      _ 
| \ | |                         | |      | ___ \|  ___|                                          | |    (_)      | |    | |
|  \| |  __ _  _ __    ___    __| |  ___ | |_/ /| |_     ___   ___  _ __   ___   ___   _ __  ___ | |__   _   ___ | |  __| |
| . ` | / _` || '_ \  / _ \  / _` | / _ \|    / |  _|   / __| / _ \| '_ \ / __| / _ \ | '__|/ __|| '_ \ | | / _ \| | / _` |
| |\  || (_| || | | || (_) || (_| ||  __/| |\ \ | |     \__ \|  __/| | | |\__ \| (_) || |   \__ \| | | || ||  __/| || (_| |
\_| \_/ \__,_||_| |_| \___/  \__,_| \___|\_| \_|\_|     |___/ \___||_| |_||___/ \___/ |_|   |___/|_| |_||_| \___||_| \__,_|

Relay's data recieved from the local sensorshield to COSM and EMONCMS3
Relay's temperature data recieved from emonGLCD 
Decodes reply from COSM server to set software real time clock
Relay's time data to emonglcd - and any other listening nodes.
Looks for 'ok' reply from http request to verify data reached COSM and EMONCMS

emonBase Documentation: http://openenergymonitor.org/emon/emonbase

Authors: Trystan Lea and Glyn Hudson
First COSM modification by Roger James
Arduino hardware sensorshield designed by Martin Harizanov
Sensorshield readings by Alco van Neck read http://www.bluemotica.nl/en/arduino-projects for details about this sensorshield build

Part of the: openenergymonitor.org project
Licenced under GNU GPL V3
http://openenergymonitor.org/emon/license

EtherCard Library by Jean-Claude Wippler and Andrew Lindsay
JeeLib Library by Jean-Claude Wippler
=====================================================================================================================================================================
CHANGELOG
=====================================================================================================================================================================

2012-08-06

Version 1.000
- Complete rewrite of the code major change
- now working with EMONCMS v3 and COSM-website (aka patchub) for posting you measurements
- reordered all the includes and define statements
- rewrite of posting code to JSON strings
- Crash-watchdog fixed
- Added 2 more temperature sensors, LDR (lightsensor) and Magnetic Hall effect sensors.
- Ethercard libary changes from JCW patched
- Small edit of tcip.cpp file for beter performance. The start/ending of a TCP-session is improved (see JCW's readme the libary for more info)
- NO EMONGLCD OR RM12B clients at my setup..so didn't test that part!

N.B. This version needs an edited version of the ethercard library (date: 2010-05-20) with tcp_client_state made non static in tcpip.cpp.
alter the tcip.cpp file line 32 with an // command out [//static byte tcp_client_state;]


