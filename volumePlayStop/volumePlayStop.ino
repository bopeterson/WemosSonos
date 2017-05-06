/*
   connections

   Wemos   Rotary encoder (Keyes KY-040)
   GND     GND
   5V      +
   D3      CLK - pin A
   D6      SW - push button
   D7      DT - pin B

*/

/*
  Two things remaining:
  1 - better volume response
  2 - chose speaker - ok, this could be inititated by - long press - then press - change - press - change

  And maybe:
  3 - long press
  
 */

/*
måste kolla igen vad apa sta 0 innebär. 
och testa apa sta 1 som nån slags workaround för att behålla apa men ansluta ganska direkt
funkade inte så bra med nyuppladdat program...
*/

#include <WemosButton.h>
#include <WemosSonos.h>
#include <WemosSetup.h>

#include "settings.h"

bool testFlag=false;
bool testFlag2=false;
bool discoveryReady=false;

int device = 0;

const char compiletime[] = __TIME__;
const char compiledate[] = __DATE__;

WemosSetup wifisetup;

WemosSonos sonos;

WemosButton knobButton;

WemosButton testButton;

const unsigned long checkRate = 15 * 1000; //how often main loop performs periodical task
unsigned long lastPost = 0;

//encoder variables
const int pinA = D3;  // Connected to CLK on KY-040
const int pinB = D7;  // Connected to DT on KY-040
int encoderPosCount = 0;
int pinALast;
int aVal;
int encoderRelativeCount;
unsigned long moveTime;

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
  //wifisetup.begin(WIFI_AP_STA, 30, BUILTIN_LED);//

  
  wifisetup.server.on("/room", handleRoom);
  wifisetup.server.on("/shdev", handleShowDevices);

//xxx bryt ut detta till en configure funktion som bara körs om man gör hold på knappen, annars hämtar man device ur eeprom
  //wifisetup.switchToAP_STA();

  while (!wifisetup.connected()) {
    wifisetup.inLoop();
    unsigned long loopStart = millis();
    if (lastPost + checkRate <= loopStart) {
      lastPost = loopStart;
      wifisetup.printInfo(); //WFS_DEBUG to be true in WemosSetup.h for this to work
    }
  }


//xxx  delay(2000); //give time to show webpage before discovery begins...

  discoveryReady=false;
  sonos.discoverSonos();
  Serial.print("Found devices: ");
  Serial.println(sonos.getNumberOfDevices());
  for (int i = 0; i < sonos.getNumberOfDevices(); i++) {
    Serial.print("Device ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(sonos.getIpOfDevice(i));
    Serial.print(" ");
    Serial.print(sonos.roomName(i));
    if (sonos.roomName(i)==settings.roomname) {
      Serial.print(" *");
      device=i;
    }
    Serial.println("");
  }

  discoveryReady=true;
/* xxx
  Serial.println("entering testFlag2 loop-wait for device to be selected");
  while (!testFlag2) {
    wifisetup.inLoop();
  }
  Serial.println("exit testFlag2 loop");  
*/
}


void loop() {
  unsigned long loopStart = millis();

  wifisetup.inLoop(); 

  if (lastPost + checkRate <= loopStart) {
    lastPost = loopStart;
    //put any code here that should run periodically
    //Serial.print(wifisetup.connected());
    //Serial.print(" ");
    //Serial.println(millis());
    wifisetup.printInfo();
    //Serial.println(wifisetup.connected());
  }



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
    sonos.setVolume(newVolume, device);
    moveTime = 0;
    encoderRelativeCount = 0;
  }

  byte knobButtonStatus=knobButton.readButtonAdvanced(5000);
  if (knobButtonStatus & knobButton.HOLD_DETECTED) {
    Serial.println("hold detected");
    wifisetup.switchToAP_STA();
  } else if (knobButtonStatus & knobButton.RELEASE_DETECTED) {
    Serial.println("button pressed");
    if (sonos.getTransportInfo(device) == "PLAYING") {
      //sonos.pause(device);
      Serial.println("PAUSE DISABLED");
    } else {
      //sonos.play(device);
      Serial.println("PLAY DISABLED");
    }
  }


  if (testButton.readButton()) {
    for (int i = 0; i < sonos.getNumberOfDevices(); i++) {
      Serial.print("Device ");
      Serial.print(i);
      Serial.print(" vol: ");
      Serial.print(sonos.getVolume(i));
      Serial.print(" ");
      Serial.println(sonos.roomName(i));
    }


  }


}

void handleRoom() {
  Serial.println("handleDev");

  if (!discoveryReady) {
    sprintf(wifisetup.onload,"");
    sprintf(wifisetup.body,"checking devices");
    wifisetup.sendHtml(wifisetup.body,wifisetup.onload);
  } else {
    sprintf(wifisetup.onload,"");
    String names="Select room: ";
    for (int i=0;i < sonos.getNumberOfDevices();i++) {
      names = names + "<div><a href=shdev?room="+i+">"+sonos.roomName(i)+"</a></div>";
    }
    char namesch[100];
    names.toCharArray(namesch,100);
    sprintf(wifisetup.body,"%s",namesch);  
    sprintf(wifisetup.onload,"");
    wifisetup.sendHtml(wifisetup.body,wifisetup.onload);
  }
  testFlag=true;
}

void handleShowDevices() {
  Serial.println("handleShowDevices");

  if (wifisetup.server.hasArg("room")) {
    String roomStr = wifisetup.server.arg("room");
    device=roomStr.toInt();    
  } else {
    device=0;
  }
  Serial.print("Selected device: ");
  Serial.println(device);
  
  sprintf(wifisetup.onload,"");
  sprintf(wifisetup.body,"Setup is ready. Enjoy");

  settings.roomname = sonos.roomName(device);

  settings.save();

  wifisetup.sendHtml(wifisetup.body,wifisetup.onload);

  testFlag2=true;

}

