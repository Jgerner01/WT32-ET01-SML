/**
 * mqtt_client.h - MQTT Client Interface mit Home Assistant Auto-Discovery
 */

#ifndef MQTT_CLIENT_WRAPPER_H
#define MQTT_CLIENT_WRAPPER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "config.h"
#include "config_manager.h"
#include "sml_reader.h"

typedef void (*MqttStatusCallback)(bool connected);

class MqttClientManager {
public:
    MqttClientManager();
    ~MqttClientManager();

    bool begin(const MqttConfig& config);
    void loop();

    // Publiziert alle SML-Messwerte; sendet OBIS-Discovery dynamisch bei neuen Codes
    void publishSmlData(const SmlData& smlData);

    // Publiziert Diagnosewerte (IP, RSSI, Heap, Uptime)
    void publishDiagnostics(const String& ipAddress, bool wifiConnected);

    // Sendet Home Assistant Auto-Discovery (Diagnose-Sensoren)
    void sendDiscoveryConfig();

    bool testConnection();
    bool isConnected() const;
    void setStatusCallback(MqttStatusCallback callback);
    void disconnect();

private:
    WiFiClient wifiClient;
    PubSubClient* mqttClient;
    MqttConfig mqttConfig;
    MqttStatusCallback statusCb;
    bool connected;
    unsigned long lastPublish;
    unsigned long lastReconnect;
    bool diagnosticDiscoverySent;

    // Dynamisches OBIS-Discovery-Tracking
    char discoveredObisCodes[SML_MAX_OBIS_VALUES][MAX_OBIS_CODE_LEN];
    int discoveredObisCount;

    bool connect();
    void sendDiagnosticDiscovery();
    void sendDiagnosticSensor(const char* sensorId, const char* name,
                               const char* deviceClass, const char* stateClass,
                               const char* unit);
    void checkObisDiscovery(const SmlData& smlData);
    void sendObisDiscovery(const char* obisCode, const char* unit);
    bool isObisDiscovered(const char* obisCode) const;
    void markObisDiscovered(const char* obisCode);
};

#endif // MQTT_CLIENT_WRAPPER_H
