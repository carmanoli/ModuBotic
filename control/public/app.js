const ids = {
  device: document.querySelector("#device"),
  status: document.querySelector("#status"),
  metricK10Temperature: document.querySelector("#metric-k10-temperature"),
  metricEnvTemperature: document.querySelector("#metric-env-temperature"),
  metricEnvHumidity: document.querySelector("#metric-env-humidity"),
  metricEnvPressure: document.querySelector("#metric-env-pressure"),
  metricLight: document.querySelector("#metric-light"),
  metricGas: document.querySelector("#metric-gas"),
  metricIp: document.querySelector("#metric-ip"),
  temperature: document.querySelector("#temperature"),
  envTemperature: document.querySelector("#env-temperature"),
  envHumidity: document.querySelector("#env-humidity"),
  envPressure: document.querySelector("#env-pressure"),
  light: document.querySelector("#light"),
  gas: document.querySelector("#gas"),
  ip: document.querySelector("#ip"),
  telemetryChart: document.querySelector("#telemetry-chart"),
  telemetryStats: document.querySelector("#telemetry-stats"),
  chartLegend: document.querySelector("#chart-legend"),
  message: document.querySelector("#message"),
  history: document.querySelector("#history"),
  commandForm: document.querySelector("#command-form"),
  commandSelect: document.querySelector("#command-select"),
  command: document.querySelector("#command"),
  lastCommand: document.querySelector("#last-command"),
  peekCommand: document.querySelector("#peek-command"),
  clearCommands: document.querySelector("#clear-commands"),
  nextCommand: document.querySelector("#next-command"),
  pendingCount: document.querySelector("#pending-count"),
  pendingCommands: document.querySelector("#pending-commands"),
  motionCount: document.querySelector("#motion-count"),
  lastMotionHit: document.querySelector("#last-motion-hit"),
  pollCount: document.querySelector("#poll-count"),
  lastPollHit: document.querySelector("#last-poll-hit"),
  telemetryCount: document.querySelector("#telemetry-count"),
  lastTelemetryHit: document.querySelector("#last-telemetry-hit"),
  mqttStatus: document.querySelector("#mqtt-status"),
  i2cDevices: document.querySelector("#i2c-devices"),
  scanI2c: document.querySelector("#scan-i2c"),
  readColor: document.querySelector("#read-color"),
  debugColor: document.querySelector("#debug-color"),
  colorSwatch: document.querySelector("#color-swatch"),
  colorRgb: document.querySelector("#color-rgb"),
  colorUpdated: document.querySelector("#color-updated"),
  clearTelemetry: document.querySelector("#clear-telemetry"),
};

const SENSOR_FRESH_MS = 30000;

function formatMetric(value, suffix = "") {
  if (value === null || value === undefined || value === "") return "--";
  if (typeof value === "number") return `${value.toFixed(1)}${suffix}`;
  return `${value}${suffix}`;
}

function formatTime(ms) {
  if (!ms) return "sem dados";
  return new Date(ms).toLocaleTimeString("pt-PT");
}

function isOnline(updatedAt) {
  return updatedAt && Date.now() - updatedAt < 15000;
}

function isFresh(updatedAt, maxAgeMs = SENSOR_FRESH_MS) {
  return updatedAt && Date.now() - updatedAt < maxAgeMs;
}

function numericValue(value) {
  if (value === null || value === undefined || value === "") return null;
  const number = Number(value);
  return Number.isFinite(number) ? number : null;
}

function clampRgb(value) {
  const number = Number(value);
  if (!Number.isFinite(number)) return 0;
  return Math.max(0, Math.min(255, Math.round(number)));
}

function collectSeries(history, keys) {
  const keyList = Array.isArray(keys) ? keys : [keys];
  return (history || [])
    .filter((item) => item.type === "telemetry" && item.values)
    .map((item) => {
      for (const key of keyList) {
        if (item.values[key] !== undefined) {
          return { at: item.at, value: numericValue(item.values[key]) };
        }
      }
      return null;
    })
    .filter(Boolean)
    .filter((item) => item.value !== null)
    .reverse();
}

function drawLine(ctx, points, color, chart, min, max) {
  if (!points.length) return;

  const range = max - min || 1;

  if (points.length === 1) {
    const x = chart.x + chart.width;
    const y = chart.y + chart.height - ((points[0].value - min) / range) * chart.height;
    ctx.beginPath();
    ctx.arc(x, y, 4, 0, Math.PI * 2);
    ctx.fillStyle = color;
    ctx.fill();
    return;
  }

  ctx.beginPath();
  points.forEach((point, index) => {
    const x = chart.x + (index / (points.length - 1)) * chart.width;
    const y = chart.y + chart.height - ((point.value - min) / range) * chart.height;
    if (index === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.strokeStyle = color;
  ctx.lineWidth = 2.5;
  ctx.stroke();
}

function getSeriesRange(series) {
  const values = series.flatMap((item) => item.values.map((point) => point.value));
  if (!values.length) return { min: 0, max: 1 };
  return { min: Math.min(...values), max: Math.max(...values) };
}

function formatSeriesRange(item) {
  if (item.values.length) {
    const values = item.values.map((point) => point.value);
    const min = Math.min(...values);
    const max = Math.max(...values);
    return `${min.toFixed(1)}-${max.toFixed(1)}`;
  }

  return "sem dados";
}

function renderTelemetryLegend(series) {
  if (!ids.chartLegend) return;

  ids.chartLegend.innerHTML = "";
  for (const item of series) {
    const label = document.createElement("span");
    label.textContent = item.label;
    label.style.color = item.color;
    ids.chartLegend.appendChild(label);
  }
}

function renderTelemetryStats(series) {
  if (!ids.telemetryStats) return;

  ids.telemetryStats.innerHTML = "";
  for (const item of series) {
    const stat = document.createElement("div");
    stat.className = "chart-stat";
    stat.style.color = item.color;

    const label = document.createElement("strong");
    label.textContent = item.label;

    const range = document.createElement("span");
    range.textContent = formatSeriesRange(item);

    stat.append(label, range);
    ids.telemetryStats.appendChild(stat);
  }
}

function renderTelemetryChart(history, state = {}) {
  const canvas = ids.telemetryChart;
  if (!canvas) return;

  const rect = canvas.getBoundingClientRect();
  const ratio = window.devicePixelRatio || 1;
  canvas.width = Math.max(1, Math.floor(rect.width * ratio));
  canvas.height = Math.max(1, Math.floor(rect.height * ratio));

  const ctx = canvas.getContext("2d");
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  ctx.clearRect(0, 0, rect.width, rect.height);

  const chart = { x: 46, y: 10, width: rect.width - 66, height: rect.height - 40 };
  const series = [];

  if (isFresh(state.envUpdatedAt) || collectSeries(history, "envTemperature").length > 0) {
    series.push(
      { key: "envTemperature", label: "Temperature(AHT20)", color: "#ea580c", scale: "temperature", values: collectSeries(history, "envTemperature") },
      { key: "envHumidity", label: "Humidity(AHT20)", color: "#0891b2", scale: "humidity", values: collectSeries(history, "envHumidity") },
      { key: "envPressure", label: "Pressure(BMP280)", color: "#7c3aed", scale: "pressure", values: collectSeries(history, "envPressure") },
    );
  }

  const active = series.filter((item) => item.values.length > 0);
  renderTelemetryLegend(active);
  renderTelemetryStats(active);
  const ranges = new Map();

  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, rect.width, rect.height);
  ctx.strokeStyle = "#d9e0ea";
  ctx.lineWidth = 1;
  ctx.strokeRect(chart.x, chart.y, chart.width, chart.height);

  for (let i = 1; i < 4; i++) {
    const y = chart.y + (chart.height / 4) * i;
    ctx.beginPath();
    ctx.moveTo(chart.x, y);
    ctx.lineTo(chart.x + chart.width, y);
    ctx.strokeStyle = "#eef2f7";
    ctx.stroke();
  }

  if (!active.length) {
    ctx.fillStyle = "#647084";
    ctx.font = "14px Arial";
    const message = series.length
      ? "A espera de dados dos sensores I2C"
      : "Sem dados de sensores I2C";
    ctx.fillText(message, chart.x + 12, chart.y + 32);
    return;
  }

  for (const scale of new Set(active.map((item) => item.scale))) {
    ranges.set(scale, getSeriesRange(active.filter((item) => item.scale === scale)));
  }

  active.forEach((item, index) => {
    const range = ranges.get(item.scale);
    drawLine(ctx, item.values, item.color, chart, range.min, range.max);
  });

  const first = active[0].values[0]?.at;
  const last = active[0].values.at(-1)?.at;
  ctx.fillStyle = "#647084";
  ctx.font = "12px Arial";
  if (first) ctx.fillText(formatTime(first), chart.x, rect.height - 12);
  if (last) ctx.fillText(formatTime(last), chart.x + chart.width - 56, rect.height - 12);
}

async function refresh() {
  const response = await fetch("/api/status", { cache: "no-store" });
  const state = await response.json();

  ids.device.textContent = state.device || "A espera de mind";
  const hasEnv = isFresh(state.envUpdatedAt);
  const hasGas = state.gas !== null && state.gas !== undefined && state.gas !== "";

  ids.metricEnvTemperature?.classList.toggle("hidden", !hasEnv || state.envTemperature === null || state.envTemperature === undefined);
  ids.metricEnvHumidity?.classList.toggle("hidden", !hasEnv || state.envHumidity === null || state.envHumidity === undefined);
  ids.metricEnvPressure?.classList.toggle("hidden", !hasEnv || state.envPressure === null || state.envPressure === undefined);
  ids.metricGas?.classList.toggle("hidden", !hasGas);

  ids.temperature.textContent = formatMetric(state.k10Temperature ?? state.temperature, " C");
  if (ids.envTemperature) ids.envTemperature.textContent = formatMetric(hasEnv ? state.envTemperature : null, " C");
  if (ids.envHumidity) ids.envHumidity.textContent = formatMetric(hasEnv ? state.envHumidity : null, " %");
  if (ids.envPressure) ids.envPressure.textContent = formatMetric(hasEnv ? state.envPressure : null, " hPa");
  ids.light.textContent = formatMetric(state.light);
  if (ids.gas) ids.gas.textContent = formatMetric(state.gas);
  ids.ip.textContent = state.ip || "--";
  ids.message.textContent = state.message || "A espera de dados da mind";
  ids.lastCommand.textContent = state.lastCommand?.command || "--";
  renderColorReading(state.color, state.colorUpdatedAt);
  renderI2cDevices(state.i2cDevices || [], state.i2cUpdatedAt, state);

  const online = isOnline(state.updatedAt);
  ids.status.textContent = online ? "online" : "offline";
  ids.status.classList.toggle("online", online);

  const history = state.history || [];

  ids.history.innerHTML = "";
  for (const item of history.slice(0, 12)) {
    const li = document.createElement("li");
    const label = formatHistoryItem(item);
    li.textContent = `${formatTime(item.at)} ${label}`;
    ids.history.appendChild(li);
  }

  renderTelemetryChart(state.history || [], state);

  await refreshCommandDebug();
}

function renderColorReading(color, updatedAt) {
  if (!ids.colorSwatch || !ids.colorRgb || !ids.colorUpdated) return;

  if (!color || color.r === null || color.g === null || color.b === null) {
    ids.colorSwatch.style.background = "#e2e8f0";
    ids.colorRgb.textContent = "RGB --";
    ids.colorUpdated.textContent = "sem leitura";
    return;
  }

  const r = clampRgb(color.r);
  const g = clampRgb(color.g);
  const b = clampRgb(color.b);
  ids.colorSwatch.style.background = `rgb(${r}, ${g}, ${b})`;
  ids.colorRgb.textContent = `RGB ${r}, ${g}, ${b}`;
  ids.colorUpdated.textContent = formatTime(updatedAt);
}

function formatI2cDeviceName(name) {
  return String(name || "UNKNOWN").replaceAll("_", " ");
}

function renderI2cDevices(devices, updatedAt, state = {}) {
  if (!ids.i2cDevices) return;

  ids.i2cDevices.innerHTML = "";
  const isScanning = state.i2cScanStatus === "scanning";

  if (ids.scanI2c) {
    ids.scanI2c.disabled = isScanning;
    ids.scanI2c.textContent = isScanning ? "Scan I2C em curso..." : "Scan I2C";
  }

  if (isScanning) {
    const pending = document.createElement("div");
    pending.className = "i2c-empty";
    pending.textContent = `Scan pedido ${formatTime(state.i2cScanRequestedAt)}; a espera do robot`;
    ids.i2cDevices.appendChild(pending);
    return;
  }

  if (!devices.length) {
    const empty = document.createElement("div");
    empty.className = "i2c-empty";
    empty.textContent = updatedAt
      ? `Nenhum dispositivo I2C (${formatTime(updatedAt)})`
      : "Sem scan I2C nesta sessao";
    ids.i2cDevices.appendChild(empty);
    return;
  }

  const summary = document.createElement("div");
  summary.className = "i2c-empty";
  summary.textContent = `Ultimo scan ${formatTime(updatedAt)}`;
  ids.i2cDevices.appendChild(summary);

  for (const device of devices) {
    const row = document.createElement("div");
    row.className = "i2c-device";

    const address = document.createElement("strong");
    address.textContent = device.address;

    const name = document.createElement("span");
    name.textContent = formatI2cDeviceName(device.name);

    row.append(address, name);
    ids.i2cDevices.appendChild(row);
  }
}

async function scanI2cBus() {
  await fetch("/api/i2c/scan", { method: "POST" });
  await refresh();
}

function formatHistoryItem(item) {
  if (item.type === "camera") return "camera";
  if (item.type === "command") return `cmd ${item.command}`;
  if (item.type === "color") return `cor RGB ${item.color.r}, ${item.color.g}, ${item.color.b}`;

  const values = item.values || {};
  if (values.color) {
    const color = values.color;
    return `cor RGB ${color.r}, ${color.g}, ${color.b}`;
  }

  const parts = [];
  if (values.k10Temperature !== undefined) parts.push(`Mind ${formatMetric(values.k10Temperature, " C")}`);
  if (values.envTemperature !== undefined) parts.push(`AHT20 ${formatMetric(values.envTemperature, " C")}`);
  if (values.envHumidity !== undefined) parts.push(`AHT20 H ${formatMetric(values.envHumidity, " %")}`);
  if (values.envPressure !== undefined) parts.push(`BMP280 ${formatMetric(values.envPressure, " hPa")}`);
  if (values.light !== undefined) parts.push(`Luz ${formatMetric(values.light)}`);
  if (values.gas !== undefined) parts.push(`Gas ${formatMetric(values.gas)}`);

  return parts.length ? parts.join(" | ") : JSON.stringify(values);
}

async function refreshCommandDebug() {
  const response = await fetch("/api/motion/peek", { cache: "no-store" });
  const debug = await response.json();

  ids.nextCommand.textContent = debug.nextCommand || "NONE";
  ids.pendingCount.textContent = debug.pendingCount;
  ids.pendingCommands.textContent = debug.pendingCommands.length
    ? debug.pendingCommands.join(", ")
    : "--";
  ids.motionCount.textContent = debug.motionRequestCount || 0;
  ids.lastMotionHit.textContent = debug.lastMotionRequest
    ? `${formatTime(debug.lastMotionRequest.at)} ${debug.lastMotionRequest.method} ${debug.lastMotionRequest.client} -> ${debug.lastMotionRequest.response}`
    : "--";
  ids.pollCount.textContent = debug.pollRequestCount || 0;
  ids.lastPollHit.textContent = debug.lastPollRequest
    ? `${formatTime(debug.lastPollRequest.at)} ${debug.lastPollRequest.client} -> ${debug.lastPollRequest.response}`
    : "--";
  ids.telemetryCount.textContent = debug.telemetryRequestCount || 0;
  ids.lastTelemetryHit.textContent = debug.lastTelemetryRequest
    ? `${formatTime(debug.lastTelemetryRequest.at)} ${debug.lastTelemetryRequest.client} -> ${debug.lastTelemetryRequest.response || ""}`
    : "--";
  if (ids.mqttStatus) {
    const mqtt = debug.mqtt || {};
    if (!mqtt.enabled) {
      ids.mqttStatus.textContent = "off";
    } else if (!mqtt.available) {
      ids.mqttStatus.textContent = "sem paho-mqtt";
    } else if (mqtt.connected) {
      const last = mqtt.lastMessage
        ? ` | ${formatTime(mqtt.lastMessage.at)} ${mqtt.lastMessage.topic}: ${mqtt.lastMessage.payload}`
        : "";
      ids.mqttStatus.textContent = `online ${mqtt.commandTopic}${last}`;
    } else {
      ids.mqttStatus.textContent = `offline ${mqtt.host}:${mqtt.port} ${mqtt.lastError || ""}`;
    }
  }
}

async function sendCommand(command) {
  await fetch("/api/commands", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ command }),
  });
  await refresh();
}

ids.commandForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const selectedCommand = ids.commandSelect?.value || "";
  const command = selectedCommand === "manual"
    ? ids.command.value.trim()
    : selectedCommand.trim();
  if (!command) return;

  await sendCommand(command);
  if (selectedCommand === "manual") {
    ids.command.value = "";
  }
});

ids.commandSelect?.addEventListener("change", () => {
  const isManual = ids.commandSelect.value === "manual";
  ids.command?.classList.toggle("hidden", !isManual);
  if (isManual) {
    ids.command?.focus();
  }
});

document.addEventListener("click", async (event) => {
  const button = event.target.closest("[data-command]");
  if (!button) return;
  await sendCommand(button.dataset.command);
});

ids.peekCommand.addEventListener("click", refreshCommandDebug);

ids.clearCommands.addEventListener("click", async () => {
  await fetch("/api/commands/clear", { method: "POST" });
  await refresh();
});

ids.clearTelemetry?.addEventListener("click", async () => {
  await fetch("/api/telemetry/clear", { method: "POST" });
  await refresh();
});

ids.scanI2c?.addEventListener("click", scanI2cBus);

ids.readColor?.addEventListener("click", async () => {
  await sendCommand("GY33_READ");
});

ids.debugColor?.addEventListener("click", async () => {
  await sendCommand("GY33_DEBUG");
});

refresh();
setInterval(refresh, 1000);
