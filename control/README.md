# ModuBotic Control

Painel local para o PC receber dados do UNIHIKER K10 pela rede.

## Arrancar

```powershell
python server.py
```

Abre no PC:

```text
http://localhost:8080
```

No K10 usa o IP do PC na rede, por exemplo:

```text
http://192.168.1.20:8080
```

## Enviar telemetria do K10

Endpoint:

```text
POST /api/telemetry
```

JSON:

```json
{
  "device": "UNIHIKER K10",
  "temperature": 24.5,
  "humidity": 56,
  "light": 120,
  "ip": "192.168.1.50",
  "message": "ok"
}
```

Tambem aceita `application/x-www-form-urlencoded`, se os blocos HTTP do Mind+ forem mais simples:

```text
temperature=24.5&humidity=56&ip=192.168.1.50&message=ok
```

Com os blocos HTTP do Mind+, tambem podes enviar valores pela query string:

```text
POST http://10.0.0.100:8080/api/telemetry?temperature=24.5&light=120
```

Para montar nos blocos:

```text
http://10.0.0.100:8080/api/telemetry?temperature=<temperatura>&light=<luz>
```

Evita fazer um POST separado para cada sensor. Um unico POST com todos os valores
atualiza a pagina de uma vez.

Este endpoint devolve sempre `OK` e nunca retira comandos da fila. Assim podes
enviar temperatura/luz do K10 mesmo quando o Arduino nao tem nenhum sensor ativo.

## Reencaminhar sensores I2C do Arduino

O Arduino envia pela Serial1 uma linha pronta para URL quando o modulo
AHT20/BMP280 tem leitura valida:

```text
env=20.4,55.2,20.8,1013.4,76
```

No K10, le essa linha da porta serial e envia-a para o painel com o endpoint
que nao consome comandos:

```text
POST http://10.0.0.100:8080/api/telemetry?env=20.4,55.2,20.8,1013.4,76
```

Nos blocos, podes montar:

```text
url = "http://10.0.0.100:8080/api/telemetry?" + serial
```

Quando o servidor recebe AHT20/BMP280, a temperatura/humidade principais passam
a ser do AHT20. A temperatura interna do K10 continua a ser guardada
separadamente e deixa de sobrescrever o grafico enquanto o sensor I2C estiver
fresco.

O watchdog I2C tambem envia uma lista generica quando o barramento muda:

```text
i2c=27:LCD_I2C,38:AHT20_AHT21,5A:GY33_CONTROLLER
```

No K10, reencaminha esta linha pelo mesmo endpoint `/api/telemetry`. O
painel mostra qualquer endereco detetado; para enderecos desconhecidos usa
`UNKNOWN` em vez de assumir que e um dos sensores ja suportados.

## Enviar imagem

O painel aceita imagem em base64/JPEG no campo `image`:

```json
{
  "image": "/9j/4AAQSkZJRgABAQ..."
}
```

Mas o bloco `capture photo to TF card` do Mind+ apenas guarda o ficheiro no K10.
Ele nao envia automaticamente a imagem para o PC. Para isso e preciso um bloco que
consiga enviar corpo HTTP/base64 ou entao passar para codigo manual.

## Ler comandos no K10

Endpoint:

```text
GET /api/commands/next
```

Resposta quando existe comando:

```json
{"command":"LED_ON"}
```

Resposta quando nao existe:

```json
{"command":null}
```

Para Mind+ com o bloco `HTTP read ONE line message`, usa este endpoint mais simples:

```text
POST http://10.0.0.100:8080/api/command
```

Ele devolve texto puro:

```text
HEAD_LEFT
```

Quando nao ha comando pendente:

```text
NONE
```

Tambem funcionam `GET /api/motion` e `POST /api/motion`, para manter compatibilidade
com os blocos que ja estavam feitos.

## Fluxo recomendado no Mind+

No ciclo `forever`, separa comandos e telemetria:

```text
1. POST http://10.0.0.100:8080/api/command
   Ler uma linha da resposta.
   Se for diferente de NONE, OK ou ERROR, escrever essa linha na serial1.

2. POST http://10.0.0.100:8080/api/telemetry?temperature=<temp_k10>&light=<luz_k10>
   Isto fica fora do bloco que le a serial, para continuar a atualizar o K10
   mesmo quando nao existe sensor I2C ligado ao Arduino.

3. Se a serial1 tiver texto vindo do Arduino, juntar os bytes numa string.
   Se a string tiver "env=", "rgb=" ou "i2c=", enviar:
   POST http://10.0.0.100:8080/api/telemetry?<serial>
```

Assim o segundo POST de telemetria deixa de consumir comandos por engano.

Comandos criados pelos botoes da pagina:

```text
HEAD_LEFT
HEAD_CENTER
HEAD_RIGHT
ARM_LEFT_UP
ARM_LEFT_DOWN
ARM_RIGHT_UP
ARM_RIGHT_DOWN
MOVE_FORWARD
MOVE_BACK
TURN_LEFT
TURN_RIGHT
STOP
GY33_SCAN
GY33_READ
GY33_ON
GY33_OFF
ENV_SCAN
ENV_READ
ENV_ON
ENV_OFF
```
