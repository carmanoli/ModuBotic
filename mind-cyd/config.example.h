#pragma once

// WiFi da rede onde tambem esta o PC a correr control/server.py.
#define MODUBOTIC_WIFI_SSID "A_TUA_REDE"
#define MODUBOTIC_WIFI_PASSWORD "A_TUA_PASSWORD"

// Usa o IP real do PC na rede, nao localhost.
#define MODUBOTIC_CONTROL_BASE_URL "http://192.168.1.20:8080"

// UART entre CYD/ESP32 e Arduino body.
// Ajusta conforme os pinos livres no teu CYD.
#define MODUBOTIC_BODY_RX_PIN 16
#define MODUBOTIC_BODY_TX_PIN 17
#define MODUBOTIC_BODY_BAUD 9600

// Intervalos.
#define MODUBOTIC_COMMAND_POLL_MS 250
#define MODUBOTIC_TELEMETRY_FLUSH_MS 100

