/**
 * webserver.h – WebServer (WiFiServer-basiert)
 * Unterstützt LAN und WiFi, WLAN deaktivierbar
 */
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include "config.h"
#include "config_manager.h"
#include "sml_reader.h"

typedef void (*WifiSaveCallback)(const WifiConfig& config);
typedef void (*MqttSaveCallback)(const MqttConfig& config);
typedef void (*NetworkSaveCallback)(const NetworkConfig& config);
typedef void (*MqttTestCallback)();

class WebServerManager {
public:
    WebServerManager();
    ~WebServerManager();

    bool begin(const SmlData* smlData);
    void loop();
    void setWifiSaveCallback(WifiSaveCallback cb);
    void setMqttSaveCallback(MqttSaveCallback cb);
    void setNetworkSaveCallback(NetworkSaveCallback cb);
    void setMqttTestCallback(MqttTestCallback cb);
    void setDisplayCallback(void (*cb)(const String& ip));
    void setNetworkStatus(bool hasLan, bool hasWifi, bool hasMqtt);
    void setSmlData(const SmlData* data) { smlDataRef = data; }

    bool isApMode() const { return apMode; }
    bool isConnected() const { return !apMode && staConnected; }
    bool isWifiDisabled() const { return wifiDisabled; }
    String getIp() const;

private:
    WiFiServer* server;
    DNSServer* dnsServer;
    bool apMode;
    bool staConnected;
    bool wifiDisabled;        // WLAN explizit deaktiviert
    bool hasLan;              // LAN aktiv
    const SmlData* smlDataRef;
    WifiSaveCallback wifiSaveCb;
    MqttSaveCallback mqttSaveCb;
    NetworkSaveCallback netSaveCb;
    MqttTestCallback mqttTestCb;
    void (*displayCb)(const String&);
    unsigned long connectStartTime;
    unsigned long lastClientCheck;
    int scrollPos;

    static const uint32_t AP_FALLBACK_TIMEOUT_MS = 1200000;  // 20 Min

    void startApMode();
    bool startStaMode();
    void handleClient();
    void parseRequest(WiFiClient& client);
    void sendHtml(WiFiClient& client, int code, const char* body);
    void handleRoot(WiFiClient& client);
    void handleWifiScan(WiFiClient& client);
    void handleWifiSave(WiFiClient& client, const String& body);
    void handleNetwork(WiFiClient& client);
    void handleNetworkSave(WiFiClient& client, const String& body);
    void handleMqtt(WiFiClient& client);
    void handleMqttSave(WiFiClient& client, const String& body);
    void handleMqttTest(WiFiClient& client);
    void handleStatus(WiFiClient& client);
    void handleReboot(WiFiClient& client);
    void handleApiData(WiFiClient& client);
    void handleOta(WiFiClient& client);
    void handleOtaUpload(WiFiClient& client, const String& contentType, int contentLength);
    String getRequestParam(const String& request, const char* param);
    String urlDecode(const String& str);
};

#endif
