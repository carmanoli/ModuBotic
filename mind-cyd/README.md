# ModuBotic mind-cyd

Mente alternativa para um ESP32 CYD (Cheap Yellow Display), mantendo o mesmo
protocolo usado pela mente K10.

## Ideia

O Arduino continua a receber comandos por UART, sempre em linhas terminadas por
`\n`:

```text
HEAD_LEFT
MOVE_FORWARD
STOP
ENV_READ
```

E continua a enviar telemetria tambem por UART:

```text
BODY_READY
RECEIVED HEAD_LEFT
env=20.4,55.2,20.8,1013.4,76
i2c=27:LCD_I2C,38:AHT20_AHT21,5A:GY33_CONTROLLER
```

O CYD fica no meio:

```text
Painel PC <---- WiFi HTTP ----> CYD/ESP32 <---- UART ----> Arduino body
```

Assim podes ter varias minds diferentes, desde que todas respeitem este contrato.
O Arduino nao precisa de saber se esta ligado ao K10, CYD, UNIHIKER, ou outro
controlador.

## Wiring UART

Exemplo para ESP32:

```text
Arduino UNO R4 D1 TX  -> ESP32 RX2 (ex: GPIO16)
Arduino UNO R4 D0 RX  <- ESP32 TX2 (ex: GPIO17)
Arduino GND           -> ESP32 GND
```

Importante: confirma os niveis logicos. O ESP32 e 3.3 V; se o TX do Arduino sair
a 5 V, usa divisor de tensao ou conversor de nivel para proteger o RX do ESP32.

## Configurar

1. Copia `config.example.h` para `config.h`.
2. Ajusta WiFi, IP do PC e pinos UART.
3. Abre `mind-cyd.ino` no Arduino IDE.
4. Seleciona uma board ESP32 compativel com o teu CYD.
5. Faz upload.

Se tiveres a biblioteca `TFT_eSPI` instalada e configurada para o teu CYD, o
sketch mostra estado basico no ecra. Sem essa biblioteca, continua a funcionar
pela serial USB.

## Endpoints usados

O CYD usa os endpoints que ja existem no `control/server.py`:

```text
POST /api/command
```

Devolve o proximo comando como texto puro, ou `NONE`.

```text
POST /api/telemetry?<linha_uart>
```

Atualiza o painel sem consumir comandos.

## Protocolo comum das minds

Qualquer mind deve fazer estas tres coisas:

1. Pedir comandos ao painel.
2. Enviar comandos validos ao Arduino pela UART.
3. Reencaminhar linhas de telemetria do Arduino para o painel.

Linhas de debug como `BODY_READY`, `RECEIVED ...`, `UNKNOWN ...` podem ser
mostradas localmente, mas normalmente nao precisam de ser enviadas como
telemetria.
