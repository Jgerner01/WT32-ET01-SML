/**
 * display.cpp – Nokia 5110 Implementierung
 */
#include "display.h"
#include <Arduino.h>

#define LEDC_CHANNEL      1
#define LEDC_FREQ         1000
#define LEDC_RESOLUTION   8

const unsigned char icon_wifi_off[] PROGMEM = {0x00,0x00,0x42,0x24,0x18,0x18,0x24,0x42};
const unsigned char icon_wifi_on[]  PROGMEM = {0x00,0x42,0x66,0x3C,0x18,0x18,0x3C,0x66};
const unsigned char icon_mqtt_off[] PROGMEM = {0x00,0x00,0x7E,0x42,0x5A,0x42,0x7E,0x00};
const unsigned char icon_mqtt_on[]  PROGMEM = {0x7E,0x42,0x5A,0x5A,0x5A,0x42,0x7E,0x00};
const unsigned char icon_lan_off[]  PROGMEM = {0x00,0x00,0x7E,0x66,0x66,0x66,0x7E,0x00};
const unsigned char icon_lan_on[]   PROGMEM = {0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0x00};

DisplayManager::DisplayManager()
    : display(nullptr), currentPage(0), lastPageChange(0),
      wifiConnected(false), lanConnected(false), mqttConnected(false),
      brightness(DISPLAY_BRIGHTNESS), ipAddress(""),
      lastScrollTime(0), scrollPos(0) {}

DisplayManager::~DisplayManager() {
    if (display) { delete display; display = nullptr; }
}

void DisplayManager::begin() {
    display = new Adafruit_PCD8544(PIN_DISPLAY_SCLK, PIN_DISPLAY_DIN,
                                    PIN_DISPLAY_DC, PIN_DISPLAY_CS,
                                    PIN_DISPLAY_RST);
    display->begin();
    display->setContrast(DISPLAY_CONTRAST);
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(BLACK);
    display->display();

    ledcAttachPin(PIN_DISPLAY_BL, LEDC_CHANNEL);
    ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RESOLUTION);
    ledcWrite(LEDC_CHANNEL, brightness);

    DEBUG_PRINTLN("[Display] Nokia 5110 initialisiert");

    display->setCursor(5, 5);
    display->println("SML-Display");
    display->setCursor(10, 20);
    display->print("v"); display->print(FIRMWARE_VERSION);
    display->setCursor(5, 35);
    display->print("Start...");
    display->display();
    delay(1500);
}

void DisplayManager::update(const SmlData& smlData) {
    unsigned long now = millis();
    if (now - lastPageChange >= DISPLAY_PAGE_INTERVAL) {
        currentPage = (currentPage + 1) % DISPLAY_PAGE_COUNT;
        lastPageChange = now;
    }

    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(BLACK);
    drawStatusBar();

    if (!smlData.isValid) {
        drawNoData();
    } else {
        switch (currentPage) {
            case DISPLAY_PAGE_1: drawPage1(smlData); break;
            case DISPLAY_PAGE_2: drawPage2(smlData); break;
            case DISPLAY_PAGE_3: drawPage3(smlData); break;
        }
    }
    display->display();
}

void DisplayManager::setWifiStatus(bool c) { wifiConnected = c; }
void DisplayManager::setLanStatus(bool c) { lanConnected = c; }
void DisplayManager::setMqttStatus(bool c) { mqttConnected = c; }
void DisplayManager::setIpAddress(const String& ip) { ipAddress = ip; }

void DisplayManager::nextPage() { currentPage = (currentPage+1)%DISPLAY_PAGE_COUNT; lastPageChange = millis(); }
void DisplayManager::resetPage() { currentPage = 0; lastPageChange = millis(); }

void DisplayManager::setBrightness(uint8_t v) { brightness = v; ledcWrite(LEDC_CHANNEL, brightness); }
void DisplayManager::setContrast(uint8_t v) { display->setContrast(v); display->display(); }

void DisplayManager::showTestMessage() {
    display->clearDisplay();
    display->setCursor(5, 15); display->println("Test Display");
    display->setCursor(5, 25); display->println("Nokia 5110");
    display->setCursor(5, 35); display->println("84x48 Pixel");
    display->display();
}

// ============================================================

void DisplayManager::drawStatusBar() {
    display->drawLine(0, 9, 83, 9, BLACK);

    // Laufband ZUERST – Icons werden danach darübergelegt und
    // decken den Pixel-Überlauf am linken/rechten Rand ab
    drawMarquee();

    // WiFi Icon links (0,0)
    if (wifiConnected)
        display->drawBitmap(0, 0, icon_wifi_on, 8, 8, BLACK);
    else
        display->drawBitmap(0, 0, icon_wifi_off, 8, 8, BLACK);

    // LAN Icon neben WiFi (8,0)
    if (lanConnected)
        display->drawBitmap(8, 0, icon_lan_on, 8, 8, BLACK);
    else
        display->drawBitmap(8, 0, icon_lan_off, 8, 8, BLACK);

    // MQTT Icon rechts (75,0)
    if (mqttConnected)
        display->drawBitmap(75, 0, icon_mqtt_on, 8, 8, BLACK);
    else
        display->drawBitmap(75, 0, icon_mqtt_off, 8, 8, BLACK);

    // Seiten-Indikator unten rechts
    display->setCursor(68, 40);
    display->print(String(currentPage+1) + "/" + String(DISPLAY_PAGE_COUNT));
}

void DisplayManager::drawMarquee() {
    if (ipAddress.length() == 0) return;

    // Text mit 4 Leerzeichen als Trenner für nahtlosen Loop
    String text = ipAddress + "    ";
    int totalPixels = (int)text.length() * 6;  // 6 Pixel pro Zeichen (5 + 1 Abstand)

    // scrollPos = Pixel-Offset; alle 100ms um 1 Spalte vorwärts
    unsigned long now = millis();
    if (now - lastScrollTime >= SCROLL_INTERVAL_MS) {
        lastScrollTime = now;
        if (++scrollPos >= totalPixels) scrollPos = 0;
    }

    // Erstes sichtbares Zeichen und Pixel-Versatz innerhalb dieses Zeichens
    int startChar   = scrollPos / 6;
    int pixelOffset = scrollPos % 6;

    // Cursor um pixelOffset Spalten vor x=17 setzen → erstes Zeichen
    // erscheint nur teilweise. Die Icons werden danach darübergelegt
    // und decken den Überlauf links (< 17) und rechts (> 74) ab.
    display->setTextWrap(false);
    display->setCursor(17 - pixelOffset, 1);

    // Einen Charakter mehr rendern als nötig, damit der Bereich vollständig gefüllt ist
    for (int i = 0; i <= STATUS_BAR_CHARS; i++) {
        int idx = (startChar + i) % (int)text.length();
        display->print(text.charAt(idx));
    }
}

void DisplayManager::drawPage1(const SmlData& smlData) {
    const ObisValue* importVal = nullptr;
    const ObisValue* exportVal = nullptr;
    for (int i = 0; i < smlData.valueCount; i++) {
        if (strcmp(smlData.values[i].obisCode, "1-0:1.8.0") == 0 ||
            strcmp(smlData.values[i].obisCode, "1-0:1.8.0*255") == 0)
            importVal = &smlData.values[i];
        if (strcmp(smlData.values[i].obisCode, "1-0:2.8.0") == 0 ||
            strcmp(smlData.values[i].obisCode, "1-0:2.8.0*255") == 0)
            exportVal = &smlData.values[i];
    }
    display->setCursor(2, 12); display->print("Bezug:");
    display->setCursor(2, 22);
    if (importVal) { display->print(importVal->value, 3); display->print(" kWh"); }
    else display->print("---.--- kWh");
    display->setCursor(2, 32); display->print("Lief.:");
    display->setCursor(2, 42);
    if (exportVal) { display->print(exportVal->value, 3); display->print(" kWh"); }
    else display->print("---.--- kWh");
}

void DisplayManager::drawPage2(const SmlData& smlData) {
    const ObisValue* power = nullptr;
    const ObisValue* powerL1 = nullptr;
    const ObisValue* powerL2 = nullptr;
    const ObisValue* powerL3 = nullptr;
    for (int i = 0; i < smlData.valueCount; i++) {
        if (strcmp(smlData.values[i].obisCode, "1-0:16.7.0") == 0 ||
            strcmp(smlData.values[i].obisCode, "1-0:16.7.0*255") == 0) power = &smlData.values[i];
        if (strcmp(smlData.values[i].obisCode, "1-0:36.7.0") == 0 ||
            strcmp(smlData.values[i].obisCode, "1-0:36.7.0*255") == 0) powerL1 = &smlData.values[i];
        if (strcmp(smlData.values[i].obisCode, "1-0:56.7.0") == 0 ||
            strcmp(smlData.values[i].obisCode, "1-0:56.7.0*255") == 0) powerL2 = &smlData.values[i];
        if (strcmp(smlData.values[i].obisCode, "1-0:76.7.0") == 0 ||
            strcmp(smlData.values[i].obisCode, "1-0:76.7.0*255") == 0) powerL3 = &smlData.values[i];
    }
    display->setCursor(2, 12); display->print("Leistung:");
    display->setCursor(2, 22);
    if (power) { display->print(power->value, 0); display->print(" W");
        if (power->value < 0) display->print("<"); }
    else display->print("--- W");
    display->setCursor(2, 32); display->print("L1:");
    if (powerL1) { display->print(powerL1->value, 0); display->print("W"); } else display->print("---W");
    display->setCursor(2, 42); display->print("L2:");
    if (powerL2) { display->print(powerL2->value, 0); display->print("W"); } else display->print("---W");
}

void DisplayManager::drawPage3(const SmlData& smlData) {
    const ObisValue* voltL1=nullptr, *voltL2=nullptr, *voltL3=nullptr;
    const ObisValue* currL1=nullptr, *currL2=nullptr, *currL3=nullptr;
    for (int i = 0; i < smlData.valueCount; i++) {
        if (strcmp(smlData.values[i].obisCode,"1-0:32.7.0")==0||strcmp(smlData.values[i].obisCode,"1-0:32.7.0*255")==0) voltL1=&smlData.values[i];
        if (strcmp(smlData.values[i].obisCode,"1-0:52.7.0")==0||strcmp(smlData.values[i].obisCode,"1-0:52.7.0*255")==0) voltL2=&smlData.values[i];
        if (strcmp(smlData.values[i].obisCode,"1-0:72.7.0")==0||strcmp(smlData.values[i].obisCode,"1-0:72.7.0*255")==0) voltL3=&smlData.values[i];
        if (strcmp(smlData.values[i].obisCode,"1-0:31.7.0")==0||strcmp(smlData.values[i].obisCode,"1-0:31.7.0*255")==0) currL1=&smlData.values[i];
        if (strcmp(smlData.values[i].obisCode,"1-0:51.7.0")==0||strcmp(smlData.values[i].obisCode,"1-0:51.7.0*255")==0) currL2=&smlData.values[i];
        if (strcmp(smlData.values[i].obisCode,"1-0:71.7.0")==0||strcmp(smlData.values[i].obisCode,"1-0:71.7.0*255")==0) currL3=&smlData.values[i];
    }
    display->setCursor(2, 12); display->print("U L1:");
    if (voltL1) { display->print(voltL1->value,1); display->print("V"); } else display->print("---.-V");
    display->setCursor(2, 22); display->print("U L2:");
    if (voltL2) { display->print(voltL2->value,1); display->print("V"); } else display->print("---.-V");
    display->setCursor(2, 32); display->print("U L3:");
    if (voltL3) { display->print(voltL3->value,1); display->print("V"); } else display->print("---.-V");
    display->setCursor(2, 42); display->print("I:");
    if (currL1) { display->print(currL1->value,2); display->print("A"); } else display->print("---.--A");
}

void DisplayManager::drawNoData() {
    display->setCursor(5, 18); display->print("Warte auf");
    display->setCursor(5, 28); display->print("SML-Daten...");
    display->setCursor(5, 40); display->print("IR-Kopf pruefen");
}

void DisplayManager::drawValue(int x, int y, float value, const char* unit, int decimals) {
    display->setCursor(x, y);
    display->print(value, decimals);
    display->print(unit);
}

void DisplayManager::drawValueWithLabel(int x, int y, const char* label, float value, const char* unit, int decimals) {
    display->setCursor(x, y);
    display->print(label); display->print(":");
    drawValue(x + strlen(label)*6+4, y, value, unit, decimals);
}
