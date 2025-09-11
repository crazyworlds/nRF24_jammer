#ifndef OPTIONS_H
#define OPTIONS_H

#include "Update.h"
#include "EEPROM.h"
#include "WiFi.h"
#include "WebServer.h"
#include "RF24.h"

extern const char *default_ssid;
extern const char *default_password;

extern size_t EEPROM_SIZE;
extern SPIClass *hp;
extern int menu_number;
extern int bluetooth_jam_method;
extern int drone_jam_method;
extern int ble_jam_method;
extern int wifi_jam_method;
extern int zigbee_jam_method;
extern int Separate_or_together;
extern int misc_jam_method;
extern int logo;
extern int access_point;
extern int channel1;
extern int channel2;
extern int flag;
extern bool hspi;
extern byte bluetooth_channels[];
extern byte ble_channels[];
extern const char jam_text[17];
extern RF24 *radios[30];
extern int ce_pins[30];
extern int csn_pins[30];
extern int nrf24_count;
extern WebServer server;

#endif