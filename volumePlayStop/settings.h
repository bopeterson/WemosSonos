//borrowed code from https://github.com/buxtronix/arduino/blob/master/esp8266-ledclock/esp8266-ledclock.ino
#include <EEPROM.h>

#ifndef SETTINGS_H
#define SETTINGS_H

#define EEPROM_WIFI_SIZE 512
#define EEPROM_MAGIC "sonos"
#define EEPROM_MAGIC_OFFSET 0
#define EEPROM_MAGIC_LENGTH 5
#define EEPROM_SSID_OFFSET EEPROM_MAGIC_OFFSET + EEPROM_MAGIC_LENGTH
#define EEPROM_SSID_LENGTH 32
#define EEPROM_PSK_OFFSET EEPROM_SSID_OFFSET + EEPROM_SSID_LENGTH
#define EEPROM_PSK_LENGTH 64
#define EEPROM_TZ_OFFSET EEPROM_PSK_OFFSET + EEPROM_PSK_LENGTH
#define EEPROM_TIMESERVER_OFFSET EEPROM_TZ_OFFSET + 1
#define EEPROM_TIMESERVER_LENGTH 32
#define EEPROM_ROOMNO_OFFSET EEPROM_TIMESERVER_OFFSET + EEPROM_TIMESERVER_LENGTH
#define EEPROM_ROOMNO_LENGTH 2
#define EEPROM_ROOMNAME_OFFSET EEPROM_ROOMNO_OFFSET + EEPROM_ROOMNO_LENGTH
#define EEPROM_ROOMNAME_LENGTH 32

#define DEFAULT_TIMESERVER "time.nist.gov"


class Settings {

  public:
    Settings() {};

    void load() {
      char buffer[EEPROM_WIFI_SIZE];
      EEPROM.begin(EEPROM_WIFI_SIZE);
      for (int i = 0 ; i < EEPROM_WIFI_SIZE ; i++) {
        buffer[i] = EEPROM.read(i);
      }
      EEPROM.end();

      // Verify magic;
      String magic;
      for (int i = EEPROM_MAGIC_OFFSET ; i < EEPROM_MAGIC_OFFSET+EEPROM_MAGIC_LENGTH ; i++) {
        magic += buffer[i];
      }
      if (magic != EEPROM_MAGIC) {
        return;
      }
      // Read SSID
      ssid = "";
      for (int i = EEPROM_SSID_OFFSET ; i < EEPROM_SSID_OFFSET+EEPROM_SSID_LENGTH ; i++) {
        if (buffer[i]) ssid += buffer[i];
      }
      // Read PSK
      psk = "";
      for (int i = EEPROM_PSK_OFFSET ; i < EEPROM_PSK_OFFSET+EEPROM_PSK_LENGTH ; i++) {
        if (buffer[i]) psk += buffer[i];
      }

      timezone = long(buffer[EEPROM_TZ_OFFSET]);

      strncpy(timeserver, &buffer[EEPROM_TIMESERVER_OFFSET], EEPROM_TIMESERVER_LENGTH);
      if (strlen(timeserver) < 1) {
        strcpy(timeserver, DEFAULT_TIMESERVER);
      }

      roomno = time_t(buffer[EEPROM_ROOMNO_OFFSET]) << 8;
      roomno |= buffer[EEPROM_ROOMNO_OFFSET+1];
      // room name
      roomname = "";
      for (int i = EEPROM_ROOMNAME_OFFSET ; i < EEPROM_ROOMNAME_OFFSET+EEPROM_ROOMNAME_LENGTH ; i++) {
        if (buffer[i]) roomname += buffer[i];
      }
    }

    void save() {
      unsigned char buffer[EEPROM_WIFI_SIZE];
      memset(buffer, 0, EEPROM_WIFI_SIZE);

      // Copy magic to buffer;
      strncpy((char *)buffer, EEPROM_MAGIC, EEPROM_MAGIC_LENGTH);

      // Copy SSID to buffer;
      ssid.getBytes(&buffer[EEPROM_SSID_OFFSET], EEPROM_SSID_LENGTH, 0);
      // Copy PSK to buffer.
      psk.getBytes(&buffer[EEPROM_PSK_OFFSET], EEPROM_PSK_LENGTH, 0);
      // Copy timezone.
      buffer[EEPROM_TZ_OFFSET] = timezone;
      // Copy timeserver.
      strncpy((char *)&buffer[EEPROM_TIMESERVER_OFFSET], (char *)timeserver, EEPROM_TIMESERVER_LENGTH);
      // Copy roomno.
      buffer[EEPROM_ROOMNO_OFFSET] = roomno >> 8;
      buffer[EEPROM_ROOMNO_OFFSET+1] = roomno & 0xff;
      // Copy room name.
      roomname.getBytes(&buffer[EEPROM_ROOMNAME_OFFSET], EEPROM_ROOMNAME_LENGTH, 0);

      // Write to EEPROM.
      EEPROM.begin(EEPROM_WIFI_SIZE);
      for (int i = 0 ; i < EEPROM_WIFI_SIZE ; i++) {
        EEPROM.write(i, buffer[i]);
      }
      EEPROM.end();
    }

    String ssid;
    String psk;
    long timezone;
    char timeserver[64];
    int roomno;
    String roomname;
};

Settings settings;

#endif


