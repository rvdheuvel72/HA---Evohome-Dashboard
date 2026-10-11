#!/usr/bin/env python3
"""Apply a pinned, receive-only ESP32-WROOM-32E port to IndaloTech/ramses_esp.

This is a source overlay, not a replacement for the upstream RAMSES II decoder.
Upstream source must be at commit 12836b2907cb0f0a0b1f3be648dafc40192c2023.
"""
from pathlib import Path
import sys

UPSTREAM = "12836b2907cb0f0a0b1f3be648dafc40192c2023"

def replace_once(root, rel, old, new):
    path = root / rel
    src = path.read_text()
    if src.count(old) != 1:
        raise RuntimeError(f"{rel}: expected one matching source fragment; got {src.count(old)}")
    path.write_text(src.replace(old, new, 1))

def port(root):
    if not (root / "CMakeLists.txt").is_file() or not (root / "components/cc1101/cc1101.c").is_file():
        raise RuntimeError("Pass the root of a checked-out IndaloTech/ramses_esp source tree")

    # Avoid S3 native USB serial, which the classic ESP32 does not have.
    cmd="components/command/cmd.c"
    replace_once(root, cmd,
        '#include "driver/usb_serial_jtag.h"',
        '#include "sdkconfig.h"\n#if CONFIG_IDF_TARGET_ESP32\n#include "driver/uart.h"\n#else\n#include "driver/usb_serial_jtag.h"\n#endif')
    replace_once(root, cmd,
        'static void console_init( char *line, int len ) {\n',
        'static void console_init( char *line, int len ) {\n'
        '#if CONFIG_IDF_TARGET_ESP32\n'
        '  (void)line; (void)len;\n'
        '  esp_err_t err = uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);\n'
        '  if (err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);\n'
        '  uart_flush_input(UART_NUM_0);\n'
        '#else\n')
    replace_once(root, cmd,
        '  while( usb_serial_jtag_read_bytes( line, len, 0 ) ){}\n}',
        '  while( usb_serial_jtag_read_bytes( line, len, 0 ) ){}\n#endif\n}')
    replace_once(root, cmd,
        '      ret = usb_serial_jtag_read_bytes( line+n, 1, 0 );',
        '      #if CONFIG_IDF_TARGET_ESP32\n'
        '      ret = uart_read_bytes(UART_NUM_0, line+n, 1, 0);\n'
        '      #else\n'
        '      ret = usb_serial_jtag_read_bytes( line+n, 1, 0 );\n'
        '      #endif')
    replace_once(root, cmd, '            n++;\n', '            if (n < len-1) n++;\n')

    # Classic ESP32 needs a different GPIO range.
    replace_once(root, "components/cc1101/Kconfig.projbuild",
        'default 48 if IDF_TARGET_ESP32S3',
        'default 48 if IDF_TARGET_ESP32S3\n\t\tdefault 39 if IDF_TARGET_ESP32')

    # TX from MQTT/serial must not activate the heating system during bring-up.
    replace_once(root, "components/gateway/gateway.c",
        'static void tx_msg( char const *cmd ){\n',
        'static void tx_msg( char const *cmd ){\n'
        '#if CONFIG_IDF_TARGET_ESP32\n'
        '  (void)cmd;\n'
        '  ESP_LOGW(TAG, "WROOM prototype is receive-only: RF transmit blocked");\n'
        '#else\n')
    replace_once(root, "components/gateway/gateway.c",
        '}\n\nvoid gateway_tx( char const *msg ) {',
        '#endif\n}\n\nvoid gateway_tx( char const *msg ) {')
    replace_once(root, "components/ramses-mqtt/ramses-mqtt.c",
        'static void mqtt_process_tx( struct mqtt_data *ctxt, char const *data, int dataLen ) {\n',
        'static void mqtt_process_tx( struct mqtt_data *ctxt, char const *data, int dataLen ) {\n'
        '#if CONFIG_IDF_TARGET_ESP32\n'
        '  (void)ctxt; (void)data; (void)dataLen;\n'
        '  ESP_LOGW(TAG, "WROOM prototype: MQTT TX ignored (receive-only)");\n'
        '  return;\n'
        '#else\n')
    replace_once(root, "components/ramses-mqtt/ramses-mqtt.c",
        '  cJSON_Delete( json );\n}\n\n/*******************************************************************************\n * General command',
        '  cJSON_Delete( json );\n#endif\n}\n\n/*******************************************************************************\n * General command')

    # Small read-only status API. Packet transport remains upstream MQTT.
    replace_once(root, "main/CMakeLists.txt",
        'SRCS main.c radio.c host.c platform.c',
        'SRCS main.c radio.c host.c platform.c wroom_http.c')
    replace_once(root, "main/CMakeLists.txt",
        'PRIV_REQUIRES command',
        'PRIV_REQUIRES esp_http_server esp_wifi esp_timer command')
    replace_once(root, "main/main.c",
        '#include "host.h"',
        '#include "host.h"\n#include "wroom_http.h"')
    replace_once(root, "main/main.c",
        '  ramses_network_init( CONFIG_HOST_CORE );',
        '  ramses_network_init( CONFIG_HOST_CORE );\n  wroom_http_init();')

    (root / "main/wroom_http.h").write_text(
        "#pragma once\nvoid wroom_http_init(void);\n")
    (root / "main/wroom_http.c").write_text(r'''#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "wroom_http.h"

static esp_err_t status_get(httpd_req_t *req) {
    wifi_ap_record_t ap = {0};
    bool wifi_connected = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);
    const esp_app_desc_t *app = esp_app_get_description();
    char json[320];
    snprintf(json, sizeof(json),
             "{\"board\":\"ESP32-WROOM-32E\",\"mode\":\"receive_only\","
             "\"rf_verified\":false,\"wifi_connected\":%s,"
             "\"uptime_ms\":%llu,\"free_heap\":%lu,\"firmware\":\"%.32s\"}",
             wifi_connected ? "true" : "false",
             (unsigned long long)(esp_timer_get_time() / 1000),
             (unsigned long)esp_get_free_heap_size(), app->version);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

void wroom_http_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE("WROOM_HTTP", "HTTP server start failed: %s", esp_err_to_name(err));
        return;
    }
    const httpd_uri_t endpoint = {
        .uri = "/api/v1/status", .method = HTTP_GET,
        .handler = status_get, .user_ctx = NULL
    };
    err = httpd_register_uri_handler(server, &endpoint);
    if (err != ESP_OK)
        ESP_LOGE("WROOM_HTTP", "URI registration failed: %s", esp_err_to_name(err));
}
''')

    # 4 MB dual OTA partition scheme, with 1.875 MB per application slot.
    (root / "partitions.csv").write_text(
        "# Name, Type, SubType, Offset, Size\n"
        "nvs, data, nvs, 0x9000, 0x4000\n"
        "otadata, data, ota, 0xd000, 0x2000\n"
        "phy_init, data, phy, 0xf000, 0x1000\n"
        "ota_0, app, ota_0, 0x10000, 0x1e0000\n"
        "ota_1, app, ota_1, 0x1f0000, 0x1e0000\n")

    (root / "sdkconfig.defaults").write_text("""\
CONFIG_IDF_TARGET="esp32"
CONFIG_IDF_TARGET_ESP32=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200
CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
CONFIG_LWIP_LOCAL_HOSTNAME="ramses-wroom32"
CONFIG_WIFI_AUTH_WPA2_PSK=y
CONFIG_RADIO_CORE=1
CONFIG_HOST_CORE=0
CONFIG_FUNCTION_PIN=0
CONFIG_CS_PLATFORM_PIN=25
CONFIG_CC_GPIO_RANGE_MAX=39
CONFIG_CC_MOSI_GPIO=23
CONFIG_CC_SCK_GPIO=18
CONFIG_CC_MISO_GPIO=19
CONFIG_CC_CSN_GPIO=5
CONFIG_CC_GDO0_GPIO=26
CONFIG_CC_GDO2_GPIO=27
CONFIG_CC_SPI2_HOST=y
CONFIG_GPIO_RX_LED=32
CONFIG_GPIO_TX_LED=33
CONFIG_DEBUG_PIN1=0
CONFIG_DEBUG_PIN2=0
CONFIG_DEBUG_PIN3=0
CONFIG_DEBUG_PIN4=0
CONFIG_DEBUG_PIN5=0
CONFIG_DEBUG_PIN6=0
CONFIG_OTA_FILENAME="ramses_esp_wroom32e.bin"
CONFIG_OTA_URL_SERVER="https://example.invalid/no-automatic-updates"
CONFIG_WIFI_MAXIMUM_RETRY=5
CONFIG_MQTT_ROOT="RAMSES/GATEWAY"
CONFIG_N_MSG=16
CONFIG_LOG_COLORS=n
""")
    print("Patched ESP32-WROOM-32E source (receive-only); ready for ESP-IDF compilation.")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: python apply_port.py /path/to/IndaloTech/ramses_esp")
    port(Path(sys.argv[1]).resolve())
