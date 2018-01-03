#include "Arduino.h"
#include "WemosSonos.h"
#include<string.h>

WemosSonos::WemosSonos() {
    _numberOfDevices=0;
}

void WemosSonos::pause(int device) {
    const char url[] = "/AVTransport/Control";
    const char service[] = "AVTransport:1";
    const char action[] = "Pause";
    const char arguments[] = "<InstanceID>0</InstanceID>";
    sonosAction(url,service,action,arguments,device);
}

void WemosSonos::stop(int device) {
    const char url[] = "/AVTransport/Control";
    const char service[] = "AVTransport:1";
    const char action[] = "Stop";
    const char arguments[] = "<InstanceID>0</InstanceID>";
    sonosAction(url,service,action,arguments,device);
}

void WemosSonos::play(int device) {
    const char url[] = "/AVTransport/Control";
    const char service[] = "AVTransport:1";
    const char action[] = "Play";
    const char arguments[] = "<InstanceID>0</InstanceID><Speed>1</Speed>";
    sonosAction(url,service,action,arguments,device);
}

byte WemosSonos::getVolume(int device) {
    //returns zero at timeout
    const char url[] = "/RenderingControl/Control";
    const char service[] = "RenderingControl:1";
    const char action[] = "GetVolume";
    const char arguments[] = "<InstanceID>0</InstanceID><Channel>Master</Channel>";
    sonosAction(url,service,action,arguments,device);
    filter("<CurrentVolume>","</CurrentVolume>");
    
    return string2int(_filtered);
}

String WemosSonos::getTransportInfoTrans(int device) {
    //returns STOPPED, PLAYING, PAUSED_PLAYBACK or TRANSITIONING
    const char url[] = "/AVTransport/Control";
    const char service[] = "AVTransport:1";
    const char action[] = "GetTransportInfo";
    const char arguments[] = "<InstanceID>0</InstanceID>";
    sonosAction(url,service,action,arguments,device);
    filter("<CurrentTransportState>","</CurrentTransportState>");
    return String(_filtered);
}

String WemosSonos::getTransportInfo(int device) {
    //returns STOPPED, PLAYING or PAUSED_PLAYBACK (but not TRANSITIONING)
    const char url[] = "/AVTransport/Control";
    const char service[] = "AVTransport:1";
    const char action[] = "GetTransportInfo";
    const char arguments[] = "<InstanceID>0</InstanceID>";
    sonosAction(url,service,action,arguments,device);
    filter("<CurrentTransportState>","</CurrentTransportState>");
    while(strcmp(_filtered,"TRANSITIONING")==0) {
        sonosAction(url,service,action,arguments,device);
        filter("<CurrentTransportState>","</CurrentTransportState>");
        delay(100);
    }
    return String(_filtered);
}

void WemosSonos::removeAllTracksFromQueue(int device) { 
    const char url[] = "/AVTransport/Control";
    const char service[] = "AVTransport:1";
    const char action[] = "RemoveAllTracksFromQueue";
    const char arguments[] = "<InstanceID>0</InstanceID>";
    sonosAction(url,service,action,arguments,device);
}

int WemosSonos::string2int(const char *s) {
    //returns zero if string is empty
    char *ptr; //not used but needed in strtol
    int ret = strtol(s, &ptr, 10);      
    return ret;
}

void WemosSonos::filter(const char *starttag,const char *endtag) {
    //looks in global var _response for content between starttag and endtag and "returns" it in global var _filtered
    //check for starttag
    char * startpart=strstr(_response,starttag);
    char * endpart;
    if (startpart!=0) {
      //check for endtag if starttag exists
      endpart=strstr(startpart+strlen(starttag),endtag);
    }
    int startindex=startpart-_response+strlen(starttag);
    int endindex=endpart-_response;
    if (startpart!=0 && endpart!=0 && endindex>startindex) {
        strncpy(_filtered,_response+startindex,endindex-startindex);
        _filtered[endindex-startindex]='\0';
    } else {
        _filtered[0]='\0'; //empty string
    }
}

void WemosSonos::setVolume(byte vol,int device) {
    const char url[] = "/RenderingControl/Control";
    const char service[] = "RenderingControl:1";
    const char action[] = "SetVolume";
    char arguments[512];
    sprintf(arguments, "<InstanceID>0</InstanceID><Channel>Master</Channel><DesiredVolume>%d</DesiredVolume>", vol);  
    sonosAction(url,service,action,arguments,device);
}

String WemosSonos::roomName(int device) {
    const char starttag[]="<roomName>";
    const char endtag[]="</roomName>";
    return getDeviceDesctiptionTagContent(starttag,endtag,device);
}

String WemosSonos::getDeviceDesctiptionTagContent(const char *starttag,const char *endtag,int device) {
    //xxx consider replacing _filtered with _response
    const char url[] = "/xml/device_description.xml";
    
    IPAddress deviceIP = getIpOfDevice(device);
    if (_client.connect(deviceIP, 1400)) {
        _client.print("GET ");
        _client.print(url);
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(deviceIP); //note: device is of type IPAddress which converts to int
        _client.println("");
    }
    
    //wait if _client not immediately available
    unsigned long starttimer=millis();
    unsigned long timelimit=5000; //Used to be 12000
    bool timeout = false;
    while (!_client.available() && !timeout) {
        //wait until _client available
        delay(100);
        if ((millis() - starttimer) > timelimit) {
            timeout=true;
        }
    }
    //timeout shall keep current state for the coming while-loop. Only starttimer should be updated

    timelimit=10000;
    starttimer=millis();
    int index=0;
    bool betweenCR=false;
    bool firstLtRead=false;
    bool headerRead=false;
    bool found=false;
    for (int i=0;i<SNSESP_FILTSIZ-1;i++) {
      _filtered[i]=' ';
    }
    _filtered[SNSESP_FILTSIZ-1]='\0';
  
    while (_client.available()  && !timeout && !found) {
        char c = _client.read();
        if (c=='<' && !firstLtRead) {
            firstLtRead=true;
        }
        if (c=='?' && firstLtRead && !headerRead) {
            headerRead=true;
            _filtered[SNSESP_FILTSIZ-2]='<';
            index++;
        }
        
        if (headerRead) {
            if ((index % 100)==0) {
                delay(1); //a short delay (or yield?) needed now and then, otherwise all characters won't be read
            }
            if (c=='\r') {
                if (betweenCR) {
                    char dropLF=_client.read(); //drop a linefeed \n (10) after CR \r (13) and move on to next character
                }
                betweenCR=!betweenCR;
            }
        
            if (!betweenCR && c!='\r') {
                memcpy(&_filtered[0], &_filtered[1], SNSESP_FILTSIZ-1);//shift array one step left
                _filtered[SNSESP_FILTSIZ-2]=c; //add read character at the end
                if (c=='>') {
                    //search for tags
                    char * startpart=strstr(_filtered,starttag);
                    char * endpart=strstr(_filtered,endtag);

                    if (startpart!=0 && endpart!=0) {
                        int startindex=startpart-_filtered+strlen(starttag);
                        int endindex=endpart-_filtered;
 
                        strncpy(_filtered,_filtered+startindex,endindex-startindex);
                        _filtered[endindex-startindex]='\0';
                        found=true;
                    }
                    
                }
                index++;
            }
        }
        
        if ((millis() - starttimer) > timelimit) {
            timeout=true;
        }
    }
    if (timeout) {
        _client.stop();
    }
    
    if (!found) {
        _filtered[0]='\0'; //empty string
    }
 
    return String(_filtered);
}

/*
String WemosSonos::roomName(int device) {
    //this function requires a large _filtered buffer. It has been replaced
    //by a more economical one
    
    const char url[] = "/xml/device_description.xml";
    deviceInfoRaw(url,device);
    filter("<roomName>","</roomName>");

    return String(_filtered);
}
*/

int WemosSonos::getCoordinator(int device) {
    const char url[] = "/status/topology";

//    const char starttag[]="<ZonePlayer ";
//    const char endtag[]="</ZonePlayer>";

    
    IPAddress deviceIP = getIpOfDevice(device);
    IPAddress loopIP;
    if (_client.connect(deviceIP, 1400)) {
        _client.print("GET ");
        _client.print(url);
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(deviceIP); //note: device is of type IPAddress which converts to int
        _client.println("");
    }
    
    //wait if _client not immediately available
    unsigned long starttimer=millis();
    unsigned long timelimit=5000; //Used to be 12000
    bool timeout = false;
    while (!_client.available() && !timeout) {
        //wait until _client available
        delay(100);
        if ((millis() - starttimer) > timelimit) {
            timeout=true;
        }
    }
    //timeout shall keep current state for the coming while-loop. Only starttimer should be updated

    timelimit=10000;
    starttimer=millis();
    int index=0;
    bool betweenCR=false;
    bool firstLtRead=false;
    bool headerRead=false;
    int foundDevices=0;

    for (int i=0;i<SNSESP_BUFSIZ-1;i++) {
       _response[i]=' ';
    }
    _response[SNSESP_BUFSIZ-1]='\0';
      
    while (_client.available()  && !timeout) {
        char c = _client.read();
        if (c=='<' && !firstLtRead) {
            firstLtRead=true;
        }
        if (c=='?' && firstLtRead && !headerRead) {
            headerRead=true;
            _response[SNSESP_BUFSIZ-2]='<';
            index++;
        }
        
        if (headerRead) {
            if ((index % 100)==0) {
                delay(1); //a short delay (or yield?) needed now and then, otherwise all characters won't be read
            }
            if (c=='\r') {
                if (betweenCR) {
                    char dropLF=_client.read(); //drop a linefeed \n (10) after CR \r (13) and move on to next character
                }
                betweenCR=!betweenCR;
            }
        
            if (!betweenCR && c!='\r') {
                memcpy(&_response[0], &_response[1], SNSESP_BUFSIZ-1);//shift array one step left
                _response[SNSESP_BUFSIZ-2]=c; //add read character at the end
                if (c=='>') {
                    
                    filter("<ZonePlayer ","</ZonePlayer>");
                    
                    if (strlen(_filtered)>0) {
                        foundDevices++;
                        //copy back filtered to response as it will be further filtered
                        strcpy(_response,_filtered);    
                        filter("location='http://",":1400");
                        int loopDeviceNumber=-1;
                        bool found=false;
                        for (int i=0;i<getNumberOfDevices() && !found;i++) {
                            if ((String)_filtered==getIpOfDevice(i).toString()) {
                                found=true;
                                loopDeviceNumber=i;
                            }
                        }
                        
                        
                        filter("group='","'");
                        _group[loopDeviceNumber]=String(_filtered);
                        filter("coordinator='","'");
                        _isCoordinator[loopDeviceNumber]=(strcmp(_filtered,"true")==0);
                        
                        //clear buffer
                        for (int i=0;i<SNSESP_BUFSIZ-1;i++) {
                          _response[i]=' ';
                        }
                    }
                    
                }
                index++;
            }
        }
        
        if ((millis() - starttimer) > timelimit) {
            timeout=true;
        }
    }
    if (timeout) {
        _client.stop();
    }
    
    for (int thisDevice=0;thisDevice<getNumberOfDevices();thisDevice++) {        
        _myCoordinator[thisDevice]=-1;
        if (!_isCoordinator[thisDevice]) {
            for (int otherDevice=0;otherDevice<getNumberOfDevices();otherDevice++) {
                if (_group[thisDevice]==_group[otherDevice]) {
                    _myCoordinator[thisDevice]=otherDevice;
                }
            }
        }
    }
    
 
    return _myCoordinator[device];
}



void WemosSonos::sonosAction(const char *url, const char *service, const char *action, const char *arguments,int device) {
    //length of response typically 400-600 characters
    IPAddress deviceIP = getIpOfDevice(device);
    if (_client.connect(deviceIP, 1400)) {
        _client.print("POST /MediaRenderer");
        _client.print(url); //skiljer
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(deviceIP); //note: device is of type IPAddress which converts to int
        _client.print("Content-Length: "); //samma
        int contlen = 124+8+3+strlen(action)+39+strlen(service)+2+strlen(arguments)+4+strlen(action)+1+9+13;
        _client.println(contlen);
        _client.println("Content-Type: text/xml; charset=utf-8");
        _client.print("Soapaction: ");
        _client.print("urn:schemas-upnp-org:service:");
        _client.print(service);
        _client.print("#");
        _client.println(action);
        _client.println("Connection: close"); //samma
        _client.println(""); //samma
        _client.print("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" ");
        _client.print("s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">");
        _client.print("<s:Body>");
        _client.print("<u:");
        _client.print(action);
        _client.print(" xmlns:u=\"urn:schemas-upnp-org:service:");
        _client.print(service);
        _client.print("\">");
        _client.print(arguments);
        _client.print("</u:");
        _client.print(action);
        _client.print(">");
        _client.print("</s:Body>");
        _client.println("</s:Envelope>");  

        //wait if _client not immediately available
        unsigned long starttimer=millis();
        unsigned long timelimit=10000;
        bool timeout = false;
        while (!_client.available() && !timeout) {
            //wait until _client available
            delay(100);
            if ((millis() - starttimer) > timelimit) {
                timeout=true;
            }
        }
        //timeout shall keep current state for the coming while-loop. Only starttimer should be updated

        starttimer=millis();
        int index=0;

        while (_client.available()  && !timeout) {
            char c = _client.read();
            _response[index]=c;
            index++;
            if (index >= SNSESP_BUFSIZ) { 
                index = SNSESP_BUFSIZ -1;
            }
            if ((millis() - starttimer) > timelimit) {
                timeout=true;
            }
        }
        if (timeout) {
            _client.stop();
        }
        _response[index]='\0'; //end of string
    } else {
        _response[0]='\0';
    }
}

int WemosSonos::discoverSonos(int timeout){
    //timeout in seconds
    //_numberOfDevices=0;
    WiFiUDP Udp;
    Udp.begin(1900);
    IPAddress sonosIP;
    bool timedOut = false;
    unsigned long timeLimit = timeout*1000;
    unsigned long innerLoopTimeLimit = 3000;
    unsigned long firstSearch = millis();
    do {
        Udp.beginPacketMulticast(IPAddress(239, 255, 255, 250), 1900, WiFi.localIP());
        Udp.write("M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
                "MAN: \"ssdp:discover\"\r\n"
                    "MX: 1\r\n"
                        "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n");
        Udp.endPacket();
        unsigned long lastSearch = millis();

        while(((millis() - lastSearch) < innerLoopTimeLimit) && ((millis() - firstSearch) < timeLimit)){
            int packetSize = Udp.parsePacket();
            if(packetSize){
                char packetBuffer[255];
                sonosIP = Udp.remoteIP();

                //if new IP, it should be put in an array
                addIp(sonosIP);
                
                // read the packet into packetBufffer
                int len = Udp.read(packetBuffer, 255);
                if (len > 0) {
                    packetBuffer[len] = 0;
                }
            }
            delay(50);
        }
    } while((millis()-firstSearch)<timeLimit);
    
    return getNumberOfDevices();
}

bool WemosSonos::addIp(IPAddress ip) {
    bool found=false;
    bool added=false;
    if (_numberOfDevices<SNSESP_MAXNROFDEVICES) { //must agree with definition of global array _deviceIPs
        for (int i=0;i<_numberOfDevices && !found;i++) {
            if (ip==_deviceIPs[i]) {
                found=true;     
            }
        }
        if (!found) {
            _deviceIPs[_numberOfDevices]=ip;
            _numberOfDevices++;
            added=true;
        }
    } 
 
    return added;
}


IPAddress WemosSonos::getIpOfDevice(int device) {
    return _deviceIPs[device];
    //IPAddress x=_deviceIPs[0];
}


int WemosSonos::getNumberOfDevices() {
    return _numberOfDevices;
}

IPAddress WemosSonos::string2ip(const char *s) {
  //returns IPAddress from sting containing IPAddress.
  IPAddress ipad;
  char ipstring[16];
  strcpy(ipstring,s);
  for (int i=3;i>=0;i--) {
    int position=0;
    char * numberstring = strrchr(ipstring,'.');
    if (numberstring!=0) {
      position=numberstring-ipstring;
      numberstring=&numberstring[1];
    } else {
      numberstring=ipstring;
    }
    int number=string2int(numberstring);
    ipstring[position]='\0';
    ipad[i]=number;
  }
  return(ipad);
}

/*
void WemosSonos::deviceInfoRaw(const char *url,int device) {
    //disabled - requires too much memory. 
    //use getDeviceDesctiptionTagContent instead
    IPAddress deviceIP = getIpOfDevice(device);
    if (_client.connect(deviceIP, 1400)) {
        _client.print("GET ");
        _client.print(url);
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(deviceIP); //note: device is of type IPAddress which converts to int
        //_client.println("Connection: close");
        _client.println("");
    }
    
    //wait if _client not immediately available
    unsigned long starttimer=millis();
    unsigned long timelimit=5000; //Used to be 12000
    bool timeout = false;
    while (!_client.available() && !timeout) {
        //wait until _client available
        delay(100);
        if ((millis() - starttimer) > timelimit) {
            timeout=true;
        }
    }
    //timeout shall keep current state for the coming while-loop. Only starttimer should be updated

    timelimit=10000;
    starttimer=millis();
    int index=0;
    bool betweenCR=false;
    bool firstLtRead=false;
    bool headerRead=false;
    while (_client.available()  && !timeout) {
        char c = _client.read();
        if (c=='<' && !firstLtRead) {
            firstLtRead=true;
        }
        if (c=='?' && firstLtRead && !headerRead) {
            headerRead=true;
            _response[index]='<';
            index++;
        }
        
        if (headerRead) {
            if ((index % 100)==0) {
                delay(1); //a short delay (or yield?) needed now and then, otherwise all characters won't be read
            }
            if (c=='\r') {
                if (betweenCR) {
                    char dropLF=_client.read(); //drop a linefeed \n (10) after CR \r (13) and move on to next character
                }
                betweenCR=!betweenCR;
            }
        
            if (!betweenCR && c!='\r') {
                _response[index]=c;
                index++;
                if (index >= SNSESP_BUFSIZ) { 
                    index = SNSESP_BUFSIZ-1;
                }
            }
        }
        
        if ((millis() - starttimer) > timelimit) {
            timeout=true;
        }
    }
    if (timeout) {
        _client.stop();
    }
    
    _response[index]='\0'; //end of string
}
*/
