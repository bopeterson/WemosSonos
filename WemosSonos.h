#ifndef WemosSonos_h
#define WemosSonos_h

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>

//#define SNSESP_BUFSIZ 512
//bufsize with 10k needed if full xml device description must fit. hm, too small now...changed to 15k
//xxxx add some directive to swithc between different versions of bufsiz and roomname
//#define SNSESP_BUFSIZ 15000
//#define SNSESP_FILTSIZ 256
#define SNSESP_BUFSIZ 1024
#define SNSESP_FILTSIZ 1024
#define SNSESP_MAXNROFDEVICES 10

class WemosSonos {
  public:
    WemosSonos();
    void removeAllTracksFromQueue(int device); 
    void pause(int device);
    void stop(int device);
    void play(int device);
    byte getVolume(int device);
    void setVolume(byte vol,int device);
    String getTransportInfoTrans(int device);
    String getTransportInfo(int device);
    int discoverSonos(int timeout);
    int getNumberOfDevices();
    IPAddress getIpOfDevice(int device);
    
    int getCoordinator(int device);
    String roomName(int device);

  private:
    char _response[SNSESP_BUFSIZ];
    char _filtered[SNSESP_FILTSIZ];
    IPAddress _deviceIPs[SNSESP_MAXNROFDEVICES];
    String _group[SNSESP_MAXNROFDEVICES];
    bool _isCoordinator[SNSESP_MAXNROFDEVICES];
    int _myCoordinator[SNSESP_MAXNROFDEVICES]; //-1 if device is coordinator, otherwise the device number of the coordinator
    
    WiFiClient _client;
    int _numberOfDevices;
    
    void sonosAction(const char *url, const char *service, const char *action, const char *arguments,int device);
    void filter(const char *starttag,const char *endtag);
    String getDeviceDesctiptionTagContent(const char *starttag,const char *endtag,int device);
    
    int string2int(const char *s);
    IPAddress string2ip(const char *s);
    
    bool addIp(IPAddress ip);
    void deviceInfoRaw(const char *url,int device);
};

#endif

