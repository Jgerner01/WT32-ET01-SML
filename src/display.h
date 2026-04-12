/**
 * display.h – Nokia 5110 Display Interface
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "config.h"
#include "sml_reader.h"

extern const unsigned char icon_wifi_off[];
extern const unsigned char icon_wifi_on[];
extern const unsigned char icon_mqtt_off[];
extern const unsigned char icon_mqtt_on[];
extern const unsigned char icon_lan_off[];
extern const unsigned char icon_lan_on[];

class DisplayManager {
public:
    DisplayManager();
    ~DisplayManager();

    void begin();
    void update(const SmlData& smlData);
    void setWifiStatus(bool connected);
    void setLanStatus(bool connected);
    void setMqttStatus(bool connected);
    void setIpAddress(const String& ip);
    void nextPage();
    void resetPage();
    void setBrightness(uint8_t brightness);
    void setContrast(uint8_t contrast);
    void showTestMessage();

private:
    Adafruit_PCD8544* display;
    uint8_t currentPage;
    unsigned long lastPageChange;
    bool wifiConnected;
    bool lanConnected;
    bool mqttConnected;
    uint8_t brightness;
    String ipAddress;

    // Laufband
    unsigned long lastScrollTime;
    int scrollPos;
    static const unsigned long SCROLL_INTERVAL_MS = 100;
    static const int STATUS_BAR_CHARS = 11;

    void drawStatusBar();
    void drawMarquee();
    void drawPage1(const SmlData& smlData);
    void drawPage2(const SmlData& smlData);
    void drawPage3(const SmlData& smlData);
    void drawNoData();
    void drawValue(int x, int y, float value, const char* unit, int decimals = 1);
    void drawValueWithLabel(int x, int y, const char* label, float value, const char* unit, int decimals = 1);
};

#endif
