/**
 * webserver.cpp – WebServer für WT32-ETH01 (LAN + WiFi)
 */
#include "webserver.h"
#include <ArduinoJson.h>
#include <ETH.h>
#include <Update.h>

static const char* HTML_HEAD =
"<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>SML-Display Setup</title>"
"<style>body{font-family:system-ui,sans-serif;margin:0;padding:15px;background:#f0f2f5;color:#333}"
"h1{text-align:center;color:#1a73e8;margin:10px 0}h2{color:#555;font-size:1.1em;border-bottom:2px solid #1a73e8;padding-bottom:4px}"
".card{background:#fff;border-radius:12px;padding:16px;margin:12px 0;box-shadow:0 2px 8px rgba(0,0,0,0.1)}"
".btn{display:block;width:100%;padding:12px;background:#1a73e8;color:#fff;border:none;border-radius:8px;font-size:1em;cursor:pointer;text-align:center;text-decoration:none;margin:8px 0}"
".btn:hover{background:#1557b0}.btn.green{background:#0d904f}.btn.green:hover{background:#0a7040}"
".btn.red{background:#c5221f}.btn.red:hover{background:#a11d18}"
".net{padding:10px;margin:4px 0;background:#f8f9fa;border-radius:8px;cursor:pointer;border:1px solid #ddd}"
".net:hover{background:#e8f0fe;border-color:#1a73e8}.net strong{color:#1a73e8}.net small{color:#888}"
"input[type=text],input[type=password],input[type=number],select{width:100%;padding:10px;margin:4px 0 12px 0;border:1px solid #ddd;border-radius:8px;box-sizing:border-box;font-size:1em}"
".status{padding:6px 12px;border-radius:6px;font-weight:bold;display:inline-block}"
".ok{background:#e6f4ea;color:#0d904f}.err{background:#fce8e6;color:#c5221f}"
".data-row{display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid #eee}"
"label{display:flex;align-items:center;gap:8px;cursor:pointer;margin:8px 0}"
"</style></head><body>"
"<nav style='display:flex;justify-content:space-around;background:#333;padding:10px;border-radius:8px;margin-bottom:10px'>"
"<a href='/' style='color:#fff;text-decoration:none;padding:8px 12px;border-radius:4px'>Dashboard</a>"
"<a href='/wifi' style='color:#fff;text-decoration:none;padding:8px 12px;border-radius:4px'>WiFi</a>"
"<a href='/network' style='color:#fff;text-decoration:none;padding:8px 12px;border-radius:4px'>Netzwerk</a>"
"<a href='/mqtt' style='color:#fff;text-decoration:none;padding:8px 12px;border-radius:4px'>MQTT</a>"
"<a href='/ota' style='color:#fff;text-decoration:none;padding:8px 12px;border-radius:4px'>Firmware</a>"
"</nav>";

static const char* HTML_FOOTER = "</body></html>";

WebServerManager::WebServerManager()
    : server(nullptr), dnsServer(nullptr), apMode(false), staConnected(false),
      wifiDisabled(false), hasLan(false), smlDataRef(nullptr),
      wifiSaveCb(nullptr), mqttSaveCb(nullptr), netSaveCb(nullptr),
      mqttTestCb(nullptr), displayCb(nullptr),
      connectStartTime(0), lastClientCheck(0), scrollPos(0) {}

WebServerManager::~WebServerManager() {
    if (server) { delete server; server = nullptr; }
    if (dnsServer) { dnsServer->stop(); delete dnsServer; dnsServer = nullptr; }
}

bool WebServerManager::begin(const SmlData* smlData) {
    smlDataRef = smlData;
    connectStartTime = millis();
    server = new WiFiServer(80);
    server->begin();
    DEBUG_PRINTLN("[Web] Server gestartet auf Port 80");
    return false;
}

void WebServerManager::setWifiSaveCallback(WifiSaveCallback cb) { wifiSaveCb = cb; }
void WebServerManager::setMqttSaveCallback(MqttSaveCallback cb) { mqttSaveCb = cb; }
void WebServerManager::setNetworkSaveCallback(NetworkSaveCallback cb) { netSaveCb = cb; }
void WebServerManager::setMqttTestCallback(MqttTestCallback cb) { mqttTestCb = cb; }
void WebServerManager::setDisplayCallback(void (*cb)(const String&)) { displayCb = cb; }
void WebServerManager::setNetworkStatus(bool lan, bool wifi, bool mqtt) {
    hasLan = lan;
    staConnected = wifi;
    wifiDisabled = false; // wird extern gesetzt
}

String WebServerManager::getIp() const {
    if (hasLan && ETH.localIP()[0] != 0) return ETH.localIP().toString();
    if (staConnected) return WiFi.localIP().toString();
    if (apMode) return WiFi.softAPIP().toString();
    return "N/A";
}

// ============================================================

void WebServerManager::loop() {
    // 20-Minuten-AP-Fallback (nur wenn WLAN aktiv, kein LAN, kein STA)
    if (!wifiDisabled && !hasLan && !staConnected && !apMode &&
        millis() - connectStartTime > AP_FALLBACK_TIMEOUT_MS) {
        DEBUG_PRINTLN("[WiFi] 20 Min ohne Verbindung → starte AP-Mode");
        startApMode();
    }

    // DNS Captive Portal
    if (dnsServer) dnsServer->processNextRequest();

    // HTTP Clients bedienen
    if (server) {
        WiFiClient client = server->available();
        if (client) {
            lastClientCheck = millis();
            parseRequest(client);
            client.stop();
        }
    }
}

void WebServerManager::startApMode() {
    if (apMode) return;
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SML-Display", WIFI_AP_PASSWORD);
    WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    DEBUG_PRINTLN("[WiFi] AP-Mode: SML-Display @ 192.168.4.1");

    dnsServer = new DNSServer();
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(53, "*", WiFi.softAPIP());
    if (displayCb) displayCb("AP:192.168.4.1");
}

bool WebServerManager::startStaMode() {
    WifiConfig config;
    ConfigManager cfg; cfg.begin();
    if (!cfg.loadWifiConfig(config) || strlen(config.ssid) == 0) return false;

    DEBUG_PRINTF("[WiFi] Verbinde mit: %s\n", config.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.begin(config.ssid, config.password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) delay(250);

    if (WiFi.status() == WL_CONNECTED) {
        apMode = false; staConnected = true;
        if (dnsServer) { dnsServer->stop(); delete dnsServer; dnsServer = nullptr; }
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        DEBUG_PRINTF("[WiFi] Verbunden: %s\n", WiFi.localIP().toString().c_str());
        if (displayCb) displayCb(WiFi.localIP().toString());
        return true;
    }
    connectStartTime = millis();
    return false;
}

// ============================================================
// REQUEST PARSER
// ============================================================

void WebServerManager::parseRequest(WiFiClient& client) {
    String line = client.readStringUntil('\n'); line.trim();
    if (line.length() == 0) return;
    int sp = line.indexOf(' ');
    String method = line.substring(0, sp);
    String path = line.substring(sp + 1, line.lastIndexOf(' '));

    String headers; int contentLength = 0; String contentType;
    while (true) {
        String h = client.readStringUntil('\n'); h.trim();
        if (h.length() == 0) break;
        headers += h + "\n";
        if (h.startsWith("Content-Length:")) contentLength = h.substring(16).toInt();
        if (h.startsWith("Content-Type:"))  { contentType = h.substring(13); contentType.trim(); }
    }

    DEBUG_PRINTF("[HTTP] %s %s\n", method.c_str(), path.c_str());

    // OTA-Upload: Body NICHT puffern – direkt streaming
    if (path == "/ota/upload" && method == "POST") {
        handleOtaUpload(client, contentType, contentLength);
        return;
    }

    String body;
    if (contentLength > 0) {
        unsigned long ws = millis();
        while (client.available() < contentLength && millis() - ws < 3000) delay(10);
        for (int i = 0; i < contentLength && client.available(); i++) body += (char)client.read();
    }

    if (path == "/" || path == "/generate_204" || path == "/fwlink") handleRoot(client);
    else if (path == "/scan") handleWifiScan(client);
    else if (path == "/save" && method == "POST") handleWifiSave(client, body);
    else if (path == "/network") handleNetwork(client);
    else if (path == "/network/save" && method == "POST") handleNetworkSave(client, body);
    else if (path == "/mqtt") handleMqtt(client);
    else if (path == "/mqtt/save" && method == "POST") handleMqttSave(client, body);
    else if (path == "/mqtt/test" && method == "POST") handleMqttTest(client);
    else if (path == "/status") handleStatus(client);
    else if (path == "/reboot") handleReboot(client);
    else if (path == "/ota") handleOta(client);
    else if (path == "/api/data") handleApiData(client);
    else if (apMode) handleRoot(client);  // Captive Portal
    else handleRoot(client);
}

// ============================================================
// SEITEN
// ============================================================

void WebServerManager::handleRoot(WiFiClient& client) {
    String html = HTML_HEAD;
    html += "<h1>SML-Display Dashboard</h1>";

    // Verbindungsstatus
    html += "<div class='card'><h2>Status</h2>";
    if (hasLan) html += "<div class='data-row'><span>LAN:</span><span class='status ok'>Verbunden (" + ETH.localIP().toString() + ")</span></div>";
    else html += "<div class='data-row'><span>LAN:</span><span class='status err'>Nicht verbunden</span></div>";
    if (staConnected) html += "<div class='data-row'><span>WiFi:</span><span class='status ok'>Verbunden (" + WiFi.localIP().toString() + ")</span></div>";
    else if (wifiDisabled) html += "<div class='data-row'><span>WiFi:</span><span class='status err'>Deaktiviert</span></div>";
    else if (apMode) html += "<div class='data-row'><span>WiFi:</span><span class='status ok'>AP-Mode (192.168.4.1)</span></div>";
    else html += "<div class='data-row'><span>WiFi:</span><span class='status err'>Nicht verbunden</span></div>";

    html += "</div>";

    // AP-Fallback Countdown
    if (!hasLan && !staConnected && !apMode && !wifiDisabled) {
        uint32_t remaining = (AP_FALLBACK_TIMEOUT_MS - (millis() - connectStartTime)) / 60000;
        html += "<div class='card'><p>AP-Mode startet in: <strong>" + String(remaining) + " min</strong></p></div>";
    }

    // SML Daten
    if (smlDataRef && smlDataRef->isValid) {
        html += "<div class='card'><h2>Messwerte</h2>";
        for (int i = 0; i < smlDataRef->valueCount && i < 8; i++) {
            html += "<div class='data-row'><span>" + String(smlDataRef->values[i].obisCode) + "</span>";
            html += "<span>" + String(smlDataRef->values[i].value, 3) + " " + String(smlDataRef->values[i].unit) + "</span></div>";
        }
        html += "</div>";
    } else {
        html += "<div class='card'><p>Warte auf SML-Daten...</p></div>";
    }

    html += "<div class='card'><a class='btn green' href='/reboot'>&#x21BB; Neustart</a></div>";
    html += HTML_FOOTER;

    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
}

void WebServerManager::handleWifiScan(WiFiClient& client) {
    int n = WiFi.scanNetworks();
    String html = HTML_HEAD;
    html += "<h1>WiFi Netzwerke</h1><div class='card'><h2>Gefunden (" + String(n) + ")</h2>";
    if (n == 0) html += "<p>Keine Netzwerke.</p>";
    else {
        for (int i = 0; i < n; i++) {
            bool dup = false;
            for (int j = 0; j < i; j++) if (WiFi.SSID(j) == WiFi.SSID(i)) { dup = true; break; }
            if (dup) continue;
            String ssid = WiFi.SSID(i); ssid.replace("%", "%25"); ssid.replace(" ", "%20");
            int bars = map(constrain(WiFi.RSSI(i), -100, -30), -100, -30, 1, 4);
            String signal; for (int b = 0; b < 4; b++) signal += (b < bars) ? "&#x2588;" : "&#x2591;";
            html += "<div class='net' onclick=\"document.getElementById('ssid').value=decodeURIComponent('" + ssid + "');document.getElementById('pw').focus()\">";
            html += "<strong>" + WiFi.SSID(i) + "</strong> <small>" + signal + " " + String(WiFi.RSSI(i)) + " dBm</small><br>";
            html += "<small>" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "&#x1F512;" : "&#x1F513;") + "</small></div>";
        }
    }
    html += "</div><div class='card'><h2>Verbinden</h2><form action='/save' method='post'>";
    html += "<label>SSID:</label><input type='text' id='ssid' name='ssid' required>";
    html += "<label>Passwort:</label><input type='password' id='pw' name='password'>";
    html += "<button class='btn' type='submit'>&#x1F4F6; Verbinden</button></form>";
    html += "<a href='/' style='color:#1a73e8'>← Zur&uuml;ck</a></div>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
    WiFi.scanDelete();
}

void WebServerManager::handleWifiSave(WiFiClient& client, const String& body) {
    String ssid = urlDecode(getRequestParam(body, "ssid"));
    String pw = urlDecode(getRequestParam(body, "password"));
    WifiConfig config; memset(&config, 0, sizeof(WifiConfig));
    strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid)-1);
    strncpy(config.password, pw.c_str(), sizeof(config.password)-1);
    config.dhcp = true;
    ConfigManager cfg; cfg.begin(); cfg.saveWifiConfig(config);
    if (wifiSaveCb) wifiSaveCb(config);

    String html = HTML_HEAD;
    html += "<h1>Verbindung wird aufgebaut</h1><div class='card'>";
    html += "<p>SSID: <strong>" + ssid + "</strong></p><meta http-equiv='refresh' content='15;url=/status'>";
    html += "</div>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
    delay(500);
    startStaMode();
}

void WebServerManager::handleNetwork(WiFiClient& client) {
    NetworkConfig nc;
    ConfigManager cfg; cfg.begin(); cfg.loadNetworkConfig(nc);
    String html = HTML_HEAD;
    html += "<h1>Netzwerk</h1><div class='card'><h2>Status</h2>";
    html += "<div class='data-row'><span>LAN:</span><span class='status " + String(hasLan ? "ok'>Verbunden" : "err'>Nicht verbunden") + "</span></div>";
    html += "<div class='data-row'><span>WiFi:</span><span class='status " + String(staConnected ? "ok'>Verbunden" : "err'>Nicht verbunden") + "</span></div>";
    if (apMode) html += "<div class='data-row'><span>AP-Mode:</span><span class='status ok'>Aktiv (192.168.4.1)</span></div>";
    html += "</div>";

    html += "<div class='card'><h2>Modus</h2><form action='/network/save' method='post'>";
    html += "<label><input type='radio' name='mode' value='0'" + String(nc.mode==NET_MODE_AUTO?" checked":"") + "> Auto (LAN zuerst, dann WiFi)</label>";
    html += "<label><input type='radio' name='mode' value='1'" + String(nc.mode==NET_MODE_LAN_ONLY?" checked":"") + "> Nur LAN</label>";
    html += "<label><input type='radio' name='mode' value='2'" + String(nc.mode==NET_MODE_WIFI_ONLY?" checked":"") + "> Nur WiFi</label>";
    html += "<button class='btn' type='submit'>Speichern</button></form></div>";

    html += "<div class='card'><h2>WiFi deaktivieren</h2>";
    html += "<p>Wenn WiFi deaktiviert wird, steht nach 20 Min ohne LAN-Verbindung kein Einrichtungsportal mehr zur Verf&uuml;gung.</p>";
    html += "<form action='/network/save' method='post'>";
    html += "<input type='hidden' name='mode' value='" + String((int)nc.mode) + "'>";
    if (nc.wifiEnabled) {
        html += "<input type='hidden' name='wifiEnabled' value='0'>";
        html += "<button class='btn red' type='submit'>&#x1F4F6; WiFi deaktivieren</button>";
    } else {
        html += "<input type='hidden' name='wifiEnabled' value='1'>";
        html += "<button class='btn green' type='submit'>&#x1F4F6; WiFi aktivieren</button>";
    }
    html += "</div>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
}

void WebServerManager::handleNetworkSave(WiFiClient& client, const String& body) {
    NetworkConfig nc;
    nc.mode = (NetworkMode)getRequestParam(body, "mode").toInt();
    nc.wifiEnabled = getRequestParam(body, "wifiEnabled") == "1";
    ConfigManager cfg; cfg.begin(); cfg.saveNetworkConfig(nc);
    if (netSaveCb) netSaveCb(nc);

    if (!nc.wifiEnabled && (WiFi.getMode() == WIFI_AP || staConnected)) {
        DEBUG_PRINTLN("[WiFi] Deaktiviert durch Konfiguration");
        if (dnsServer) { dnsServer->stop(); delete dnsServer; dnsServer = nullptr; }
        WiFi.mode(WIFI_OFF);
        staConnected = false; apMode = false;
    } else if (nc.mode == NET_MODE_WIFI_ONLY && !staConnected) {
        startStaMode();
    }

    String html = HTML_HEAD;
    html += "<h1>Gespeichert</h1><div class='card'><p>Netzwerk-Einstellungen gespeichert.</p>";
    html += "<a href='/network' style='display:block;text-align:center;padding:10px;background:#4caf50;color:#fff;text-decoration:none;border-radius:4px;margin:10px 0'>Zurueck</a></div>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
}

void WebServerManager::handleMqtt(WiFiClient& client) {
    MqttConfig mc; ConfigManager cfg; cfg.begin(); cfg.loadMqttConfig(mc);
    String html = HTML_HEAD;
    html += "<h1>MQTT</h1><div class='card'><form action='/mqtt/save' method='POST'>";
    html += "<label>Broker:</label><input type='text' name='broker' value='" + String(mc.broker) + "'>";
    html += "<label>Port:</label><input type='number' name='port' value='" + String(mc.port) + "'>";
    html += "<label>Benutzer:</label><input type='text' name='username' value='" + String(mc.username) + "'>";
    html += "<label>Passwort:</label><input type='password' name='password' value='" + String(mc.password) + "'>";
    html += "<label>Client-ID:</label><input type='text' name='clientId' value='" + String(mc.clientId) + "'>";
    html += "<label>Topic-Praefix:</label><input type='text' name='topicPrefix' value='" + String(mc.topicPrefix) + "'>";
    html += "<label>Intervall (s):</label><input type='number' name='publishInterval' value='" + String(mc.publishInterval) + "'>";
    html += "<label><input type='checkbox' name='autoDiscovery' value='1'" + String(mc.autoDiscovery?" checked":"") + "> HA Auto-Discovery</label>";
    html += "<label><input type='checkbox' name='enabled' value='1'" + String(mc.enabled?" checked":"") + "> Aktiviert</label>";
    html += "<button class='btn' type='submit'>Speichern</button></form></div>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
}

void WebServerManager::handleMqttSave(WiFiClient& client, const String& body) {
    MqttConfig mc; memset(&mc, 0, sizeof(MqttConfig));
    if (getRequestParam(body,"broker").length()) strncpy(mc.broker, getRequestParam(body,"broker").c_str(), sizeof(mc.broker)-1);
    mc.port = getRequestParam(body,"port").toInt();
    if (getRequestParam(body,"username").length()) strncpy(mc.username, getRequestParam(body,"username").c_str(), sizeof(mc.username)-1);
    if (getRequestParam(body,"password").length()) strncpy(mc.password, getRequestParam(body,"password").c_str(), sizeof(mc.password)-1);
    if (getRequestParam(body,"clientId").length()) strncpy(mc.clientId, getRequestParam(body,"clientId").c_str(), sizeof(mc.clientId)-1);
    if (getRequestParam(body,"topicPrefix").length()) strncpy(mc.topicPrefix, getRequestParam(body,"topicPrefix").c_str(), sizeof(mc.topicPrefix)-1);
    mc.publishInterval = getRequestParam(body,"publishInterval").toInt();
    mc.autoDiscovery = getRequestParam(body,"autoDiscovery") == "1";
    mc.enabled = getRequestParam(body,"enabled") == "1";
    ConfigManager cfg; cfg.begin(); cfg.saveMqttConfig(mc);
    if (mqttSaveCb) mqttSaveCb(mc);
    String html = HTML_HEAD;
    html += "<h1>MQTT gespeichert</h1><div class='card'><p>Gespeichert.</p>";
    html += "<a href='/mqtt' style='display:block;text-align:center;padding:10px;background:#4caf50;color:#fff;text-decoration:none;border-radius:4px;margin:10px 0'>Zurueck</a></div>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
}

void WebServerManager::handleMqttTest(WiFiClient& client) {
    if (mqttTestCb) mqttTestCb();
    client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK");
}

void WebServerManager::handleStatus(WiFiClient& client) {
    String html = HTML_HEAD;
    html += "<h1>Status</h1><div class='card'>";
    if (hasLan) html += "<p class='status ok'>LAN: " + ETH.localIP().toString() + "</p>";
    else if (staConnected) html += "<p class='status ok'>WiFi: " + WiFi.localIP().toString() + "</p>";
    else if (apMode) html += "<p class='status ok'>AP: 192.168.4.1</p>";
    else html += "<p class='status err'>Keine Verbindung</p>";
    html += "</div><div class='card'><a class='btn' href='/'>OK</a></div>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
}

void WebServerManager::handleReboot(WiFiClient& client) {
    String html = HTML_HEAD;
    html += "<h1>Neustart</h1><div class='card'><p>Ger&auml;t startet neu...</p>";
    html += "<meta http-equiv='refresh' content='10;url=/'>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
    delay(1000); ESP.restart();
}

void WebServerManager::handleApiData(WiFiClient& client) {
    JsonDocument doc;
    doc["lan"] = hasLan;
    doc["lanIp"] = hasLan ? ETH.localIP().toString() : "";
    doc["wifi"] = staConnected;
    doc["ap"] = apMode;
    doc["ip"] = getIp();
    if (smlDataRef && smlDataRef->isValid) {
        doc["valid"] = true;
        doc["lastUpdate"] = smlDataRef->lastMessageTime;
        JsonArray values = doc["values"].to<JsonArray>();
        for (int i = 0; i < smlDataRef->valueCount; i++) {
            JsonObject v = values.add<JsonObject>();
            v["obis"] = smlDataRef->values[i].obisCode;
            v["value"] = smlDataRef->values[i].value;
            v["unit"] = smlDataRef->values[i].unit;
        }
    } else doc["valid"] = false;
    doc["uptime"] = millis()/1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    String out; serializeJson(doc, out);
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n";
    resp += out; client.print(resp);
}

// ============================================================
// OTA
// ============================================================

void WebServerManager::handleOta(WiFiClient& client) {
    String html = HTML_HEAD;
    html += "<h1>Firmware Update</h1>";
    html += "<div class='card'><h2>OTA Upload</h2>";
    html += "<p>Aktuelle Version: <strong>" FIRMWARE_VERSION "</strong></p>";
    html += "<form id='otaForm' action='/ota/upload' method='POST' enctype='multipart/form-data'>";
    html += "<label>Firmware (.bin):</label>";
    html += "<input type='file' name='firmware' id='fwFile' accept='.bin' required style='margin-bottom:12px'>";
    html += "<button class='btn' type='submit' id='uploadBtn'>&#x1F4E4; Firmware hochladen</button>";
    html += "</form>";
    html += "<div id='progress' style='display:none;margin-top:12px'>";
    html += "<p id='msg'>Upload l&auml;uft, bitte warten...</p>";
    html += "<progress id='bar' value='0' max='100' style='width:100%;height:20px'></progress>";
    html += "<p id='pct'>0%</p></div>";
    html += "<script>";
    html += "document.getElementById('otaForm').onsubmit=function(e){";
    html += "e.preventDefault();";
    html += "var f=document.getElementById('fwFile').files[0];if(!f)return;";
    html += "document.getElementById('progress').style.display='block';";
    html += "document.getElementById('uploadBtn').disabled=true;";
    html += "var fd=new FormData();fd.append('firmware',f);";
    html += "var x=new XMLHttpRequest();";
    html += "x.upload.onprogress=function(e){if(e.lengthComputable){";
    html += "var p=Math.round(e.loaded/e.total*100);";
    html += "document.getElementById('bar').value=p;";
    html += "document.getElementById('pct').textContent=p+'%';}};";
    html += "x.onload=function(){document.open().write(x.responseText);};";
    html += "x.onerror=function(){alert('Verbindungsfehler beim Upload!');};";
    html += "x.open('POST','/ota/upload');x.send(fd);};";
    html += "</script>";
    html += "</div>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
}

void WebServerManager::handleOtaUpload(WiFiClient& client, const String& contentType, int contentLength) {
    // Boundary aus Content-Type extrahieren
    // z.B.: "multipart/form-data; boundary=----WebKitFormBoundaryXXX"
    int bIdx = contentType.indexOf("boundary=");
    if (bIdx == -1) {
        client.print("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nKein Multipart-Boundary");
        return;
    }
    String actualBoundary = contentType.substring(bIdx + 9);
    actualBoundary.replace("\"", "");
    actualBoundary.trim();
    // Trailing im Body: "\r\n--<boundary>--\r\n" = 2+2+len+2+2 = len+8 Bytes
    int trailingLen = actualBoundary.length() + 8;

    // Multipart-Part-Header überspringen (bis \r\n\r\n)
    int headerBytes = 0;
    uint8_t prev[4] = {0, 0, 0, 0};
    bool headersDone = false;
    unsigned long t0 = millis();
    while (!headersDone && millis() - t0 < 15000) {
        if (!client.available()) { delay(1); continue; }
        uint8_t b = client.read();
        headerBytes++;
        prev[0] = prev[1]; prev[1] = prev[2]; prev[2] = prev[3]; prev[3] = b;
        if (prev[0] == '\r' && prev[1] == '\n' && prev[2] == '\r' && prev[3] == '\n')
            headersDone = true;
        if (headerBytes > 1024) break;
    }

    if (!headersDone) {
        client.print("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nMultipart-Header nicht gefunden");
        return;
    }

    int firmwareSize = contentLength - headerBytes - trailingLen;
    DEBUG_PRINTF("[OTA] Starte Update: %d Bytes (header=%d trailing=%d)\n",
                 firmwareSize, headerBytes, trailingLen);

    if (firmwareSize <= 0 || firmwareSize > 2000000) {
        client.print("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nUngueltige Groesse");
        return;
    }

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        String err = Update.errorString();
        DEBUG_PRINTF("[OTA] begin() Fehler: %s\n", err.c_str());
        client.print("HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nUpdate.begin() fehlgeschlagen: " + err);
        return;
    }

    // Firmware bytes streaming
    const int BUF_SIZE = 512;
    uint8_t buf[BUF_SIZE];
    int written = 0;
    t0 = millis();

    while (written < firmwareSize && millis() - t0 < 120000) {
        if (!client.available()) { delay(1); continue; }
        int toRead = min((int)client.available(), min(BUF_SIZE, firmwareSize - written));
        int n = client.readBytes(buf, toRead);
        if (n > 0) {
            if ((int)Update.write(buf, n) != n) {
                String err = Update.errorString();
                DEBUG_PRINTF("[OTA] write() Fehler: %s\n", err.c_str());
                client.print("HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nUpdate.write() fehlgeschlagen: " + err);
                return;
            }
            written += n;
        }
    }

    if (written != firmwareSize || !Update.end(true)) {
        String err = Update.errorString();
        DEBUG_PRINTF("[OTA] Fehler: written=%d/%d err=%s\n", written, firmwareSize, err.c_str());
        String html = String(HTML_HEAD);
        html += "<h1>OTA Fehler</h1><div class='card'>";
        html += "<p class='status err'>Update fehlgeschlagen: " + err + "</p>";
        html += "<p>Geschrieben: " + String(written) + " / " + String(firmwareSize) + " Bytes</p>";
        html += "<a class='btn' href='/ota'>Zur&uuml;ck</a></div>";
        html += HTML_FOOTER;
        String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
        resp += html; client.print(resp);
        return;
    }

    DEBUG_PRINTLN("[OTA] Update erfolgreich! Neustart...");
    String html = String(HTML_HEAD);
    html += "<h1>Update erfolgreich!</h1><div class='card'>";
    html += "<p class='status ok'>Firmware wurde installiert.</p>";
    html += "<p>Ger&auml;t startet in <span id='t'>5</span> Sekunden neu...</p>";
    html += "<script>var i=5;setInterval(function(){document.getElementById('t').textContent=--i;if(i<=0)location.href='/';},1000);</script>";
    html += "</div>";
    html += HTML_FOOTER;
    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    resp += html; client.print(resp);
    client.flush();
    delay(2000);
    ESP.restart();
}

String WebServerManager::getRequestParam(const String& req, const char* param) {
    String s = String(param) + "=";
    int start = req.indexOf(s);
    if (start == -1) return "";
    start += s.length();
    int end = req.indexOf('&', start);
    if (end == -1) end = req.length();
    return req.substring(start, end);
}

String WebServerManager::urlDecode(const String& str) {
    String r = str; r.replace("+", " ");
    int i = 0;
    while (i < r.length()-2) {
        if (r[i] == '%') {
            String hex = r.substring(i+1, i+3);
            char c = (char)strtol(hex.c_str(), nullptr, 16);
            r = r.substring(0, i) + String(c) + r.substring(i+3);
        } else i++;
    }
    return r;
}
