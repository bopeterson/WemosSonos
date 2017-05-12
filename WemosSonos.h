#ifndef WemosSonos_h
#define WemosSonos_h

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>

//#define SNSESP_BUFSIZ 512
//bufsize with 10k needed if full xml device description must fit
#define SNSESP_BUFSIZ 10000
#define SNSESP_MAXNROFDEVICES 10


class WemosSonos {
  public:
    WemosSonos();
    void removeAllTracksFromQueue(int device); 
    void pause(int device);
    void play(int device);
    byte getVolume(int device);
    void setVolume(byte vol,int device);
    String getTransportInfo(int device);
    int discoverSonos(int timeout);
    int getNumberOfDevices();
    IPAddress getIpOfDevice(int device);
    
    
    
    String roomName(int device);

  

  private:
    char _response[SNSESP_BUFSIZ];
    char _filtered[256];
    IPAddress _deviceIPs[SNSESP_MAXNROFDEVICES];
    WiFiClient _client;
    int _numberOfDevices;
    
    void sonosAction(const char *url, const char *service, const char *action, const char *arguments,int device);
    void filter(const char *starttag,const char *endtag);
    int string2int(const char *s);
    bool addIp(IPAddress ip);
    void deviceInfoRaw(const char *url,int device);
};

#endif


