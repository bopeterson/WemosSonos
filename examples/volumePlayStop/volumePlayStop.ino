/*
   connections

   Wemos   Rotary encoder (Keyes KY-040)
   GND     GND
   5V      +
   D3      CLK - pin A
   D6      SW - push button
   D7      DT - pin B

*/

#include <WemosButton.h>
#include <WemosSonos.h>
#include <WemosSetup.h>

#include "settings.h"

int noOfDiscoveries = 0;
bool discoveryInProgress = false;
int device = -1;
unsigned long timeToFirstDiscovery = 0;

const char compiletime[] = __TIME__;
const char compiledate[] = __DATE__;

WemosSetup wifisetup;
WemosSonos sonos;
WemosButton knobButton;
WemosButton testButton;

const unsigned long checkRate = 15 * 1000; //how often main loop performs periodical task
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

int oldVolume; //xxx used for readKnob2


unsigned long periodicLast = 0;

void setup() {
  settings.load();
  knobButton.begin(D6);
  testButton.begin(D5);

  Serial.begin(115200);
  Serial.println("");
  Serial.print("compiletime: ");
  Serial.print(compiletime);
  Serial.print(" ");
  Serial.println(compiledate);

  //testing settings
  Serial.print("Saved room name>>>");
  Serial.print(settings.roomname);
  Serial.println("<<<");

  //encoder setup
  pinMode (pinA, INPUT);
  pinMode (pinB, INPUT);
  pinALast = digitalRead(pinA);

  wifisetup.begin(WIFI_STA, 0, BUILTIN_LED);//try to connect

  wifisetup.printStringLength();

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
      //wifisetup.printInfo(); //WFS_DEBUG to be true in WemosSetup.h for this to work
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
  Serial.println("entering main loop");
}

void loop() {
  unsigned long loopStart = millis();

  wifisetup.inLoop();

  //xxx om den aldrig gjorde någon discovery i setup för att den inte var connected men senare blivit connected,
  //då ska den typ automatiskt göra en discovery xxx ändra kommentar till engelska

  /*
    xxx två oberoende if, den första sätter timetofirstdiscovery, den andra kollar timeto...
    xxx och då kankse man ska skippa discovery i setup? nej, den gör ingen skada
  */

  //do a new search if saved room wasn't available at startup
  if (device == -1 && settings.roomname != "" && noOfDiscoveries != 0) {
    noOfDiscoveries = 0; //not so logical to set it to zero, but think of it as a new start
  }
  if (noOfDiscoveries == 0) {
    if (timeToFirstDiscovery == 0 && wifisetup.connected() && !discoveryInProgress) {
      timeToFirstDiscovery = millis() + 30000;
    }
  }
  if (timeToFirstDiscovery != 0) {
    if (noOfDiscoveries == 0 && millis() > timeToFirstDiscovery) {
      timeToFirstDiscovery = 0;
      Serial.println("discover sonos 2 (in main loop)");
      discoverSonos(discoverSonosTimeout);
    }
  }

  if (lastPost + checkRate <= loopStart) {
    lastPost = loopStart;
    //put any code here that should run periodically

    //wifisetup.printInfo();
    Serial.print("Device ");
    Serial.print(device);
    if (device >= 0 && wifisetup.connected()) {
      Serial.print(" ");
      Serial.print(sonos.roomName(device));
      Serial.print(" ");
      Serial.print(sonos.getIpOfDevice(device));
    }
    Serial.println("");
    Serial.println("");

    //update oldVolume regularly, as other devices might change the volume as well
    if (device >= 0) {
      oldVolume = sonos.getVolume(device);
    }
  }

  //readKnob1();
  oldVolume = readKnob2(oldVolume);

  byte knobButtonStatus = knobButton.readButtonAdvanced(5000);
  if ((knobButtonStatus & knobButton.HOLD_DETECTED)) {
    Serial.println("hold detected");
    wifisetup.switchToAP_STA();
    wifisetup.timeToChangeToSTA = millis() + 5 * 60 * 1000;
  } else if (knobButtonStatus & knobButton.PRESS_DETECTED) {
    Serial.println("button pressed");
    if (device >= 0) {
      if (sonos.getTransportInfo(device) == "PLAYING") {
        //sonos.pause(device);
        Serial.println("PAUSE DISABLED");
      } else {
        //sonos.play(device);
        Serial.println("PLAY DISABLED");
      }
    }
  }

  if (testButton.readButton()) {
    clearSettings(); //only for test
  }
}

void clearSettings() {
  Serial.println("Clearing settings");
  settings.roomname = "";
  settings.roomno = -1;
  device = -1;
  settings.ssid = "";
  settings.psk = "";
  settings.save();
  WiFi.begin("foooooooooo", "");
}

void handleSearch() {
  Serial.println("handleSearch - searches for Sonos devices");

  long reloadTime = discoverSonosTimeout * 1000 + 4000 + 5000;
  sprintf(wifisetup.onload, "i1=setInterval(function() {location.href='/setup';},%i)", reloadTime);
  sprintf(wifisetup.body, "<h2>Wifi connection</h2><p>Searching for Sonos devices...</p>");
  wifisetup.sendHtml(wifisetup.body, wifisetup.onload);
  //give time to show page before discovery
  for (int i = 0; i < 2000; i++) { //2000 takes ca 4 s xxx could be made into a delay function
    wifisetup.inLoop();
  }
  Serial.println("discover sonos 3 (in handleSearch)");
  discoverSonos(discoverSonosTimeout);
  wifisetup.timeToChangeToSTA = millis() + 10 * 60 * 1000;
}

void handleSetup() {
  Serial.println("handleSetup");
  char bodych[1024];
  int selectedDevice = -1;

  if (wifisetup.server.hasArg("room")) {
    String roomStr = wifisetup.server.arg("room");
    device = roomStr.toInt();

    if (device >= 0 && sonos.getNumberOfDevices() > device) {
      settings.roomname = sonos.roomName(device);
      Serial.println(settings.roomname);
      Serial.print("Saving settings... ");
      settings.save();
    }

    wifisetup.timeToChangeToSTA = millis() + 5 * 60 * 1000;

    Serial.print("setting device to ");
    Serial.println(device);
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
      if (body.length()<1024-300) { //truncate if very many devices
        body = body + "<li><a href=setup?room=" + i + ">" + sonos.roomName(i) + "</a>";
        if (sonos.roomName(i) == settings.roomname && sonos.roomName(i) != "") {
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

  body.toCharArray(bodych, 1024);
  sprintf(wifisetup.body, "%s", bodych);
  sprintf(wifisetup.onload, "");
  wifisetup.sendHtml(wifisetup.body, wifisetup.onload);
}

void discoverSonos(int timeout) {
  if (!discoveryInProgress) {
    discoveryInProgress = true;
    noOfDiscoveries++;
    wifisetup.stopWebServer();//unpredicted behaviour if weserver contaced during discovery multicast
    Serial.println("discover sonos in library");
    sonos.discoverSonos(timeout);
    wifisetup.startWebServer();
    Serial.print("Found devices: ");
    Serial.println(sonos.getNumberOfDevices());
    for (int i = 0; i < sonos.getNumberOfDevices(); i++) {
      Serial.print("Device ");
      Serial.print(i);
      Serial.print(": ");
      Serial.print(sonos.getIpOfDevice(i));
      Serial.print(" ");
      Serial.print(sonos.roomName(i));
      if (sonos.roomName(i) == settings.roomname && sonos.roomName(i) != "") {
        Serial.print(" *");
        device = i;
      }
      Serial.println("");
    }
    Serial.println("discoverReady");
    wifisetup.startWebServer();
    discoveryInProgress = false;
  }
}

void readKnob1() {
  //read knob
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

    Serial.print("Encoder Position: ");
    Serial.println(encoderPosCount);

  }
  pinALast = aVal;

  if (millis() > moveTime && moveTime != 0) {
    int oldVolume = sonos.getVolume(device);
    //xxx handle timeout in getVolume
    int newVolume = oldVolume + floor(2 * 0.5 * (encoderRelativeCount + 0.5));
    newVolume = constrain(newVolume, 0, 100);

    Serial.print("Volume:         ");
    Serial.println(newVolume);
    Serial.print("Volume change;  ");
    Serial.println(newVolume - oldVolume);
    Serial.print("Encoder change: ");
    Serial.println(encoderRelativeCount);
    if (device >= 0) {
      sonos.setVolume(newVolume, device);
    }
    moveTime = 0;
    encoderRelativeCount = 0;
  }
}

int readKnob2(int oldVolume) {
  int newVolume = oldVolume;
  //read knob
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

    Serial.print("Encoder Position: ");
    Serial.println(encoderPosCount);

  }
  pinALast = aVal;

  if (millis() > moveTime && moveTime != 0) {
    //xxx handle timeout in getVolume
    if (encoderRelativeCount == 2 || encoderRelativeCount == -2) {
      //for finetuning of volume
      newVolume = oldVolume + 0.5 * encoderRelativeCount;
    } else {
      newVolume = oldVolume + floor(2 * 0.5 * (encoderRelativeCount + 0.5));
    }
    newVolume = constrain(newVolume, 0, 100);

    Serial.print("Encoder change: ");
    Serial.println(encoderRelativeCount);
    Serial.print("Volume change;  ");
    Serial.println(newVolume - oldVolume);
    Serial.print("Volume:         ");
    Serial.println(newVolume);
    if (device >= 0) {
      sonos.setVolume(newVolume, device);
    }
    moveTime = 0;
    encoderRelativeCount = 0;
  }
  return newVolume;
}

