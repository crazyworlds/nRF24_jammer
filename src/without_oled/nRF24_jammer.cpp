#include "html.h"
#include "jam.h"
#include "options.h"


void handlernRF24Pins() 
{
  nrf24_count = server.arg("count").toInt();
  String ceStr = server.arg("ce");
  String csnStr = server.arg("csn");
  auto SplitStr = [](const String &str, int *arr, int maxCount) {
    int idx = 0;
    int start = 0;
    while (idx < maxCount) {
      int sep = str.indexOf('|', start);
      if (sep == -1)
        sep = str.length();
      String part = str.substring(start, sep);
      arr[idx++] = part.toInt();
      if (sep == str.length())
        break;
      start = sep + 1;
    }
  };
  SplitStr(ceStr, ce_pins, nrf24_count);
  SplitStr(csnStr, csn_pins, nrf24_count);
  for (int i = 0; i < nrf24_count; i++) {
    EEPROM.write(74 + i, ce_pins[i]);
    EEPROM.write(104 + i, csn_pins[i]);
  }
  EEPROM.write(134, nrf24_count);
  EEPROM.commit();
  server.send(200, "text/html", html);
}

void handlenRF24Init() 
{
  nrf24_count = EEPROM.read(134);
  if (nrf24_count > 30) {
    nrf24_count = 0;
    return;
  }
  for (int i = 0; i < nrf24_count; i++) {
    ce_pins[i] = EEPROM.read(74 + i);
    csn_pins[i] = EEPROM.read(104 + i);
  }
}

void handleRoot() 
{
  if (nrf24_count <= 0) {
    server.send(200, "text/html", html_nrf24_settings);
  } else {
    server.send(200, "text/html", html);
  }
}

void sendHtmlAndExecute(const char *htmlResponse, void (*action)() = nullptr) 
{
  server.send(200, "text/html", htmlResponse);
  delay(500);
  if (action)
    action();
}

void jamHandler(const char *htmlResponse, void (*jamFunction)()) 
{
  sendHtmlAndExecute(htmlResponse, jamFunction);
  updateDisplay(menu_number);
}

void miscChannelsHandler() 
{
  int channel1 = server.arg("start").toInt();
  int channel2 = server.arg("stop").toInt();
  sendHtmlAndExecute(html_misc_jam);
  misc_jam(channel1, channel2);
}

void wifiChannelsHandler() 
{
  int channel = server.arg("channel").toInt();
  sendHtmlAndExecute(html_wifi_jam);
  wifi_channel(channel);
}

void settingsHandler(const char *htmlResponse) 
{
  sendHtmlAndExecute(htmlResponse);
}

void storeEEPROMAndSet(int index, int value, int &targetVar) 
{
  EEPROM.write(index, value);
  EEPROM.commit();
  targetVar = value;
  sendHtmlAndExecute(html);
}

void registerRoute(const char *path, void (*handler)()) 
{
  server.on(path, handler);
}

void saveWiFiSettings(const char *new_ssid, const char *new_password) 
{
  for (int i = 0; i < 32; i++) {
    if (i < strlen(new_ssid)) {
      EEPROM.write(11 + i, new_ssid[i]);
    } else {
      EEPROM.write(11 + i, 0);
    }
  }

  for (int i = 0; i < 32; i++) {
    if (i < strlen(new_password)) {
      EEPROM.write(43 + i, new_password[i]);
    } else {
      EEPROM.write(43 + i, 0);
    }
  }

  EEPROM.commit();
}

void initWiFiSettings() 
{
  if (EEPROM.read(11) == 255) {
    saveWiFiSettings(default_ssid, default_password);
  }
}

String getSSIDFromEEPROM() 
{
  char ssid[33] = {0};
  for (int i = 0; i < 32; i++) {
    ssid[i] = EEPROM.read(11 + i);
    if (ssid[i] == 0)
      break;
  }
  return String(ssid);
}

String getPasswordFromEEPROM() 
{
  char password[33] = {0};
  for (int i = 0; i < 32; i++) {
    password[i] = EEPROM.read(43 + i);
    if (password[i] == 0)
      break;
  }
  return String(password);
}

void handleSaveWiFiSettings() 
{
  String new_ssid = server.arg("ssid");
  String new_password = server.arg("password");

  if (new_ssid == "" && new_password == "") {
    server.send(200, "text/html", html_pls_reboot);
    storeEEPROMAndSet(8, 1, access_point);
    return;
  }

  saveWiFiSettings(new_ssid.c_str(), new_password.c_str());

  server.send(200, "text/html", html_pls_reboot);
  delay(1000);
  ESP.restart();
}

void handleResetWiFiSettings() 
{
  saveWiFiSettings(default_ssid, default_password);

  server.send(200, "text/html", html_pls_reboot);
  delay(1000);
  ESP.restart();
}

void handleFileUpload() 
{
  HTTPUpload &upload = server.upload();
  static unsigned long lastUpdate = 0;
  const unsigned long updateInterval = 100;

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      server.send(200, "text/plain", "Update Failed");
      return;
    } else {
      server.sendContent(html_upload_progress);
      lastUpdate = millis();
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }

    if (millis() - lastUpdate >= updateInterval) {
      lastUpdate = millis();
      int progress = (100.0 * Update.progress() / Update.size());
      server.sendContent(String("<script>updateProgress(") + progress +
                         ");</script>");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u bytes\n", upload.totalSize);
      server.sendContent(String("<script>updateProgress(100); "
                                "document.getElementById('status').innerHTML = "
                                "'Update Success';</script>"));
      delay(1000);
      ESP.restart();
    } else {
      Update.printError(Serial);
      server.sendContent("<script>document.getElementById('status').innerHTML "
                         "= 'Update Failed';</script>");
    }
  }
}

void misc() 
{
  auto incrementChannel = [&](int &channel) {
    channel++;
    if (channel > 125) {
      channel = 0;
    }
  };
  auto reduceChannel = [&](int &channel) {
    channel--;
    if (channel < 0) {
      channel = 125;
    }
  };


}


void access_poin_off() 
{
  settingsHandler(html_pls_reboot);
  storeEEPROMAndSet(8, 1, access_point);
  delay(1000);
  ESP.restart();
}

void setup() 
{
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  for (int i = 0; i < 11; i++) {
    if (EEPROM.read(i) == 255) {
      EEPROM.write(i, 0);
    }
  }

  handlenRF24Init();

  initWiFiSettings();

  // Load settings
  bluetooth_jam_method = EEPROM.read(0);
  drone_jam_method = EEPROM.read(1);
  ble_jam_method = EEPROM.read(2);
  wifi_jam_method = EEPROM.read(3);
  zigbee_jam_method = EEPROM.read(5);
  misc_jam_method = EEPROM.read(6);
  logo = EEPROM.read(7);
  access_point = EEPROM.read(8);
  buttons = EEPROM.read(9);
  Separate_or_together = EEPROM.read(10);

  if (access_point == 0) 
  {
    String current_ssid = getSSIDFromEEPROM();
    String current_password = getPasswordFromEEPROM();

    WiFi.softAP(current_ssid.c_str(), current_password.c_str());

    // Register routes
    registerRoute("/", handleRoot);
    registerRoute("/bluetooth_jam", []() {
      jamHandler(html_bluetooth_jam, bluetooth_jam);
    });
    registerRoute("/drone_jam", []() {
      jamHandler(html_drone_jam, drone_jam);
    });
    registerRoute("/wifi_jam", []() {
      jamHandler(html_wifi_jam, wifi_jam);
    });
    registerRoute("/ble_jam",
                  []() { jamHandler(html_ble_jam, ble_jam); });
    registerRoute("/zigbee_jam", []() {
      jamHandler(html_zigbee_jam, zigbee_jam);
    });
    registerRoute("/misc_jammer",
                  []() { sendHtmlAndExecute(html_misc_jammer); });
    registerRoute("/misc_jam", miscChannelsHandler);
    registerRoute("/wifi_selected_jam", wifiChannelsHandler);

    registerRoute("/setting_bluetooth_jam",
                  []() { settingsHandler(html_bluetooth_setings); });
    registerRoute("/setting_drone_jam",
                  []() { settingsHandler(html_drone_setings); });
    registerRoute("/setting_separate_together",
                  []() { settingsHandler(html_separate_or_together); });
    registerRoute("/setting_misc_jam",
                  []() { settingsHandler(html_misc_setings); });
    registerRoute("/setting_logo",
                  []() { settingsHandler(html_logo_setings); });
    registerRoute("/setting_buttons",
                  []() { settingsHandler(html_buttons_settings); });
    registerRoute("/OTA", []() { settingsHandler(html_ota); });
    registerRoute("/wifi_select", []() { settingsHandler(html_wifi_select); });
    registerRoute("/wifi_channel",
                  []() { settingsHandler(html_wifi_channel); });
    registerRoute("/wifi_settings",
                  []() { settingsHandler(html_wifi_settings); });
    registerRoute("/nrf24_settings",
                  []() { settingsHandler(html_nrf24_settings); });

    server.on(
        "/update", HTTP_POST,
        []() {
          server.send(200, "text/plain",
                      (Update.end() ? "Update Success" : "Update Failed"));
        },
        handleFileUpload);
    server.on("/save_wifi_settings", handleSaveWiFiSettings);
    server.on("/reset_wifi_settings", handleResetWiFiSettings);

    registerRoute("/bluetooth_method_0",
                  []() { storeEEPROMAndSet(0, 0, bluetooth_jam_method); });
    registerRoute("/bluetooth_method_1",
                  []() { storeEEPROMAndSet(0, 1, bluetooth_jam_method); });
    registerRoute("/bluetooth_method_2",
                  []() { storeEEPROMAndSet(0, 2, bluetooth_jam_method); });
    registerRoute("/drone_method_0",
                  []() { storeEEPROMAndSet(1, 0, drone_jam_method); });
    registerRoute("/drone_method_1",
                  []() { storeEEPROMAndSet(1, 1, drone_jam_method); });
    registerRoute("/separate_or_together_method_0",
                  []() { storeEEPROMAndSet(4, 0, Separate_or_together); });
    registerRoute("/separate_or_together_method_1",
                  []() { storeEEPROMAndSet(4, 1, Separate_or_together); });
    registerRoute("/misc_method_0",
                  []() { storeEEPROMAndSet(6, 0, misc_jam_method); });
    registerRoute("/misc_method_1",
                  []() { storeEEPROMAndSet(6, 1, misc_jam_method); });
    registerRoute("/logo_on", []() { storeEEPROMAndSet(7, 0, logo); });
    registerRoute("/logo_off", []() { storeEEPROMAndSet(7, 1, logo); });
    registerRoute("/button_method_0",
                  []() { storeEEPROMAndSet(9, 0, buttons); });
    registerRoute("/button_method_1",
                  []() { storeEEPROMAndSet(9, 1, buttons); });
    registerRoute("/button_method_2",
                  []() { storeEEPROMAndSet(9, 2, buttons); });
    registerRoute("/access_point_off", []() { access_poin_off(); });
    registerRoute("/set_nrf24_pins", []() { handlernRF24Pins(); });

    server.begin();
  }

}

void executeAction(int menuNum) {
  if (nrf24_count <= 0) {
    display.clearDisplay();
    display.setCursor(40, 0);
    display.println("WARNING");

    display.setCursor(5, 10);
    display.println("NRF24 not configured");

    display.setCursor(7, 20);
    display.println("Configure in web UI");

    display.display();
    while (nrf24_count <= 0) {
      server.handleClient();
      delay(100);
    }
  }
  if (menuNum == 5)
    misc();
  display.clearDisplay();
  const uint8_t *bitmap = (menu_number == 0)   ? bitmap_bluetooth_jam
                          : (menu_number == 1) ? bitmap_drone_jam
                          : (menu_number == 2) ? bitmap_wifi_all
                          : (menu_number == 3) ? bitmap_ble_jam
                          : (menu_number == 4) ? bitmap_zigbee_jam
                          : (menu_number == 6) ? bitmap_pls_reboot
                                               : NULL;
  display.drawBitmap(0, 0, bitmap, 128, 64, WHITE);
  display.display();

  switch (menu_number) {
  case 0:
    bluetooth_jam();
    break;
  case 1:
    drone_jam();
    break;
  case 2:
    wifi_select();
    break;
  case 3:
    ble_jam();
    break;
  case 4:
    zigbee_jam();
    break;
  case 6:
    storeEEPROMAndSet(8, 0, logo);
    break;
  }
  if (menuNum != 6)
    updateDisplay(menuNum);
}

void loop() {

  if (access_point == 0)
    server.handleClient();
}