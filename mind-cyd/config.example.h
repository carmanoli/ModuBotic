#pragma once

// WiFi da rede onde tambem esta o PC a correr control/server.py.
#define MODUBOTIC_WIFI_SSID "A_TUA_REDE"
#define MODUBOTIC_WIFI_PASSWORD "A_TUA_PASSWORD"

// Usa o IP real do PC na rede, nao localhost.
#define MODUBOTIC_CONTROL_BASE_URL "http://10.0.0.100:8080"

// Identidade desta mind/robot na rede.
#define MODUBOTIC_ROBOT_ID "pink"
#define MODUBOTIC_MIND_TYPE "cyd"

// MQTT broker Mosquitto. Instala a biblioteca PubSubClient no Arduino IDE.
#define MODUBOTIC_MQTT_ENABLED 1
#define MODUBOTIC_MQTT_HOST "10.0.0.90"
#define MODUBOTIC_MQTT_PORT 1883
#define MODUBOTIC_MQTT_BASE_TOPIC "modubotic/robot"
#define MODUBOTIC_MQTT_RETRY_MS 5000

// UART entre CYD/ESP32 e Arduino body.
// Ajusta conforme os pinos livres no teu CYD.
// No CYD, GPIO16/GPIO17 sao LEDs onboard. Evita usa-los para UART.
// Sugestao: Arduino TX -> CYD GPIO35 (RX, input-only)
//           Arduino RX <- CYD GPIO22 (TX)
#define MODUBOTIC_BODY_RX_PIN 35
#define MODUBOTIC_BODY_TX_PIN 22
#define MODUBOTIC_BODY_BAUD 9600
#define MODUBOTIC_BODY_USE_USB_UART 0
#define MODUBOTIC_ENABLE_USB_SERIAL_LOG 1

// Intervalos.
#define MODUBOTIC_COMMAND_POLL_MS 250
#define MODUBOTIC_TELEMETRY_FLUSH_MS 100
#define MODUBOTIC_BODY_PING_MS 5000
#define MODUBOTIC_BODY_ONLINE_MS 15000

// Mostra no Serial Monitor todos os bytes recebidos do body.
// Mantem ligado enquanto validas a ligacao UART.
#define MODUBOTIC_BODY_DEBUG_RAW 1
#define MODUBOTIC_FORWARD_BODY_TELEMETRY 1

// Se quiseres usar o conector fisico 5V/TX/RX/GND do CYD, normalmente e UART0:
//   #define MODUBOTIC_BODY_RX_PIN 3
//   #define MODUBOTIC_BODY_TX_PIN 1
//   #define MODUBOTIC_BODY_USE_USB_UART 1
//   #define MODUBOTIC_ENABLE_USB_SERIAL_LOG 0
//   #define MODUBOTIC_BODY_DEBUG_RAW 0
// Nesse modo nao uses o Serial Monitor para debug, porque ele partilha os pinos.

// O sketch ja traz uma configuracao TFT_eSPI para o CYD normal
// ESP32-2432S028/ILI9341. Se quiseres usar a configuracao global da biblioteca,
// descomenta a linha abaixo.
// #define MODUBOTIC_DISABLE_CYD_TFT_SETUP

// Descomenta para desligar o touch enquanto testas apenas o display.
// #define MODUBOTIC_DISABLE_TOUCH
