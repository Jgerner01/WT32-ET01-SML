/**
 * mqtt_client.cpp - MQTT Client Implementierung mit dynamischem HA Auto-Discovery
 */

#include "mqtt_client.h"

// ============================================================
// OBIS-Metadaten für HA Auto-Discovery
// ============================================================

struct ObisMetadata {
    const char* code;
    const char* name;
    const char* deviceClass;   // nullptr = kein device_class
    const char* stateClass;
};

static const ObisMetadata OBIS_META[] = {
    {"1-0:1.8.0*255",  "Bezug Gesamt",       "energy",     "total_increasing"},
    {"1-0:2.8.0*255",  "Einspeisung Gesamt", "energy",     "total_increasing"},
    {"1-0:16.7.0*255", "Wirkleistung",       "power",      "measurement"},
    {"1-0:36.7.0*255", "Wirkleistung L1",    "power",      "measurement"},
    {"1-0:56.7.0*255", "Wirkleistung L2",    "power",      "measurement"},
    {"1-0:76.7.0*255", "Wirkleistung L3",    "power",      "measurement"},
    {"1-0:32.7.0*255", "Spannung L1",        "voltage",    "measurement"},
    {"1-0:52.7.0*255", "Spannung L2",        "voltage",    "measurement"},
    {"1-0:72.7.0*255", "Spannung L3",        "voltage",    "measurement"},
    {"1-0:31.7.0*255", "Strom L1",           "current",    "measurement"},
    {"1-0:51.7.0*255", "Strom L2",           "current",    "measurement"},
    {"1-0:71.7.0*255", "Strom L3",           "current",    "measurement"},
    {"1-0:14.7.0*255", "Netzfrequenz",       "frequency",  "measurement"},
};
static const int OBIS_META_COUNT = (int)(sizeof(OBIS_META) / sizeof(OBIS_META[0]));

// OBIS-Code → MQTT-Topic-Suffix: "1-0:1.8.0*255" → "1_0_1_8_0_255"
static String obisToSuffix(const char* obisCode) {
    String s = obisCode;
    s.replace("-", "_");
    s.replace(":", "_");
    s.replace(".", "_");
    s.replace("*", "_");
    return s;
}

// Gemeinsamer Device-Block für alle Discovery-Payloads
static String deviceBlock() {
    String d = "\"device\":{";
    d += "\"identifiers\":[\"" DEVICE_ID "\"],";
    d += "\"name\":\"" DEVICE_NAME "\",";
    d += "\"model\":\"" DEVICE_MODEL "\",";
    d += "\"manufacturer\":\"" DEVICE_MANUFACTURER "\",";
    d += "\"sw_version\":\"" FIRMWARE_VERSION "\"";
    d += "}";
    return d;
}

// ============================================================

MqttClientManager::MqttClientManager()
    : mqttClient(nullptr), connected(false), lastPublish(0),
      lastReconnect(0), diagnosticDiscoverySent(false), discoveredObisCount(0),
      statusCb(nullptr) {
    memset(&mqttConfig, 0, sizeof(MqttConfig));
    memset(discoveredObisCodes, 0, sizeof(discoveredObisCodes));
}

MqttClientManager::~MqttClientManager() {
    disconnect();
    if (mqttClient) { delete mqttClient; mqttClient = nullptr; }
}

bool MqttClientManager::begin(const MqttConfig& config) {
    mqttConfig = config;

    if (mqttClient) { delete mqttClient; mqttClient = nullptr; }

    mqttClient = new PubSubClient(wifiClient);
    mqttClient->setServer(config.broker, config.port);
    mqttClient->setBufferSize(2048);

    connected = false;
    diagnosticDiscoverySent = false;
    discoveredObisCount = 0;
    memset(discoveredObisCodes, 0, sizeof(discoveredObisCodes));

    DEBUG_PRINTF("[MQTT] Initialisiert: Broker=%s:%d\n", config.broker, config.port);
    return true;
}

void MqttClientManager::loop() {
    if (!mqttClient || !mqttConfig.enabled) return;

    if (!mqttClient->connected()) {
        if (connected) {
            connected = false;
            // Discovery-Status zurücksetzen, damit nach Reconnect neu gesendet wird
            diagnosticDiscoverySent = false;
            discoveredObisCount = 0;
            memset(discoveredObisCodes, 0, sizeof(discoveredObisCodes));
            DEBUG_PRINTLN("[MQTT] Verbindung verloren");
            if (statusCb) statusCb(false);
        }
        if (millis() - lastReconnect >= MQTT_RECONNECT_INTERVAL) {
            lastReconnect = millis();
            connect();
        }
    } else {
        if (!connected) {
            connected = true;
            DEBUG_PRINTLN("[MQTT] Verbunden");
            if (statusCb) statusCb(true);

            if (mqttConfig.autoDiscovery && !diagnosticDiscoverySent) {
                sendDiagnosticDiscovery();
                diagnosticDiscoverySent = true;
            }
        }
        mqttClient->loop();
    }
}

bool MqttClientManager::connect() {
    if (!mqttClient) return false;

    String willTopic = String(mqttConfig.topicPrefix) + "/status";

    bool result;
    if (strlen(mqttConfig.username) > 0) {
        result = mqttClient->connect(
            mqttConfig.clientId, mqttConfig.username, mqttConfig.password,
            willTopic.c_str(), MQTT_QOS, MQTT_RETAIN, "offline"
        );
    } else {
        result = mqttClient->connect(
            mqttConfig.clientId,
            willTopic.c_str(), MQTT_QOS, MQTT_RETAIN, "offline"
        );
    }

    if (result) {
        mqttClient->publish(willTopic.c_str(), "online", MQTT_RETAIN);
        DEBUG_PRINTLN("[MQTT] Online-Status gesendet");
    } else {
        DEBUG_PRINTF("[MQTT] Verbindung fehlgeschlagen (rc=%d), retry in %d ms\n",
                     mqttClient->state(), MQTT_RECONNECT_INTERVAL);
    }
    return result;
}

// ============================================================
// MESSWERTE
// ============================================================

void MqttClientManager::publishSmlData(const SmlData& smlData) {
    if (!connected || !mqttClient) return;

    // Discovery für neue OBIS-Codes senden (dynamisch, einmalig pro Code)
    if (mqttConfig.autoDiscovery) {
        checkObisDiscovery(smlData);
    }

    String baseTopic = String(mqttConfig.topicPrefix) + "/sensors";

    for (int i = 0; i < smlData.valueCount; i++) {
        String suffix = obisToSuffix(smlData.values[i].obisCode);
        String topic = baseTopic + "/" + suffix;

        String payload = "{\"value\":";
        payload += String(smlData.values[i].value, 3);
        payload += ",\"unit\":\"";
        payload += smlData.values[i].unit;
        payload += "\",\"ts\":";
        payload += millis() / 1000;
        payload += "}";

        mqttClient->publish(topic.c_str(), payload.c_str(), MQTT_RETAIN);
    }
}

// ============================================================
// DIAGNOSEWERTE
// ============================================================

void MqttClientManager::publishDiagnostics(const String& ipAddress, bool wifiConnected) {
    if (!connected || !mqttClient) return;

    String base = String(mqttConfig.topicPrefix) + "/diagnostic";

    // IP-Adresse
    mqttClient->publish((base + "/ip").c_str(),
        ("{\"value\":\"" + ipAddress + "\"}").c_str(), MQTT_RETAIN);

    // WiFi RSSI (nur bei aktiver WiFi-Verbindung sinnvoll)
    if (wifiConnected) {
        int rssi = WiFi.RSSI();
        mqttClient->publish((base + "/rssi").c_str(),
            ("{\"value\":" + String(rssi) + "}").c_str(), MQTT_RETAIN);
    }

    // Freier Heap-Speicher
    mqttClient->publish((base + "/heap").c_str(),
        ("{\"value\":" + String(ESP.getFreeHeap()) + "}").c_str(), MQTT_RETAIN);

    // Betriebszeit in Sekunden
    mqttClient->publish((base + "/uptime").c_str(),
        ("{\"value\":" + String(millis() / 1000) + "}").c_str(), MQTT_RETAIN);
}

// ============================================================
// AUTO-DISCOVERY: OBIS (dynamisch)
// ============================================================

bool MqttClientManager::isObisDiscovered(const char* obisCode) const {
    for (int i = 0; i < discoveredObisCount; i++) {
        if (strcmp(discoveredObisCodes[i], obisCode) == 0) return true;
    }
    return false;
}

void MqttClientManager::markObisDiscovered(const char* obisCode) {
    if (discoveredObisCount < SML_MAX_OBIS_VALUES) {
        strncpy(discoveredObisCodes[discoveredObisCount], obisCode, MAX_OBIS_CODE_LEN - 1);
        discoveredObisCodes[discoveredObisCount][MAX_OBIS_CODE_LEN - 1] = '\0';
        discoveredObisCount++;
    }
}

void MqttClientManager::checkObisDiscovery(const SmlData& smlData) {
    for (int i = 0; i < smlData.valueCount; i++) {
        if (!isObisDiscovered(smlData.values[i].obisCode)) {
            sendObisDiscovery(smlData.values[i].obisCode, smlData.values[i].unit);
            markObisDiscovered(smlData.values[i].obisCode);
            delay(50);
        }
    }
}

void MqttClientManager::sendObisDiscovery(const char* obisCode, const char* unit) {
    // Metadaten aus Tabelle, Fallback für unbekannte OBIS-Codes
    const char* name       = obisCode;
    const char* deviceClass = nullptr;
    const char* stateClass = "measurement";

    for (int i = 0; i < OBIS_META_COUNT; i++) {
        if (strcmp(OBIS_META[i].code, obisCode) == 0) {
            name        = OBIS_META[i].name;
            deviceClass = OBIS_META[i].deviceClass;
            stateClass  = OBIS_META[i].stateClass;
            break;
        }
    }

    String suffix      = obisToSuffix(obisCode);
    String uniqueId    = String(DEVICE_ID) + "_" + suffix;
    String discTopic   = "homeassistant/sensor/" + uniqueId + "/config";
    String stateTopic  = String(mqttConfig.topicPrefix) + "/sensors/" + suffix;
    String availTopic  = String(mqttConfig.topicPrefix) + "/status";

    String p = "{";
    p += "\"name\":\"" + String(name) + "\",";
    p += "\"unique_id\":\"" + uniqueId + "\",";
    p += "\"state_topic\":\"" + stateTopic + "\",";
    p += "\"availability_topic\":\"" + availTopic + "\",";
    p += "\"payload_available\":\"online\",";
    p += "\"payload_not_available\":\"offline\",";
    p += "\"value_template\":\"{{ value_json.value }}\",";
    if (strlen(unit) > 0) p += "\"unit_of_measurement\":\"" + String(unit) + "\",";
    if (deviceClass)      p += "\"device_class\":\"" + String(deviceClass) + "\",";
    p += "\"state_class\":\"" + String(stateClass) + "\",";
    p += deviceBlock();
    p += "}";

    mqttClient->publish(discTopic.c_str(), p.c_str(), true);
    DEBUG_PRINTF("[MQTT] OBIS Discovery: %s → %s\n", obisCode, name);
}

// ============================================================
// AUTO-DISCOVERY: DIAGNOSE
// ============================================================

void MqttClientManager::sendDiscoveryConfig() {
    sendDiagnosticDiscovery();
    diagnosticDiscoverySent = true;
}

void MqttClientManager::sendDiagnosticDiscovery() {
    if (!connected || !mqttClient) return;
    DEBUG_PRINTLN("[MQTT] Sende Diagnose Auto-Discovery");

    sendDiagnosticSensor("ip",     "IP-Adresse",   nullptr,           "measurement", "");
    delay(50);
    sendDiagnosticSensor("rssi",   "WiFi Signal",  "signal_strength", "measurement", "dBm");
    delay(50);
    sendDiagnosticSensor("heap",   "Freier Heap",  nullptr,           "measurement", "B");
    delay(50);
    sendDiagnosticSensor("uptime", "Betriebszeit", "duration",        "measurement", "s");
}

void MqttClientManager::sendDiagnosticSensor(const char* sensorId, const char* name,
                                               const char* deviceClass, const char* stateClass,
                                               const char* unit) {
    String uniqueId   = String(DEVICE_ID) + "_diag_" + sensorId;
    String discTopic  = "homeassistant/sensor/" + uniqueId + "/config";
    String stateTopic = String(mqttConfig.topicPrefix) + "/diagnostic/" + sensorId;
    String availTopic = String(mqttConfig.topicPrefix) + "/status";

    String p = "{";
    p += "\"name\":\"" + String(name) + "\",";
    p += "\"unique_id\":\"" + uniqueId + "\",";
    p += "\"state_topic\":\"" + stateTopic + "\",";
    p += "\"availability_topic\":\"" + availTopic + "\",";
    p += "\"payload_available\":\"online\",";
    p += "\"payload_not_available\":\"offline\",";
    p += "\"value_template\":\"{{ value_json.value }}\",";
    p += "\"entity_category\":\"diagnostic\",";
    if (strlen(unit) > 0) p += "\"unit_of_measurement\":\"" + String(unit) + "\",";
    if (deviceClass)      p += "\"device_class\":\"" + String(deviceClass) + "\",";
    p += "\"state_class\":\"" + String(stateClass) + "\",";
    p += deviceBlock();
    p += "}";

    mqttClient->publish(discTopic.c_str(), p.c_str(), true);
}

// ============================================================

bool MqttClientManager::testConnection() {
    if (!mqttClient) return false;
    return connect();
}

bool MqttClientManager::isConnected() const {
    return connected && mqttClient && mqttClient->connected();
}

void MqttClientManager::setStatusCallback(MqttStatusCallback callback) {
    statusCb = callback;
}

void MqttClientManager::disconnect() {
    if (mqttClient && mqttClient->connected()) {
        String willTopic = String(mqttConfig.topicPrefix) + "/status";
        mqttClient->publish(willTopic.c_str(), "offline", MQTT_RETAIN);
        mqttClient->disconnect();
    }
    connected = false;
}
