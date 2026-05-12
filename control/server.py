from __future__ import annotations

import base64
import json
import mimetypes
import re
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


ROOT = Path(__file__).resolve().parent
PUBLIC = ROOT / "public"
DATA = ROOT / "data"
STATE_FILE = DATA / "state.json"

HOST = "0.0.0.0"
PORT = 8080
DHT_FRESH_MS = 15000
ENV_FRESH_MS = 15000
COMMAND_TTL_MS = 60000
COMMAND_QUEUE_LIMIT = 10
DHT_READING_RE = re.compile(r"T\s*=\s*(-?\d+(?:\.\d+)?)\s*C\s+H\s*=\s*(-?\d+(?:\.\d+)?)")
DHT_QUERY_RE = re.compile(r"temperature\s*=\s*(-?\d+(?:\.\d+)?).*?humidity\s*=\s*(-?\d+(?:\.\d+)?)", re.IGNORECASE | re.DOTALL)
DHT_TEMP_KEY_RE = re.compile(r"(?:^|[&\s])temperature\s*=\s*(-?\d+(?:\.\d+)?)", re.IGNORECASE)
DHT_HUMIDITY_KEY_RE = re.compile(r"(?:^|[&\s])humidity\s*=\s*(-?\d+(?:\.\d+)?)", re.IGNORECASE)
DHT_PIN_RE = re.compile(r"(?:^|[&\s])pin\s*=\s*(\d+)", re.IGNORECASE)
DHT_VALUE_RE = re.compile(r"^\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)(?:\s*,\s*(\d+))?\s*$")
ENV_VALUE_RE = re.compile(r"^\s*(-?\d+(?:\.\d+)?)?\s*,\s*(-?\d+(?:\.\d+)?)?\s*,\s*(-?\d+(?:\.\d+)?)?\s*,\s*(-?\d+(?:\.\d+)?)?\s*,?\s*([0-9a-fA-F]+)?\s*$")
GY33_RGB8_RE = re.compile(r"RGB8\s*=\s*(\d+(?:\.\d+)?)\s*,\s*(\d+(?:\.\d+)?)\s*,\s*(\d+(?:\.\d+)?)", re.IGNORECASE)
GY33_RGB_VALUE_RE = re.compile(r"^\s*(\d+(?:\.\d+)?)\s*,\s*(\d+(?:\.\d+)?)\s*,\s*(\d+(?:\.\d+)?)\s*$")


def now_ms() -> int:
    return int(time.time() * 1000)


def default_state() -> dict:
    return {
        "device": "UNIHIKER K10",
        "temperature": None,
        "humidity": None,
        "k10Temperature": None,
        "dhtTemperature": None,
        "dhtHumidity": None,
        "dhtPin": None,
        "dhtUpdatedAt": None,
        "envTemperature": None,
        "envHumidity": None,
        "envPressure": None,
        "envBmpTemperature": None,
        "envBmpAddress": None,
        "envUpdatedAt": None,
        "temperatureSource": None,
        "light": None,
        "gas": None,
        "color": None,
        "colorUpdatedAt": None,
        "ip": None,
        "message": "A espera de dados do K10",
        "camera": None,
        "updatedAt": None,
        "history": [],
        "commands": [],
        "pendingCommand": None,
        "lastCommand": None,
        "lastMotionRequest": None,
        "motionRequestCount": 0,
        "lastPollRequest": None,
        "pollRequestCount": 0,
        "lastTelemetryRequest": None,
        "telemetryRequestCount": 0,
    }


def load_state() -> dict:
    DATA.mkdir(exist_ok=True)
    if not STATE_FILE.exists():
        return default_state()

    try:
        with STATE_FILE.open("r", encoding="utf-8") as file:
            state = json.load(file)
    except (OSError, json.JSONDecodeError):
        return default_state()

    base = default_state()
    base.update(state)
    return base


STATE = load_state()


def save_state() -> None:
    DATA.mkdir(exist_ok=True)
    with STATE_FILE.open("w", encoding="utf-8") as file:
        json.dump(STATE, file, ensure_ascii=True, indent=2)


def push_history(entry: dict) -> None:
    STATE["history"].insert(0, entry)
    del STATE["history"][200:]


def parse_number(value):
    if value in (None, ""):
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return value


def is_fresh_dht(updated_at: int) -> bool:
    dht_updated_at = STATE.get("dhtUpdatedAt")
    return bool(dht_updated_at and updated_at - int(dht_updated_at) < DHT_FRESH_MS)


def is_fresh_environment(updated_at: int) -> bool:
    env_updated_at = STATE.get("envUpdatedAt")
    return bool(env_updated_at and updated_at - int(env_updated_at) < ENV_FRESH_MS)


def has_fresh_external_temperature(updated_at: int) -> bool:
    return is_fresh_dht(updated_at) or is_fresh_environment(updated_at)


def normalize_telemetry_payload(payload: dict) -> dict:
    normalized = dict(payload)

    for key in ("body", "serial", "line", "message", "msg"):
        value = normalized.get(key)
        if not isinstance(value, str):
            continue

        text = value.strip()
        if not text:
            continue

        if text.startswith("DHT_TELEMETRY "):
            text = text.removeprefix("DHT_TELEMETRY ").strip()

        if text.startswith("COLOR "):
            text = text.removeprefix("COLOR ").strip()

        query_text = text.replace("\r", "&").replace("\n", "&").strip("&")

        if "=" in query_text:
            parsed = {field.strip(): values[-1] for field, values in parse_qs(query_text).items()}
            normalized.update(parsed)

        rgb8_match = GY33_RGB8_RE.search(text)
        if rgb8_match:
            normalized["r"] = rgb8_match.group(1)
            normalized["g"] = rgb8_match.group(2)
            normalized["b"] = rgb8_match.group(3)
            normalized.setdefault("source", "gy33")

        temp_key_match = DHT_TEMP_KEY_RE.search(text)
        humidity_key_match = DHT_HUMIDITY_KEY_RE.search(text)
        if temp_key_match:
            normalized["temperature"] = temp_key_match.group(1)
        if humidity_key_match:
            normalized["humidity"] = humidity_key_match.group(1)
            normalized.setdefault("source", "dht22")

        query_match = DHT_QUERY_RE.search(text)
        if query_match:
            normalized["temperature"] = query_match.group(1)
            normalized["humidity"] = query_match.group(2)
            normalized["source"] = "dht22"
            pin_match = DHT_PIN_RE.search(text)
            if pin_match:
                normalized["pin"] = pin_match.group(1)

        match = DHT_READING_RE.search(text)
        if match:
            normalized["temperature"] = match.group(1)
            normalized["humidity"] = match.group(2)
            normalized["source"] = "dht22"

    for key in ("rgb", "color"):
        value = normalized.get(key)
        if not isinstance(value, str):
            continue
        rgb_match = GY33_RGB_VALUE_RE.match(value)
        if not rgb_match:
            continue
        normalized["r"] = rgb_match.group(1)
        normalized["g"] = rgb_match.group(2)
        normalized["b"] = rgb_match.group(3)
        normalized.setdefault("source", "gy33")

    value = normalized.get("dht")
    if isinstance(value, str):
        dht_match = DHT_VALUE_RE.match(value)
        if dht_match:
            normalized["temperature"] = dht_match.group(1)
            normalized["humidity"] = dht_match.group(2)
            if dht_match.group(3):
                normalized["pin"] = dht_match.group(3)
            normalized["source"] = "dht22"

    value = normalized.get("env")
    if isinstance(value, str):
        env_match = ENV_VALUE_RE.match(value)
        if env_match:
            if env_match.group(1):
                normalized["ahtTemperature"] = env_match.group(1)
            if env_match.group(2):
                normalized["ahtHumidity"] = env_match.group(2)
            if env_match.group(3):
                normalized["bmpTemperature"] = env_match.group(3)
            if env_match.group(4):
                normalized["pressure"] = env_match.group(4)
            if env_match.group(5):
                normalized["bmpAddress"] = env_match.group(5)
            normalized["source"] = "aht20_bmp280"

    return normalized


def is_dht_telemetry_payload(payload: dict) -> bool:
    source = str(payload.get("source") or payload.get("sensor") or "").lower()
    return source in {"dht", "dht22", "am2302"} or "humidity" in payload or "hum" in payload


def is_gy33_telemetry_payload(payload: dict) -> bool:
    source = str(payload.get("source") or payload.get("sensor") or "").lower()
    return source in {"gy33", "gy-33", "color"} or all(key in payload for key in ("r", "g", "b"))


def is_environment_telemetry_payload(payload: dict) -> bool:
    source = str(payload.get("source") or payload.get("sensor") or "").lower()
    return source in {"env", "environment", "aht20", "bmp280", "aht20_bmp280", "aht20-bmp280"} or any(
        key in payload
        for key in ("env", "ahtTemperature", "ahtHumidity", "bmpTemperature", "pressure", "bmpAddress")
    )


class ControlHandler(BaseHTTPRequestHandler):
    server_version = "ModuBoticControl/0.1"

    def do_GET(self) -> None:
        parsed = urlparse(self.path)

        if parsed.path == "/api/status":
            normalize_command_queue()
            self.send_json(STATE)
            return

        if parsed.path == "/api/commands/next":
            command = pop_command()
            save_state()
            self.send_json({"command": command})
            return

        if parsed.path == "/api/motion/peek":
            self.send_json(command_debug())
            return

        if parsed.path == "/motion.peek":
            self.send_text(peek_command() or "NONE")
            return

        if parsed.path in {"/api/motion", "/api/motion.txt", "/motion", "/api/command", "/api/command.txt", "/api/commands/text", "/api/commands/next-text"}:
            self.serve_motion("GET")
            return

        if parsed.path == "/":
            self.send_static(PUBLIC / "index.html")
            return

        requested = (PUBLIC / parsed.path.lstrip("/")).resolve()
        if PUBLIC.resolve() not in requested.parents and requested != PUBLIC.resolve():
            self.send_error(403)
            return

        self.send_static(requested)

    def do_POST(self) -> None:
        parsed = urlparse(self.path)

        if parsed.path in {"/api/motion", "/api/motion.txt", "/motion", "/api/command", "/api/command.txt", "/api/commands/text", "/api/commands/next-text"}:
            self.serve_motion("POST")
            return

        if parsed.path in {"/api/poll", "/poll"}:
            payload = self.read_payload()
            self.update_telemetry(payload)
            command = pop_command()
            response = command or "NONE"
            STATE["pollRequestCount"] = int(STATE.get("pollRequestCount") or 0) + 1
            STATE["lastPollRequest"] = {
                "at": now_ms(),
                "client": self.client_address[0],
                "payload": payload,
                "response": response,
            }
            save_state()
            self.send_text(response)
            return

        if parsed.path == "/api/commands/clear":
            cleared = 1 if peek_command() else 0
            STATE["commands"].clear()
            STATE["pendingCommand"] = None
            save_state()
            self.send_json({"ok": True, "cleared": cleared})
            return

        if parsed.path == "/api/telemetry/clear":
            clear_telemetry()
            save_state()
            self.send_json({"ok": True})
            return

        if parsed.path == "/api/telemetry":
            payload = normalize_telemetry_payload(self.read_payload())
            self.update_telemetry(payload)
            STATE["telemetryRequestCount"] = int(STATE.get("telemetryRequestCount") or 0) + 1
            command = pop_command()
            response = command or "NONE"
            STATE["lastTelemetryRequest"] = {
                "at": now_ms(),
                "client": self.client_address[0],
                "payload": payload,
                "response": response,
            }
            save_state()
            self.send_text(response)
            return

        if parsed.path in {"/api/telemetry-only", "/api/telemetry_only"}:
            payload = normalize_telemetry_payload(self.read_payload())
            if any(key in payload for key in ("humidity", "hum")):
                payload.setdefault("source", "dht22")
            self.update_telemetry(payload)
            STATE["telemetryRequestCount"] = int(STATE.get("telemetryRequestCount") or 0) + 1
            STATE["lastTelemetryRequest"] = {
                "at": now_ms(),
                "client": self.client_address[0],
                "payload": payload,
                "response": "OK",
            }
            save_state()
            self.send_text("OK")
            return

        if parsed.path == "/api/camera":
            payload = self.read_payload()
            image = payload.get("image") or payload.get("camera")
            if image:
                STATE["camera"] = normalize_image(str(image))
                STATE["updatedAt"] = now_ms()
                push_history({"type": "camera", "at": STATE["updatedAt"], "message": "Camera updated"})
                save_state()
                self.send_json({"ok": True})
                return
            self.send_json({"ok": False, "error": "Missing image field"}, status=400)
            return

        if parsed.path == "/api/commands":
            payload = self.read_payload()
            command = str(payload.get("command", "")).strip()
            if not command:
                self.send_json({"ok": False, "error": "Missing command"}, status=400)
                return
            queue_command(command)
            save_state()
            self.send_json({"ok": True, "queued": command})
            return

        self.send_error(404)

    def update_telemetry(self, payload: dict) -> None:
        updated_at = now_ms()
        payload = normalize_telemetry_payload(payload)
        changed = {}

        source = str(payload.get("source") or payload.get("sensor") or "").lower()
        has_humidity = "humidity" in payload or "hum" in payload
        has_dht_payload = source in {"dht", "dht22", "am2302"} or has_humidity

        temperature_key = "temperature" if "temperature" in payload else "temp"
        humidity_key = "humidity" if "humidity" in payload else "hum"

        if has_dht_payload and (temperature_key in payload or humidity_key in payload):
            if temperature_key in payload:
                temperature = parse_number(payload.get(temperature_key))
                STATE["temperature"] = temperature
                STATE["dhtTemperature"] = temperature
                STATE["temperatureSource"] = "DHT22"
                changed["temperature"] = temperature
                changed["dhtTemperature"] = temperature

            if humidity_key in payload:
                humidity = parse_number(payload.get(humidity_key))
                STATE["humidity"] = humidity
                STATE["dhtHumidity"] = humidity
                changed["humidity"] = humidity
                changed["dhtHumidity"] = humidity

            if "pin" in payload:
                STATE["dhtPin"] = parse_number(payload.get("pin"))
                changed["dhtPin"] = STATE["dhtPin"]

            STATE["dhtUpdatedAt"] = updated_at
        elif temperature_key in payload:
            temperature = parse_number(payload.get(temperature_key))
            STATE["k10Temperature"] = temperature
            changed["k10Temperature"] = temperature

            if not has_fresh_external_temperature(updated_at):
                STATE["temperature"] = temperature
                STATE["temperatureSource"] = "K10"
                changed["temperature"] = temperature

        if is_environment_telemetry_payload(payload):
            env_temperature_key = "ahtTemperature" if "ahtTemperature" in payload else "temperature"
            env_humidity_key = "ahtHumidity" if "ahtHumidity" in payload else "humidity"

            if env_temperature_key in payload:
                temperature = parse_number(payload.get(env_temperature_key))
                STATE["temperature"] = temperature
                STATE["envTemperature"] = temperature
                STATE["temperatureSource"] = "AHT20"
                changed["temperature"] = temperature
                changed["envTemperature"] = temperature

            if env_humidity_key in payload:
                humidity = parse_number(payload.get(env_humidity_key))
                STATE["humidity"] = humidity
                STATE["envHumidity"] = humidity
                changed["humidity"] = humidity
                changed["envHumidity"] = humidity

            if "bmpTemperature" in payload:
                value = parse_number(payload.get("bmpTemperature"))
                STATE["envBmpTemperature"] = value
                changed["envBmpTemperature"] = value

            if "pressure" in payload:
                pressure = parse_number(payload.get("pressure"))
                STATE["envPressure"] = pressure
                changed["envPressure"] = pressure

            if "bmpAddress" in payload:
                STATE["envBmpAddress"] = str(payload.get("bmpAddress")).upper()
                changed["envBmpAddress"] = STATE["envBmpAddress"]

            STATE["envUpdatedAt"] = updated_at

        if "device" in payload:
            STATE["device"] = payload["device"]
            changed["device"] = payload["device"]

        for source_key, target in {
            "light": "light",
            "gas": "gas",
            "gases": "gas",
            "air": "gas",
            "air_quality": "gas",
            "airQuality": "gas",
            "mq135": "gas",
            "mq7": "gas",
            "co": "gas",
            "co_raw": "gas",
        }.items():
            if source_key in payload:
                value = parse_number(payload[source_key])
                STATE[target] = value
                changed[target] = value

        for source_key, target in {"ip": "ip", "message": "message", "msg": "message"}.items():
            if source_key in payload:
                STATE[target] = payload[source_key]
                changed[target] = payload[source_key]

        if is_gy33_telemetry_payload(payload):
            red = parse_number(payload.get("r"))
            green = parse_number(payload.get("g"))
            blue = parse_number(payload.get("b"))
            if all(isinstance(value, (int, float)) for value in (red, green, blue)):
                color = {
                    "r": int(red),
                    "g": int(green),
                    "b": int(blue),
                    "redRaw": parse_number(payload.get("red")),
                    "greenRaw": parse_number(payload.get("green")),
                    "blueRaw": parse_number(payload.get("blue")),
                    "clear": parse_number(payload.get("clear")),
                    "lux": parse_number(payload.get("lux")),
                    "ct": parse_number(payload.get("ct")),
                }
                STATE["color"] = color
                STATE["colorUpdatedAt"] = updated_at
                changed["color"] = color

        image = payload.get("image") or payload.get("camera")
        if image:
            STATE["camera"] = normalize_image(str(image))
            changed["camera"] = "updated"

        if not changed:
            return

        STATE["updatedAt"] = updated_at
        push_history({"type": "telemetry", "at": updated_at, "values": changed})
        save_state()

    def serve_motion(self, method: str) -> None:
        payload = normalize_telemetry_payload(self.read_payload())
        if payload:
            self.update_telemetry(payload)

        command = pop_command()
        if not command:
            command = auto_command()
        response = command or "NONE"
        STATE["motionRequestCount"] = int(STATE.get("motionRequestCount") or 0) + 1
        STATE["lastMotionRequest"] = {
            "at": now_ms(),
            "method": method,
            "client": self.client_address[0],
            "payload": payload,
            "response": response,
        }
        save_state()
        self.send_text(response)

    def read_payload(self) -> dict:
        length = int(self.headers.get("Content-Length", "0") or 0)
        raw = self.rfile.read(length) if length else b""
        content_type = self.headers.get("Content-Type", "")
        query_text = urlparse(self.path).query
        query = parse_qs(query_text)
        payload = {key: values[-1] for key, values in query.items()}

        if "application/json" in content_type:
            try:
                payload.update(json.loads(raw.decode("utf-8")))
                return payload
            except json.JSONDecodeError:
                return payload

        if "application/x-www-form-urlencoded" in content_type:
            text = raw.decode("utf-8")
            payload.update({key: values[-1] for key, values in parse_qs(text).items()})
            if text:
                payload["body"] = text
            return payload

        if query_text:
            payload["body"] = query_text
        if raw:
            payload["body"] = raw.decode("utf-8", errors="replace")
        return payload

    def send_static(self, path: Path) -> None:
        if not path.exists() or not path.is_file():
            self.send_error(404)
            return

        content_type = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
        data = path.read_bytes()

        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.end_headers()
        self.wfile.write(data)

    def send_json(self, payload: dict, status: int = 200) -> None:
        data = json.dumps(payload, ensure_ascii=True).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def send_text(self, value: str, status: int = 200) -> None:
        if not value.endswith("\n"):
            value += "\n"
        data = value.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, format: str, *args) -> None:
        print(f"{self.client_address[0]} - {format % args}")


def normalize_image(value: str) -> str:
    if value.startswith("data:image/"):
        return value

    try:
        base64.b64decode(value, validate=True)
        return f"data:image/jpeg;base64,{value}"
    except Exception:
        return value


def clear_telemetry() -> None:
    STATE["temperature"] = None
    STATE["humidity"] = None
    STATE["k10Temperature"] = None
    STATE["dhtTemperature"] = None
    STATE["dhtHumidity"] = None
    STATE["dhtPin"] = None
    STATE["dhtUpdatedAt"] = None
    STATE["envTemperature"] = None
    STATE["envHumidity"] = None
    STATE["envPressure"] = None
    STATE["envBmpTemperature"] = None
    STATE["envBmpAddress"] = None
    STATE["envUpdatedAt"] = None
    STATE["temperatureSource"] = None
    STATE["light"] = None
    STATE["gas"] = None
    STATE["color"] = None
    STATE["colorUpdatedAt"] = None
    STATE["updatedAt"] = None
    STATE["history"] = [entry for entry in STATE.get("history", []) if entry.get("type") != "telemetry"]


def command_entry_value(entry) -> str:
    if isinstance(entry, dict):
        return str(entry.get("command") or "").strip()
    return str(entry or "").strip()


def command_entry_time(entry) -> int:
    if not isinstance(entry, dict):
        return 0

    try:
        return int(entry.get("at") or 0)
    except (TypeError, ValueError):
        return 0


def normalize_command_queue() -> None:
    timestamp = now_ms()
    normalized = []
    for entry in STATE.get("commands", []):
        command = command_entry_value(entry)
        queued_at = command_entry_time(entry)
        if not command or not queued_at:
            continue
        if timestamp - queued_at <= COMMAND_TTL_MS:
            normalized.append({"command": command, "at": queued_at})

    STATE["commands"] = normalized[-COMMAND_QUEUE_LIMIT:]
    STATE["pendingCommand"] = STATE["commands"][0] if STATE["commands"] else None


def queue_command(command: str) -> None:
    timestamp = now_ms()
    pending = {"command": command, "at": timestamp}
    normalize_command_queue()
    STATE["commands"].append(pending)
    STATE["commands"] = STATE["commands"][-COMMAND_QUEUE_LIMIT:]
    STATE["pendingCommand"] = STATE["commands"][0]
    STATE["lastCommand"] = pending
    push_history({"type": "command", "at": timestamp, "command": command})


def pop_command() -> str | None:
    normalize_command_queue()
    if not STATE["commands"]:
        STATE["pendingCommand"] = None
        return None

    entry = STATE["commands"].pop(0)
    command = command_entry_value(entry)
    STATE["pendingCommand"] = STATE["commands"][0] if STATE["commands"] else None
    STATE["lastCommand"] = {"command": command, "at": now_ms()}
    return command


def peek_command() -> str | None:
    normalize_command_queue()
    return command_entry_value(STATE["commands"][0]) if STATE["commands"] else None


def auto_command() -> str | None:
    return None


def command_debug() -> dict:
    normalize_command_queue()
    timestamp = now_ms()
    pending = peek_command()
    pending_entries = list(STATE.get("commands", []))
    pending_commands = [command_entry_value(entry) for entry in pending_entries]
    pending_ages = [timestamp - command_entry_time(entry) for entry in pending_entries]
    return {
        "nextCommand": pending,
        "pendingCount": len(pending_commands),
        "pendingCommands": pending_commands,
        "pendingAgesMs": pending_ages,
        "commandTtlMs": COMMAND_TTL_MS,
        "lastCommand": STATE.get("lastCommand"),
        "lastMotionRequest": STATE.get("lastMotionRequest"),
        "motionRequestCount": STATE.get("motionRequestCount") or 0,
        "lastPollRequest": STATE.get("lastPollRequest"),
        "pollRequestCount": STATE.get("pollRequestCount") or 0,
        "lastTelemetryRequest": STATE.get("lastTelemetryRequest"),
        "telemetryRequestCount": STATE.get("telemetryRequestCount") or 0,
        "motionGetWouldReturn": pending or "NONE",
    }


def main() -> None:
    print(f"ModuBotic Control running on http://localhost:{PORT}")
    print("Use the PC network IP instead of localhost from the K10.")
    server = ThreadingHTTPServer((HOST, PORT), ControlHandler)
    server.serve_forever()


if __name__ == "__main__":
    main()
