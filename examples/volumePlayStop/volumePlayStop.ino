/*
   connections

   Wemos   Rotary encoder (Keyes KY-040)
   GND     GND                BLACK
   5V      +                  RED
   D3      CLK - pin A        GREEN
   D6      SW - push button   WHITE
   D7      DT - pin B         YELLOW

*/

/* instructions fot setup:

press and hold button for 5 seconds
connect a computer or phone to the wifi network "configure"
browse to http://192.168.4.1
connect to the wifi network with sonos devices
select which sonos device you want to control
switch back your computer to your normal wifi network
finished!
 */

#include <WemosButton.h>
#include <WemosSonos.h>
#include <WemosSetup.h>

#include "settings.h"

//length must not exceed WFS_MAXBODYLENGTH in WemosSetup.cpp, currently 1500
#define MAXBODYCHLENGTH 1024

int noOfDiscoveries = 0;
bool discoveryInProgress = false;
int device = -1;
unsigned long timeToFirstDiscovery = 0;

const char compiletime[] = __TIME__;
const char compiledate[] = __DATE__;

WemosSetup wifisetup;
WemosSonos sonos;
WemosButton knobButton;


const unsigned long checkRate =  1000 * 30; //how often main loop performs periodical task. NOTE-this also updates read volume
unsigned long lastPost = 0;
int discoverSonosTimeout = 9; //seconds

//encoder variables
const int pinA = D3;  // Connected to CLK on KY-040
const int pinB = D7;  // Connected to DT on KY-040
int encoderPosCount = 0;
int pinALast;
int aVal;
int encoderRelativeCount;
unsigned long moveTime;

int oldVolume; //used for readKnob

unsigned long periodicLast = 0;


String roomNames[SNSESP_MAXNROFDEVICES];

void setup() {
  settings.load();
  knobButton.begin(D6);

  Serial.begin(115200);
  wfs_debugprintln("");
  wfs_debugprint("compiletime: ");
  wfs_debugprint(compiletime);
  wfs_debugprint(" ");
  wfs_debugprintln(compiledate);

  //testing settings
  wfs_debugprint("Saved room name: ");
  wfs_debugprintln(settings.roomname);

  //encoder setup
  pinMode (pinA, INPUT);
  pinMode (pinB, INPUT);
  pinALast = digitalRead(pinA);

  wifisetup.begin(WIFI_STA, 0, BUILTIN_LED);//try to connect

  wifisetup.afterConnection("/search");

  wifisetup.server.on("/search", handleSearch);
  wifisetup.server.on("/setup", handleSetup);

  unsigned long timelimit = 20000;
  unsigned long starttimer = millis();
  bool timeout = false;
  while (!wifisetup.connected() && !timeout) {
    wifisetup.inLoop();
    unsigned long loopStart = millis();
    if (lastPost + checkRate <= loopStart) {
      lastPost = loopStart;
      wifisetup.printInfo(); //WFS_DEBUG to be true in WemosSetup.h for this to work
    }
    if ((loopStart - starttimer) > timelimit) {
      wfs_debugprintln("");
      wfs_debugprintln("trying to connect timed out");
      timeout = true;
    }
  }

  if (wifisetup.connected()) {
    discoverSonos(discoverSonosTimeout);
  }
}

void loop() {
  unsigned long loopStart = millis();

  wifisetup.inLoop();

  //do a new search if saved room wasn't dicovered in setup
  if (device == -1 && settings.roomname != "" && noOfDiscoveries != 0) {
    noOfDiscoveries = 0; //not so logical to set it to zero, but think of it as a new start
  }
  //set timer for first discovery if no network was available in setup, or saved room wasn't discovered in setup
  if (noOfDiscoveries == 0) {
    if (timeToFirstDiscovery == 0 && wifisetup.connected() && !discoveryInProgress) {
      timeToFirstDiscovery = millis() + 30000;
    }
  }
  //execute the first discovery
  if (timeToFirstDiscovery != 0) {
    if (noOfDiscoveries == 0 && millis() > timeToFirstDiscovery) {
      timeToFirstDiscovery = 0;
      wfs_debugprintln("discover sonos 2 (in main loop)");
      discoverSonos(discoverSonosTimeout);
    }
  }

  if (lastPost + checkRate <= loopStart) {
    lastPost = loopStart;
    //put any code here that should run periodically
      wifisetup.printInfo();
  
    //update oldVolume regularly, as other devices might change the volume as well
    if (device >= 0) {
      oldVolume = sonos.getVolume(device);
    }
  }

  //check if knob has been turned
  oldVolume = readKnob(oldVolume);

  byte knobButtonStatus = knobButton.readButtonAdvanced(5000);
  if ((knobButtonStatus & knobButton.HOLD_DETECTED)) {
    wfs_debugprintln("hold detected");
    wifisetup.switchToAP_STA();
    wifisetup.timeToChangeToSTA = millis() + 5 * 60 * 1000;
  } else if (knobButtonStatus & knobButton.PRESS_DETECTED) {
    wfs_debugprintln("button pressed");
    if (device >= 0) {
      togglePlay(device);
    }
  }
} //loop

void togglePlay(int device) {
  //if the device is a stand alone device, togglePlay is as simple as checking transport info.
  //If it is playing, pause it, if not, play it. However, if the device is controlled by a
  //coordinator, it is much more complicated. The procedure below starts to assume that the
  //device is stand alone, and tries to pause or play. If it does not work, it checks for
  //the coordiniator, and pauses or plays the coordinator instead. Each step is explained.

  //step 1: check transport info of device
  String deviceTransportInfo = sonos.getTransportInfo(device);
  String deviceTransportInfoAfter;
  String coordinatorTransportInfo;

  //step 2: check if status is playing
  if (deviceTransportInfo == "PLAYING") {
    //step 3: try to pause device if it is playing
    sonos.pause(device);
    deviceTransportInfoAfter = sonos.getTransportInfo(device);
    if (deviceTransportInfoAfter == "PAUSED_PLAYBACK") {
      //step 4: if it succeded, everything is done
    } else {
      //step 5: if not, check who is the coordinator
      int coordinator = sonos.getCoordinator(device);
      //step 6: check transport info of coordinator
      if (coordinator >= 0) {
        coordinatorTransportInfo = sonos.getTransportInfo(coordinator);
      } else {
        coordinatorTransportInfo = "UNKNOWN";
      }
      if (coordinatorTransportInfo == "PLAYING") {
        //step 7: pause coordinator if it is playing
        sonos.pause(coordinator);
      } else  if (coordinatorTransportInfo == "PAUSED_PLAYBACK" || coordinatorTransportInfo == "STOPPED") {
        //step 8: play coordinator instead, if it is paused or stopped
        sonos.play(coordinator);
      }
    }
  } else if (deviceTransportInfo == "PAUSED_PLAYBACK") {
    //step 9: try to play device if it is paused
    sonos.play(device);
    deviceTransportInfoAfter = sonos.getTransportInfo(device);
    if (deviceTransportInfoAfter == "PLAYING") {
      //step 10: if it succeded, everything is done 
    } else {
      //step 11: if not, something probably went wrong, maybe there is no queue to play
    }
  } else if (deviceTransportInfo == "STOPPED") {
    //step 12: try to play device if is stopped
    sonos.play(device);
    //step 13: check if device has coordinator, because it must play too
    int coordinator = sonos.getCoordinator(device);
    if (coordinator < 0) {
      //step 14: if there is NO coordinator, everything is done
      //done, no need to do anything
    } else {
      //step 15: check transport info of coordinator if ther is one
      coordinatorTransportInfo = sonos.getTransportInfo(coordinator);
      if (coordinatorTransportInfo == "PLAYING") {
        //step 16: if coordinator is playing, everything is done. However, it might take several minutes before the device starts playing
      } else if (coordinatorTransportInfo == "PAUSED_PLAYBACK" || coordinatorTransportInfo == "STOPPED") {
        //step 17. play coordinator if it paused or stopped
        sonos.play(coordinator);
      }
    }
  } else {
    //step 18: something went wrong, this should not happen
  }
}

void clearSettings() {
  //fot testing
  wfs_debugprintln("Clearing settings");
  settings.roomname = "";
  settings.roomno = -1;
  device = -1;
  settings.ssid = "";
  settings.psk = "";
  settings.save();
  WiFi.begin("foooooooooo", "");
}

void handleSearch() {
  wfs_debugprintln("handleSearch - searches for Sonos devices");

  long reloadTime = discoverSonosTimeout * 1000 + 4000 + 5000;
  sprintf(wifisetup.onload, "i1=setInterval(function() {location.href='/setup';},%i)", reloadTime);
  sprintf(wifisetup.body, "<h2>Wifi connection</h2><p>Searching for Sonos devices...</p>");
  wifisetup.sendHtml(wifisetup.body, wifisetup.onload);
  //give time to show page before discovery
  for (int i = 0; i < 2000; i++) { //2000 takes ca 4 s xxx could be made into a delay function
    wifisetup.inLoop();
  }
  wfs_debugprintln("discover sonos 3 (in handleSearch)");
  discoverSonos(discoverSonosTimeout);
  wifisetup.timeToChangeToSTA = millis() + 10 * 60 * 1000;
}

void handleSetup() {
  wfs_debugprintln("handleSetup");
  char bodych[MAXBODYCHLENGTH];
  int selectedDevice = -1;

  if (wifisetup.server.hasArg("room")) {
    String roomStr = wifisetup.server.arg("room");
    device = roomStr.toInt();

    if (device >= 0 && sonos.getNumberOfDevices() > device) {
      settings.roomname = roomNames[device];
      wfs_debugprintln(settings.roomname);
      wfs_debugprint("Saving settings... ");
      settings.save();
    }

    wifisetup.timeToChangeToSTA = millis() + 5 * 60 * 1000;

    wfs_debugprint("setting device to ");
    wfs_debugprintln(device);
  }

  String body;
  body = "<h2>Wifi connection</h2>";
  if (WiFi.SSID().length() == 0) {
    body = body + "<p>No network selected</p>";
  } else {
    body = body + "<p>Selected network: ";
    body = body + WiFi.SSID(); + "</p>";
  }
  body = body + "<p>Connection status: ";
  if (WiFi.status() != WL_CONNECTED) {
    body = body + "not connected</p>";
  } else {
    body = body + "connected</p>";
  }
  body = body + "<p><a href='/'>Change network</a></p>";
  body = body + "<h2>Sonos devices</h2>";
  if (sonos.getNumberOfDevices() == 0) {
    body = body + "<p>No sonos devices found</p>";
  } else {
    body = body + "<p>Click to select Sonos device: <ul>";
    for (int i = 0; i < sonos.getNumberOfDevices(); i++) {
      if (body.length() < MAXBODYCHLENGTH - 300) { //truncate if very many devices
        body = body + "<li><a href=setup?room=" + i + ">" + roomNames[i] + "</a>";
        if (roomNames[i] == settings.roomname && roomNames[i] != "") {
          body = body + " (selected)";
          selectedDevice = i;
        }
        body = body + "</li>";
      }
    }
    body = body + "</ul></p>";
  }
  if (selectedDevice == -1) {
    body = body + "<p>No Sonos device selected</p>";
  }
  body = body + "<p><a href='/search'>Search for Sonos devices</a></p>";

  body.toCharArray(bodych, MAXBODYCHLENGTH);
  sprintf(wifisetup.body, "%s", bodych);
  sprintf(wifisetup.onload, "");
  wifisetup.sendHtml(wifisetup.body, wifisetup.onload);
}

void discoverSonos(int timeout) {
  if (!discoveryInProgress) {
    discoveryInProgress = true;
    noOfDiscoveries++;
    wifisetup.stopWebServer();//unpredicted behaviour if weserver contacted during discovery multicast
    wfs_debugprintln("discover sonos in library");
    sonos.discoverSonos(timeout);
    wifisetup.startWebServer();
    wfs_debugprint("Found devices: ");
    wfs_debugprintln(sonos.getNumberOfDevices());
    for (int i = 0; i < sonos.getNumberOfDevices(); i++) {
      wfs_debugprint("Device ");
      wfs_debugprint(i);
      wfs_debugprint(": ");
      wfs_debugprint(sonos.getIpOfDevice(i));
      wfs_debugprint(" ");
      String roomName = sonos.roomName(i);
      roomNames[i] = roomName;
      wfs_debugprint(roomName);
      if (roomName == settings.roomname && roomName != "") {
        wfs_debugprint(" *");
        device = i;
      }
      wfs_debugprintln("");
    }
    wfs_debugprintln("discoverReady");
    //wifisetup.startWebServer();
    discoveryInProgress = false;
  }
}


int readKnob(int oldVolume) {
  //polling encoder rather than using interrupts seems to work good enough
  int newVolume = oldVolume;

  aVal = digitalRead(pinA);
  if (aVal != pinALast) { // Means the knob is rotating
    moveTime = millis() + 150;
    // if the knob is rotating, we need to determine direction
    // We do that by reading pin B.
    if (digitalRead(pinB) != aVal) {  // Means pin A Changed first - We're Rotating Clockwise
      encoderPosCount++;
      encoderRelativeCount++;
    } else {// Otherwise B changed first and we're moving CCW
      encoderPosCount--;
      encoderRelativeCount--;
    }

    wfs_debugprint("Encoder Position: ");
    wfs_debugprintln(encoderPosCount);

  }
  pinALast = aVal;

  if (millis() > moveTime && moveTime != 0) {
    if (encoderRelativeCount == 2 || encoderRelativeCount == -2) {
      //for finetuning of volume
      newVolume = oldVolume + 0.5 * encoderRelativeCount;
    } else {
      newVolume = oldVolume + floor(2 * 0.5 * (encoderRelativeCount + 0.5));
    }
    newVolume = constrain(newVolume, 0, 100);

    wfs_debugprint("Encoder change: ");
    wfs_debugprintln(encoderRelativeCount);
    wfs_debugprint("Volume change;  ");
    wfs_debugprintln(newVolume - oldVolume);
    wfs_debugprint("Volume:         ");
    wfs_debugprintln(newVolume);
    if (device >= 0) {
      sonos.setVolume(newVolume, device);
    }
    moveTime = 0;
    encoderRelativeCount = 0;
  }
  return newVolume;
}

