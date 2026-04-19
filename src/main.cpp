/**
 * main.cpp – SML-Display WT32-ETH01 V1.4
 * LAN + WiFi, AP-Fallback nach 20 Min, WLAN deaktivierbar
 *
 * Verbindungslogik:
 *   1. Ethernet starten (LAN)
 *   2. Falls LAN verbunden → WiFi optional (je nach Config)
 *   3. Falls KEIN LAN → WiFi STA versuchen
 *   4. Falls 20 Min keine Verbindung → AP-Mode (192.168.4.1)
 *   5. Falls WLAN deaktiviert → nur LAN, AP-Fallback unterdrückt
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "sml_reader.h"
#include "display.h"
#include "config_manager.h"
#include "webserver.h"
#include "mqtt_client.h"

// Forward declarations
void startWifiSta();
void onMqttTest();
void onMqttStatusChange(bool connected);

// ============================================================
// GLOBALE OBJEKTE
// ============================================================
static SmlReader smlReader;
static DisplayManager displayMgr;
static ConfigManager configMgr;
static WebServerManager webServer;
static MqttClientManager mqttClient;

// Konfigurationen
static WifiConfig wifiConfig;
static MqttConfig mqttConfig;
static DisplayConfig displayConfig;
static NetworkConfig networkConfig;

// Status
static bool ethConnected = false;
static bool wifiConnected = false;
static unsigned long lastMqttPublish = 0;
static unsigned long lastDiagPublish = 0;
static String currentIp;
static bool mqttReady = false;

// Taster
static unsigned long lastButtonPress = 0;
static bool buttonLongPress = false;

// ============================================================
// ETHERNET
// ============================================================

static bool ethWaiting = false;
static unsigned long ethStartTime = 0;

void startEthernet() {
    DEBUG_PRINTLN("[ETH] Starte LAN...");
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case ARDUINO_EVENT_ETH_START:
                DEBUG_PRINTLN("[ETH] ETH gestartet");
                ETH.setHostname(WIFI_HOSTNAME);
                break;
            case ARDUINO_EVENT_ETH_CONNECTED:
                DEBUG_PRINTLN("[ETH] ETH verbunden");
                break;
            case ARDUINO_EVENT_ETH_GOT_IP:
                ethConnected = true;
                ethWaiting = false;
                DEBUG_PRINTF("[ETH] IP: %s\n", ETH.localIP().toString().c_str());
                currentIp = ETH.localIP().toString();
                displayMgr.setIpAddress(currentIp);
                displayMgr.setLanStatus(true);
                break;
            case ARDUINO_EVENT_ETH_DISCONNECTED:
                ethConnected = false;
                displayMgr.setLanStatus(false);
                DEBUG_PRINTLN("[ETH] Getrennt");
                break;
            default: break;
        }
    });

    ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO,
              (eth_phy_type_t)ETH_PHY_TYPE, ETH_CLK_MODE);
    ethWaiting = true;
    ethStartTime = millis();
}

void checkEthernet() {
    // Timeout prüfen: 15s auf LAN warten
    if (ethWaiting && millis() - ethStartTime > ETH_CONNECT_TIMEOUT) {
        ethWaiting = false;
        if (!ethConnected) {
            DEBUG_PRINTLN("[ETH] Keine LAN-Verbindung innerhalb Timeout");
        }
    }
}

// ============================================================
// CALLBACKS
// ============================================================

void onWifiSave(const WifiConfig& config) {
    DEBUG_PRINTLN("[CB] WiFi gespeichert");
    configMgr.saveWifiConfig(config);
    wifiConfig = config;
    if (strlen(wifiConfig.ssid) > 0 && !ethConnected) {
        WiFi.begin(wifiConfig.ssid, wifiConfig.password);
    }
}

void onMqttSave(const MqttConfig& config) {
    DEBUG_PRINTLN("[CB] MQTT gespeichert");
    configMgr.saveMqttConfig(config);
    mqttConfig = config;
    if (config.enabled && strlen(config.broker) > 0) {
        mqttClient.setStatusCallback(onMqttStatusChange);
        mqttClient.begin(mqttConfig);
    } else {
        mqttClient.disconnect();
        displayMgr.setMqttStatus(false);
        mqttReady = false;
    }
}

void onNetworkSave(const NetworkConfig& config) {
    DEBUG_PRINTLN("[CB] Netzwerk gespeichert");
    configMgr.saveNetworkConfig(config);
    networkConfig = config;

    if (!config.wifiEnabled) {
        DEBUG_PRINTLN("[WiFi] Deaktiviert");
        WiFi.mode(WIFI_OFF);
        wifiConnected = false;
        displayMgr.setWifiStatus(false);
    } else if (config.mode == NET_MODE_WIFI_ONLY) {
        startWifiSta();
    }
}

void onIpDisplay(const String& ip) {
    currentIp = ip;
    displayMgr.setIpAddress(ip);
    DEBUG_PRINTF("[Display] IP: %s\n", ip.c_str());
}

void onMqttStatusChange(bool connected) {
    displayMgr.setMqttStatus(connected);
    mqttReady = connected;
}

void onMqttTest() {
    DEBUG_PRINTLN("[CB] MQTT Test");
    if (mqttClient.testConnection()) {
        DEBUG_PRINTLN("[MQTT] Test erfolgreich");
    } else {
        DEBUG_PRINTLN("[MQTT] Test fehlgeschlagen");
    }
}

// ============================================================
// WIFI
// ============================================================

void startWifiSta() {
    if (!networkConfig.wifiEnabled) return;
    if (strlen(wifiConfig.ssid) == 0) return;
    DEBUG_PRINTF("[WiFi] Verbinde mit: %s\n", wifiConfig.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.begin(wifiConfig.ssid, wifiConfig.password);
}

void checkWifiSta() {
    if (!networkConfig.wifiEnabled) return;
    if (networkConfig.mode == NET_MODE_LAN_ONLY) return;
    if (ethConnected && networkConfig.mode == NET_MODE_AUTO) return; // LAN优先

    static unsigned long lastCheck = 0;
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnected) {
            wifiConnected = true;
            DEBUG_PRINTF("[WiFi] Verbunden: %s\n", WiFi.localIP().toString().c_str());
            displayMgr.setWifiStatus(true);
            if (!currentIp.length()) {
                currentIp = WiFi.localIP().toString();
                displayMgr.setIpAddress(currentIp);
            }
            if (mqttConfig.enabled && !mqttReady) {
                mqttClient.begin(mqttConfig);
                mqttClient.setStatusCallback(onMqttStatusChange);
            }
        }
    } else if (wifiConnected) {
        wifiConnected = false;
        displayMgr.setWifiStatus(false);
        DEBUG_PRINTLN("[WiFi] Verbindung verloren");
    } else if (millis() - lastCheck > WIFI_RECONNECT_INTERVAL && strlen(wifiConfig.ssid) > 0) {
        lastCheck = millis();
        DEBUG_PRINTLN("[WiFi] Reconnect...");
        WiFi.disconnect();
        delay(100);
        WiFi.begin(wifiConfig.ssid, wifiConfig.password);
    }
}

// ============================================================
// ARDUINO OTA (Push von PlatformIO / esptool)
// ============================================================

void setupArduinoOta() {
    ArduinoOTA.setHostname(WIFI_HOSTNAME);
    ArduinoOTA.setPassword("sml_ota_2026");

    ArduinoOTA.onStart([]() {
        String t = ArduinoOTA.getCommand() == U_FLASH ? "Firmware" : "Filesystem";
        DEBUG_PRINTF("[OTA] ArduinoOTA Start: %s\n", t.c_str());
    });
    ArduinoOTA.onEnd([]() {
        DEBUG_PRINTLN("[OTA] ArduinoOTA fertig – Neustart");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        DEBUG_PRINTF("[OTA] Fortschritt: %u%%\r", progress * 100 / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        DEBUG_PRINTF("[OTA] Fehler[%u]: ", error);
        if      (error == OTA_AUTH_ERROR)    DEBUG_PRINTLN("Authentifizierung");
        else if (error == OTA_BEGIN_ERROR)   DEBUG_PRINTLN("Begin");
        else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect");
        else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive");
        else if (error == OTA_END_ERROR)     DEBUG_PRINTLN("End");
    });

    ArduinoOTA.begin();
    DEBUG_PRINTLN("[OTA] ArduinoOTA bereit (Port 3232, PW: sml_ota_2026)");
}

// ============================================================
// TASTER
// ============================================================

void setupButton() {
#if PIN_BUTTON_ENABLED
    pinMode(PIN_BUTTON, INPUT);
    DEBUG_PRINTLN("[Button] Taster an GPIO" + String(PIN_BUTTON));
#else
    DEBUG_PRINTLN("[Button] Taster deaktiviert (PIN_BUTTON_ENABLED=0)");
#endif
}

void checkButton() {
#if PIN_BUTTON_ENABLED
    int state = digitalRead(PIN_BUTTON);
    if (state == HIGH) {
        if (millis() - lastButtonPress > 3000 && !buttonLongPress) {
            buttonLongPress = true;
            displayMgr.resetPage();
        }
    } else {
        if (millis() - lastButtonPress < 3000 && !buttonLongPress && lastButtonPress > 0) {
            displayMgr.nextPage();
        }
        lastButtonPress = millis();
        buttonLongPress = false;
    }
#endif
}

// ============================================================
// MQTT
// ============================================================

void checkMqttPublish() {
    if (mqttConfig.publishInterval == 0) return;
    unsigned long interval = mqttConfig.publishInterval * 1000;
    if (millis() - lastMqttPublish >= interval) {
        const SmlData& d = smlReader.getData();
        if (d.isValid) mqttClient.publishSmlData(d);
        lastMqttPublish = millis();
    }
    // Diagnosewerte alle 60 Sekunden
    if (millis() - lastDiagPublish >= 60000) {
        mqttClient.publishDiagnostics(currentIp, wifiConnected);
        lastDiagPublish = millis();
    }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    DEBUG_PRINTLN("\n=======================================");
    DEBUG_PRINTLN("  SML-Display v" FIRMWARE_VERSION);
    DEBUG_PRINTLN("  Board: WT32-ETH01 V1.4");
    DEBUG_PRINTLN("  Zähler: " DEVICE_MODEL);
    DEBUG_PRINTLN("=======================================\n");

    // LittleFS
    if (!configMgr.begin()) DEBUG_PRINTLN("[ERROR] LittleFS fehlgeschlagen!");

    // Konfigurationen laden
    configMgr.loadWifiConfig(wifiConfig);
    configMgr.loadMqttConfig(mqttConfig);
    configMgr.loadDisplayConfig(displayConfig);
    configMgr.loadNetworkConfig(networkConfig);

    // Display
    displayMgr.begin();
    displayMgr.setContrast(displayConfig.contrast);
    displayMgr.setBrightness(displayConfig.brightness);

    // SML Reader
    smlReader.begin();

    // Button
    setupButton();

    // 1. Ethernet starten
    startEthernet();

    // 2. Webserver starten (immer verfügbar)
    webServer.begin(&smlReader);
    webServer.setWifiSaveCallback(onWifiSave);
    webServer.setMqttSaveCallback(onMqttSave);
    webServer.setNetworkSaveCallback(onNetworkSave);
    webServer.setMqttTestCallback(onMqttTest);
    webServer.setDisplayCallback(onIpDisplay);

    // 3. WiFi starten (wenn aktiviert und nicht LAN-only)
    if (networkConfig.wifiEnabled && networkConfig.mode != NET_MODE_LAN_ONLY) {
        if (strlen(wifiConfig.ssid) > 0) {
            startWifiSta();
        } else {
            // Kein WiFi gespeichert → AP-Mode nach 20 Min
        }
    } else if (!networkConfig.wifiEnabled) {
        DEBUG_PRINTLN("[WiFi] Deaktiviert durch Konfiguration");
        displayMgr.setWifiStatus(false);
    }

    // 4. ArduinoOTA
    setupArduinoOta();

    // 5. MQTT initialisieren (Verbindung wird im loop hergestellt sobald Netzwerk da)
    mqttClient.setStatusCallback(onMqttStatusChange);
    if (mqttConfig.enabled && strlen(mqttConfig.broker) > 0) {
        mqttClient.begin(mqttConfig);
        DEBUG_PRINTLN("[MQTT] Initialisiert");
    }

    DEBUG_PRINTLN("[Setup] Fertig!\n");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
    // 0. ArduinoOTA
    ArduinoOTA.handle();

    // 1. SML-Daten lesen
    smlReader.update();

    // 2. WebServer
    webServer.loop();
    webServer.setSmlReader(&smlReader);
    webServer.setNetworkStatus(ethConnected, wifiConnected, mqttReady);

    // 3. Display
    displayMgr.update(smlReader.getData());
    displayMgr.setLanStatus(ethConnected);
    displayMgr.setWifiStatus(wifiConnected);
    displayMgr.setMqttStatus(mqttReady);

    // 4. Netzwerk-Checks
    checkEthernet();
    checkWifiSta();

    // 5. MQTT
    if (mqttConfig.enabled) {
        mqttClient.loop();
        mqttReady = mqttClient.isConnected();
        checkMqttPublish();
    }

    // 6. Button
    checkButton();

    // 7. Debug (alle 10s)
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 10000) {
        if (smlReader.getData().isValid) smlReader.printDebug();
        DEBUG_PRINTF("[Status] LAN=%s WiFi=%s AP=%s MQTT=%s Heap=%d\n",
                     ethConnected ? "ja" : "nein",
                     wifiConnected ? "ja" : "nein",
                     webServer.isApMode() ? "ja" : "nein",
                     mqttReady ? "ja" : "nein",
                     ESP.getFreeHeap());
        lastDebug = millis();
    }

    delay(10);
}
