#include "messages.h"
#include "switches.h"

bool msgHeartbeat(char* out, size_t outsz) {
    int n = snprintf(out, outsz,
        "{\"device_id\":\"%s\",\"node_id\":\"N/A\",\"type\":\"hb\","
        "\"connection\":\"%s\","
        "\"light1\":%d,\"light2\":%d,\"fan\":%d,\"fan_speed\":%d,"
        "\"rssi\":%d,\"fw\":\"%s\"}",
        DEVICE_ID,
        (WiFi.status() == WL_CONNECTED) ? "wifi" : "none",
        getSwitchState(0) ? 1 : 0,
        getSwitchState(1) ? 1 : 0,
        getSwitchState(2) ? 1 : 0,
        getFanSpeed(),
        WiFi.RSSI(),
        FIRMWARE_VERSION);
    return n > 0 && (size_t)n < outsz;
}

bool msgAck(char* out, size_t outsz, const char* source) {
    int n = snprintf(out, outsz,
        "{\"device_id\":\"%s\",\"node_id\":\"N/A\",\"type\":\"ack\","
        "\"source\":\"%s\","
        "\"light1\":%d,\"light2\":%d,\"fan\":%d,\"fan_speed\":%d}",
        DEVICE_ID, source,
        getSwitchState(0) ? 1 : 0,
        getSwitchState(1) ? 1 : 0,
        getSwitchState(2) ? 1 : 0,
        getFanSpeed());
    return n > 0 && (size_t)n < outsz;
}

bool msgPing(char* out, size_t outsz) {
    int n = snprintf(out, outsz,
        "{\"device_id\":\"%s\",\"node_id\":\"N/A\",\"type\":\"ping_response\","
        "\"ssid\":\"%s\",\"rssi\":%d,\"ip\":\"%s\","
        "\"fw_ver\":\"%s\",\"hw_ver\":\"%s\",\"fw_date\":\"%s\","
        "\"hb_interval\":%lu}",
        DEVICE_ID,
        WiFi.SSID().c_str(),
        WiFi.RSSI(),
        WiFi.localIP().toString().c_str(),
        FIRMWARE_VERSION, HARDWARE_VERSION, FIRMWARE_RELEASE_DATE,
        (unsigned long)HB_INTERVAL_MS);
    return n > 0 && (size_t)n < outsz;
}

bool msgOta(char* out, size_t outsz, const char* status) {
    int n = snprintf(out, outsz,
        "{\"device_id\":\"%s\",\"node_id\":\"N/A\",\"type\":\"ota\","
        "\"status\":\"%s\"}",
        DEVICE_ID, status);
    return n > 0 && (size_t)n < outsz;
}

bool msgRf(char* out, size_t outsz, unsigned long code, uint8_t bits) {
    int n = snprintf(out, outsz,
        "{\"device_id\":\"%s\",\"node_id\":\"N/A\",\"type\":\"rf\","
        "\"code\":%lu,\"bits\":%u}",
        DEVICE_ID, code, (unsigned)bits);
    return n > 0 && (size_t)n < outsz;
}