/*
   ESP32 Sonos Volume Controller with Rotary Encoder

   Connections (ESP32 DevKit / Wemos S2 mini / similar):

   ESP32     Rotary encoder (KY-040)
   GND       GND
   3V3       +
   GPIO 25   CLK - pin A
   GPIO 26   DT  - pin B
   GPIO 27   SW  - push button

   Instructions:
   1. Set your WiFi credentials below (WIFI_SSID / WIFI_PASSWORD)
   2. Upload to ESP32
   3. Open serial monitor at 115200
   4. The device will discover Sonos speakers on your network
   5. Turn knob to adjust volume, press button to toggle play/pause
   6. To select a different Sonos device, browse to the ESP32's IP address
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WemosSonos.h>

// WiFi credentials - change these to match your network
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// Encoder pins - adjust for your ESP32 board
const int pinA = 25;       // CLK
const int pinB = 26;       // DT
const int pinButton = 27;  // SW (active low)

const int LED_PIN = 2;     // Built-in LED on most ESP32 boards

WemosSonos sonos;
WebServer server(80);
Preferences preferences;

int device = -1;
int discoverSonosTimeout = 9; // seconds
String roomNames[SNSESP_MAXNROFDEVICES];
String selectedRoomName = "";

// Encoder state
int pinALast;
int encoderRelativeCount = 0;
unsigned long moveTime = 0;
int oldVolume = 0;

// Periodic update
const unsigned long checkRate = 30000; // 30 seconds
unsigned long lastCheck = 0;

// Button debounce
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 300;

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 Sonos Controller starting...");

  // Load saved room preference
  preferences.begin("sonos", true); // read-only
  selectedRoomName = preferences.getString("roomname", "");
  preferences.end();
  Serial.print("Saved room: ");
  Serial.println(selectedRoomName);

  // Encoder setup
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  pinMode(pinButton, INPUT_PULLUP);
  pinALast = digitalRead(pinA);

  // LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart) < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED_PIN, HIGH);

    // Set up web server for device selection
    server.on("/", handleRoot);
    server.on("/select", handleSelect);
    server.on("/search", handleSearch);
    server.begin();
    Serial.println("Web server started");

    // Discover Sonos devices
    discoverSonos();
  } else {
    Serial.println("WiFi connection failed!");
  }
}

void loop() {
  server.handleClient();

  unsigned long now = millis();

  // Periodic volume refresh (other devices may have changed volume)
  if (device >= 0 && (now - lastCheck) >= checkRate) {
    lastCheck = now;
    oldVolume = sonos.getVolume(device);
    Serial.print("Volume update: ");
    Serial.println(oldVolume);
  }

  // Read rotary encoder
  oldVolume = readKnob(oldVolume);

  // Read button
  if (digitalRead(pinButton) == LOW && (now - lastButtonPress) > debounceDelay) {
    lastButtonPress = now;
    Serial.println("Button pressed");
    if (device >= 0) {
      togglePlay(device);
    }
  }
}

void discoverSonos() {
  Serial.println("Discovering Sonos devices...");
  sonos.discoverSonos(discoverSonosTimeout);
  Serial.print("Found devices: ");
  Serial.println(sonos.getNumberOfDevices());

  for (int i = 0; i < sonos.getNumberOfDevices(); i++) {
    Serial.print("  Device ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(sonos.getIpOfDevice(i));
    Serial.print(" - ");
    String name = sonos.roomName(i);
    roomNames[i] = name;
    Serial.print(name);
    if (name == selectedRoomName && name != "") {
      Serial.print(" *");
      device = i;
    }
    Serial.println();
  }

  if (device >= 0) {
    oldVolume = sonos.getVolume(device);
    Serial.print("Current volume: ");
    Serial.println(oldVolume);
  }
}

void togglePlay(int dev) {
  // Handle grouped devices: if device is controlled by a coordinator,
  // we need to control the coordinator instead

  String info = sonos.getTransportInfo(dev);
  String infoAfter;

  if (info == "PLAYING") {
    sonos.pause(dev);
    infoAfter = sonos.getTransportInfo(dev);
    if (infoAfter != "PAUSED_PLAYBACK") {
      // Device might be in a group, try coordinator
      int coordinator = sonos.getCoordinator(dev);
      if (coordinator >= 0) {
        String coordInfo = sonos.getTransportInfo(coordinator);
        if (coordInfo == "PLAYING") {
          sonos.pause(coordinator);
        } else {
          sonos.play(coordinator);
        }
      }
    }
  } else if (info == "PAUSED_PLAYBACK") {
    sonos.play(dev);
  } else if (info == "STOPPED") {
    sonos.play(dev);
    int coordinator = sonos.getCoordinator(dev);
    if (coordinator >= 0) {
      String coordInfo = sonos.getTransportInfo(coordinator);
      if (coordInfo == "PAUSED_PLAYBACK" || coordInfo == "STOPPED") {
        sonos.play(coordinator);
      }
    }
  }
}

int readKnob(int currentVolume) {
  int newVolume = currentVolume;
  int aVal = digitalRead(pinA);

  if (aVal != pinALast) {
    moveTime = millis() + 150;
    if (digitalRead(pinB) != aVal) {
      encoderRelativeCount++;
    } else {
      encoderRelativeCount--;
    }
  }
  pinALast = aVal;

  if (millis() > moveTime && moveTime != 0) {
    if (encoderRelativeCount == 2 || encoderRelativeCount == -2) {
      newVolume = currentVolume + encoderRelativeCount / 2;
    } else {
      newVolume = currentVolume + (int)floor(2 * 0.5 * (encoderRelativeCount + 0.5));
    }
    newVolume = constrain(newVolume, 0, 100);

    Serial.print("Volume: ");
    Serial.println(newVolume);
    if (device >= 0) {
      sonos.setVolume(newVolume, device);
    }
    moveTime = 0;
    encoderRelativeCount = 0;
  }
  return newVolume;
}

// --- Web server handlers ---

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif;margin:20px}a{display:block;padding:10px;margin:5px 0;";
  html += "background:#007bff;color:white;text-decoration:none;border-radius:5px;text-align:center}</style></head>";
  html += "<body><h2>Sonos Controller</h2>";

  if (sonos.getNumberOfDevices() == 0) {
    html += "<p>No Sonos devices found.</p>";
  } else {
    html += "<p>Select a Sonos device:</p>";
    for (int i = 0; i < sonos.getNumberOfDevices(); i++) {
      html += "<a href='/select?room=" + String(i) + "'>" + roomNames[i];
      if (roomNames[i] == selectedRoomName && roomNames[i] != "") {
        html += " (selected)";
      }
      html += "</a>";
    }
  }

  html += "<br><a href='/search' style='background:#28a745'>Search for Sonos devices</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSelect() {
  if (server.hasArg("room")) {
    int roomIdx = server.arg("room").toInt();
    if (roomIdx >= 0 && roomIdx < sonos.getNumberOfDevices()) {
      device = roomIdx;
      selectedRoomName = roomNames[roomIdx];

      // Save to preferences
      preferences.begin("sonos", false); // read-write
      preferences.putString("roomname", selectedRoomName);
      preferences.end();

      oldVolume = sonos.getVolume(device);
      Serial.print("Selected: ");
      Serial.print(selectedRoomName);
      Serial.print(" vol=");
      Serial.println(oldVolume);
    }
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleSearch() {
  server.send(200, "text/html", "<!DOCTYPE html><html><body><p>Searching...</p><script>setTimeout(function(){location.href='/';},15000);</script></body></html>");
  discoverSonos();
}
