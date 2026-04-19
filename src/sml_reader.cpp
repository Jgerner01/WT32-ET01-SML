/**
 * sml_reader.cpp - SML-Lesekopf Implementierung
 * Liest SML-Daten vom IR-Lesekopf über UART2 und parst sie
 */

#include "sml_reader.h"
#include <cstring>

// SML Parser Callback Struktur
static SmlReader* g_smlReaderInstance = nullptr;

// Externe SML Parser Callbacks (aus libsml)
// Wir implementieren einen einfachen eigenen Parser da libsml komplex ist

SmlReader::SmlReader()
    : smlSerial(nullptr), bufferPos(0), lastRawLen(0), totalBytesReceived(0) {
    memset(&smlData, 0, sizeof(SmlData));
    memset(lastRawMsg, 0, sizeof(lastRawMsg));
    smlData.valueCount = 0;
    smlData.isValid = false;
    g_smlReaderInstance = this;
}

SmlReader::~SmlReader() {
    if (smlSerial) {
        delete smlSerial;
        smlSerial = nullptr;
    }
}

void SmlReader::begin() {
    // UART2 initialisieren
    smlSerial = new HardwareSerial(2);
    smlSerial->begin(SML_BAUD_RATE, SERIAL_8N1, PIN_SML_RX, PIN_SML_TX);
    
    DEBUG_PRINTLN("[SML] UART2 initialisiert (RX=GPIO" + String(PIN_SML_RX) + 
                  ", TX=GPIO" + String(PIN_SML_TX) + ", Baud=" + String(SML_BAUD_RATE) + ")");
}

bool SmlReader::update() {
    bool newDataAvailable = false;
    
    // Verfügbare Bytes lesen
    while (smlSerial->available()) {
        if (bufferPos < SML_RX_BUFFER_SIZE) {
            rxBuffer[bufferPos++] = smlSerial->read();
            totalBytesReceived++;
        } else {
            // Buffer voll, zurücksetzen
            bufferPos = 0;
        }
        
        // SML-Frame-Struktur:
        //   Start:  1B 1B 1B 1B 01 01 01 01  (8 Bytes)
        //   Ende:   1B 1B 1B 1B 1A xx CRC_L CRC_H  (8 Bytes)
        // Erkennung: warte bis alle 8 Ende-Bytes empfangen sind
        if (bufferPos >= 16) {
            if (bufferPos >= 8 &&
                rxBuffer[bufferPos-8] == 0x1B &&
                rxBuffer[bufferPos-7] == 0x1B &&
                rxBuffer[bufferPos-6] == 0x1B &&
                rxBuffer[bufferPos-5] == 0x1B &&
                rxBuffer[bufferPos-4] == 0x1A) {  // Ende-Marker: 1A = end-of-message

                // Raw-Buffer für Debug sichern
                int copyLen = min(bufferPos, SML_RX_BUFFER_SIZE);
                memcpy(lastRawMsg, rxBuffer, copyLen);
                lastRawLen = copyLen;

                // Nachricht parsen
                if (parseSmlMessage(rxBuffer, bufferPos)) {
                    newDataAvailable = true;
                    smlData.lastMessageTime = millis();
                    smlData.isValid = true;
                }

                // Buffer zurücksetzen
                bufferPos = 0;
            }
            
            // Buffer-Overflow-Schutz: Wenn wir zu nah am Limit sind
            // und kein Nachrichtenende gefunden wurde, zurücksetzen
            if (bufferPos >= SML_RX_BUFFER_SIZE - 50) {
                bufferPos = 0;
            }
        }
    }
    
    return newDataAvailable;
}

const SmlData& SmlReader::getData() const {
    return smlData;
}

const ObisValue* SmlReader::getObisValue(const char* obisCode) const {
    for (int i = 0; i < smlData.valueCount; i++) {
        if (strcmp(smlData.values[i].obisCode, obisCode) == 0) {
            return &smlData.values[i];
        }
    }
    return nullptr;
}

float SmlReader::getActivePower() const {
    const ObisValue* val = getObisValue(OBIS_POWER_ACTIVE);
    return val ? val->value : 0.0f;
}

float SmlReader::getImportTotal() const {
    const ObisValue* val = getObisValue(OBIS_ENERGY_IMPORT_TOTAL);
    return val ? val->value : 0.0f;
}

float SmlReader::getExportTotal() const {
    const ObisValue* val = getObisValue(OBIS_ENERGY_EXPORT_TOTAL);
    return val ? val->value : 0.0f;
}

void SmlReader::printDebug() const {
    if (!smlData.isValid) {
        DEBUG_PRINTLN("[SML] Keine gültigen Daten verfügbar");
        return;
    }
    
    DEBUG_PRINTLN("========== SML Daten ==========");
    for (int i = 0; i < smlData.valueCount; i++) {
        DEBUG_PRINTF("  %s = %.3f %s\n", 
                     smlData.values[i].obisCode,
                     smlData.values[i].value,
                     smlData.values[i].unit);
    }
    DEBUG_PRINTLN("===============================");
}

// ============================================================
// PRIVATE METHODEN
// ============================================================

/**
 * Hilfsfunktion: Überspringt ein TLV-Feld.
 *
 * SML TLV-Kodierung:
 *   Type-Byte: [oberes Nibble = Typ] [unteres Nibble = Gesamtlänge inkl. Type-Byte]
 *   Typen: 0x0x = Octet-String, 0x4x = Boolean,
 *          0x5x = Integer (signed), 0x6x = Unsigned Integer, 0x7x = Liste
 *   Sonderfall 0x01: optional/nicht vorhanden (1 Byte, kein Datenwert)
 *   Für Listen gilt das untere Nibble als Anzahl der Listeneinträge.
 *
 * Gibt false zurück wenn die Grenze überschritten wird.
 */
static bool smlSkipField(const uint8_t* buffer, int& pos, int endLimit, int depth = 0) {
    if (pos >= endLimit || depth > 10) return false;
    uint8_t tag = buffer[pos];
    if (tag == 0x01) { pos += 1; return true; }    // optional/absent
    if ((tag & 0xF0) == 0x70) {
        // List type: lower nibble = number of child entries
        uint8_t count = tag & 0x0F;
        pos += 1;
        for (int i = 0; i < count; i++) {
            if (!smlSkipField(buffer, pos, endLimit, depth + 1)) return false;
        }
        return true;
    }
    // Primitive type: lower nibble = total length incl. type byte
    uint8_t len = tag & 0x0F;
    if (len == 0 || pos + len > endLimit) return false;
    pos += len;
    return true;
}

/**
 * SML Parser – sucht nach SML_ListEntry (0x77) im Frame und extrahiert OBIS-Werte.
 *
 * SML-Frame-Struktur:
 *   Start-Escape:  1B 1B 1B 1B 01 01 01 01  (8 Bytes)
 *   SML-Body (TLV-kodiert)
 *   Ende-Escape:   1B 1B 1B 1B 1A xx CRC_L CRC_H  (8 Bytes)
 *
 * SML_ListEntry (0x77) enthält 7 Felder:
 *   [objName: Octet-String 0x07 = 6 Byte OBIS]
 *   [status: optional]
 *   [valTime: optional]
 *   [unit: optional]
 *   [scaler: signed int8, optional]
 *   [value: integer beliebiger Größe]
 *   [valueSignature: optional]
 */
bool SmlReader::parseSmlMessage(const uint8_t* buffer, int length) {
    bool found = false;

    // Start-Escape (8 Bytes) überspringen; Ende-Escape belegt letzte 8 Bytes
    int pos = 8;
    int endLimit = length - 8;

    while (pos < endLimit) {
        // SML_ListEntry beginnt mit 0x77 (Liste mit 7 Einträgen)
        if (buffer[pos] != 0x77) {
            pos++;
            continue;
        }

        int p = pos + 1;

        // Feld 1: objName – OBIS-Code als 6-Byte Octet-String (Tag 0x07)
        if (p >= endLimit || buffer[p] != 0x07) { pos++; continue; }
        p++;
        if (p + 6 > endLimit) { pos++; continue; }

        const uint8_t* obis = &buffer[p];
        if (obis[0] > 15) { pos++; continue; }  // Plausibilitätsprüfung A-Feld

        char obisStr[32];
        snprintf(obisStr, sizeof(obisStr), "%d-%d:%d.%d.%d*%d",
                 obis[0], obis[1], obis[2], obis[3], obis[4], obis[5]);
        p += 6;

        // Feld 2: status (überspringen)
        if (!smlSkipField(buffer, p, endLimit)) { pos++; continue; }

        // Feld 3: valTime (überspringen)
        if (!smlSkipField(buffer, p, endLimit)) { pos++; continue; }

        // Feld 4: unit (überspringen)
        if (!smlSkipField(buffer, p, endLimit)) { pos++; continue; }

        // Feld 5: scaler (signed int8, Tag 0x52; oder 0x01 = absent)
        if (p >= endLimit) { pos++; continue; }
        int8_t scaler = 0;
        uint8_t scalerTag = buffer[p];
        if ((scalerTag & 0xF0) == 0x50) {           // signed integer
            uint8_t scalerDataLen = (scalerTag & 0x0F) - 1;
            if (scalerDataLen == 1 && p + 1 < endLimit) {
                scaler = (int8_t)buffer[p + 1];
            }
        }
        if (!smlSkipField(buffer, p, endLimit)) { pos++; continue; }

        // Feld 6: value (signed 0x5x oder unsigned 0x6x integer)
        if (p >= endLimit) { pos++; continue; }
        uint8_t valTag     = buffer[p];
        uint8_t valType    = valTag & 0xF0;
        uint8_t valTotalLen = valTag & 0x0F;
        uint8_t dataLen    = (valTotalLen > 0) ? (valTotalLen - 1) : 0;

        if ((valType == 0x50 || valType == 0x60) &&
            dataLen > 0 && dataLen <= 8 &&
            p + valTotalLen <= endLimit) {

            // Integer big-endian lesen; bei signed: Vorzeichen erweitern
            int64_t intValue = 0;
            if (valType == 0x50 && (buffer[p + 1] & 0x80)) {
                intValue = -1LL;  // Vorzeichen-Extension (0xFFFFFFFFFFFFFFFF)
            }
            for (int b = 0; b < (int)dataLen; b++) {
                intValue = (intValue << 8) | buffer[p + 1 + b];
            }

            // Scaler anwenden: physikalischer Wert = intValue × 10^scaler
            float value = (float)intValue * powf(10.0f, (float)scaler);

            // Einheit aus dem OBIS C-Feld ableiten
            const char* unit = "";
            switch (obis[2]) {
                case 1:  case 2:                         unit = "kWh"; break;
                case 16: case 36: case 56: case 76:      unit = "W";   break;
                case 32: case 52: case 72:               unit = "V";   break;
                case 31: case 51: case 71:               unit = "A";   break;
                case 14:                                 unit = "Hz";  break;
                default: break;
            }

            storeObisValue(obisStr, value, unit);
            found = true;
        }

        pos++;
    }

    return found;
}

// obisToString nicht mehr benötigt

void SmlReader::storeObisValue(const char* obisCode, float value, const char* unit) {
    int idx = findOrCreateObisEntry(obisCode);
    if (idx >= 0 && idx < SML_MAX_OBIS_VALUES) {
        strncpy(smlData.values[idx].obisCode, obisCode, MAX_OBIS_CODE_LEN - 1);
        smlData.values[idx].obisCode[MAX_OBIS_CODE_LEN - 1] = '\0';
        smlData.values[idx].value = value;
        strncpy(smlData.values[idx].unit, unit, sizeof(smlData.values[idx].unit) - 1);
        smlData.values[idx].unit[sizeof(smlData.values[idx].unit) - 1] = '\0';
        smlData.values[idx].lastUpdate = millis();
    }
}

int SmlReader::findOrCreateObisEntry(const char* obisCode) {
    // Bestehenden Eintrag suchen
    for (int i = 0; i < smlData.valueCount; i++) {
        if (strcmp(smlData.values[i].obisCode, obisCode) == 0) {
            return i;
        }
    }
    
    // Neuen Eintrag erstellen
    if (smlData.valueCount < SML_MAX_OBIS_VALUES) {
        int idx = smlData.valueCount;
        memset(&smlData.values[idx], 0, sizeof(ObisValue));
        smlData.valueCount++;
        return idx;
    }
    
    return -1; // Array voll
}
