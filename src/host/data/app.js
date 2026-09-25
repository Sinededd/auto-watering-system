"use strict";
const $ = (id) => document.getElementById(id);
const state = {
  devices: [],
  pairing: [],
  readings: {},
  thresholds: [],
  irrigation: null,
  selected: null,
  calibration: null,
};
let refreshing = false,
  toastTimer;
const esc = (v) =>
  String(v ?? "").replace(
    /[&<>"']/g,
    (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c],
  );
function toast(message, error = false) {
  const el = $("toast");
  el.textContent = message;
  el.classList.toggle("error", error);
  el.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => (el.hidden = true), 4500);
}
function connection(ok, label) {
  const el = $("connection");
  el.className = "connection " + (ok ? "online" : "offline");
  el.querySelector("span").textContent = label;
}
async function command(cmd, payload = {}) {
  let response;
  try {
    response = await fetch("/api/command", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ cmd, payload }),
    });
  } catch (e) {
    connection(false, "Нет связи с хостом");
    throw Error("Нет связи с хостом");
  }
  if (!response.ok) throw Error("Ошибка HTTP " + response.status);
  const result = await response.json();
  if (!result.success) throw Error(result.message || "Команда не выполнена");
  connection(true, "Хост подключён");
  return result.data || {};
}
async function run(fn, message) {
  try {
    await fn();
    if (message) toast(message);
  } catch (e) {
    toast(e.message, true);
  }
}
function empty(message) {
  return '<div class="empty">' + esc(message) + "</div>";
}
function valuesFor(device) {
  const rows = state.readings[device.type] || {};
  const row =
    rows[device.mac] ||
    rows[device.mac.toLowerCase()] ||
    rows[device.mac.toUpperCase()];
  return (row?.values || []).map(Number).filter(Number.isFinite);
}
function deviceRow(d, compact) {
  const values = valuesFor(d),
    reading = values.length
      ? (values.reduce((a, b) => a + b, 0) / values.length).toFixed(0) + "%"
      : "—";
  const status =
    Number(d.status) === 2
      ? "Активен"
      : Number(d.status) === 3
        ? "Нет связи"
        : d.statusStr || "Ожидание";
  return (
    '<div class="item"><div class="item-main"><span class="item-icon">❀</span><div><b>' +
    esc(d.type || "Датчик") +
    "</b><small>" +
    esc(d.mac) +
    " · " +
    esc(status) +
    '</small></div></div><div class="item-end"><span class="reading">' +
    reading +
    "</span>" +
    (compact
      ? ""
      : '<button class="btn secondary small" data-details="' +
        esc(d.mac) +
        '">Подробнее</button>') +
    "</div></div>"
  );
}
function renderDevices() {
  const list = state.devices;
  $("device-list").innerHTML = list.length
    ? list.map((d) => deviceRow(d, false)).join("")
    : empty(
        "Пока нет подключённых датчиков. Переведите датчик в режим сопряжения.",
      );
  $("overview-devices").innerHTML = list.length
    ? list
        .slice(0, 3)
        .map((d) => deviceRow(d, true))
        .join("")
    : empty("Показания появятся после подключения датчика.");
  $("pairing-list").innerHTML = state.pairing.length
    ? state.pairing
        .map(
          (d) =>
            '<div class="item"><div class="item-main"><span class="item-icon">✦</span><div><b>' +
            esc(d.type || "Новый датчик") +
            "</b><small>" +
            esc(d.mac) +
            " · сигнал " +
            esc(d.sigLevel ?? "—") +
            ' dBm</small></div></div><button class="btn small" data-pair="' +
            esc(d.mac) +
            '">Подключить</button></div>',
        )
        .join("")
    : empty("Запросов пока нет. Включите режим сопряжения на датчике.");
  $("device-count").textContent = list.length;
  $("pair-count").textContent = state.pairing.length;
  $("stat-devices").textContent = list.length;
  $("sensor-types").innerHTML = [
    ...new Set(list.map((d) => d.type).filter(Boolean)),
  ]
    .map((t) => '<option value="' + esc(t) + '"></option>')
    .join("");
  const vals = list.flatMap(valuesFor);
  $("stat-moisture").textContent = vals.length
    ? (vals.reduce((a, b) => a + b, 0) / vals.length).toFixed(0)
    : "—";
}
function renderIrrigation() {
  const d = state.irrigation;
  if (!d) return;
  const enabled = Boolean(d.enabled),
    status = String(d.state || ""),
    watering = status.includes("WATERING"),
    error = status.includes("ERROR");
  $("auto-enabled").checked = enabled;
  $("irrigation-badge").textContent = enabled ? "Включён" : "Выключен";
  $("irrigation-badge").className = "badge" + (enabled ? "" : " warn");
  $("overview-mode").textContent = enabled ? "Автоматический" : "Выключен";
  $("overview-mode").className = "badge" + (enabled ? "" : " warn");
  $("irrigation-state").textContent = error
    ? "Нет свежих данных от датчиков. Автополив приостановлен."
    : watering
      ? "Идёт полив растений. Клапан открыт."
      : !enabled
        ? "Автоматический режим выключен."
        : status.includes("COOLDOWN")
          ? "Пауза до следующего полива: " +
            Number(d.remaining_cooldown_sec || 0) +
            " сек."
          : "Система наблюдает за влажностью почвы.";
  $("irrigation-state").classList.toggle("warning", error);
  $("stat-water").textContent = watering ? "Идёт" : enabled ? "Авто" : "Выкл";
  $("overview-duration").textContent = d.duration_sec ?? "—";
  $("overview-cooldown").textContent = d.cooldown_sec ?? "—";
  if (document.activeElement !== $("duration"))
    $("duration").value = d.duration_sec ?? "";
  if (document.activeElement !== $("cooldown"))
    $("cooldown").value = d.cooldown_sec ?? "";
  $("hero-title").textContent = error
    ? "Датчики не передают данные"
    : watering
      ? "Растения получают воду"
      : enabled
        ? "Всё растёт своим чередом"
        : "Автополив выключен";
  $("hero-copy").textContent = error
    ? "Проверьте датчики и дождитесь новых показаний."
    : watering
      ? "Система выполняет цикл полива."
      : enabled
        ? "Система следит за влажностью и польёт растения при необходимости."
        : "Включите автоматический режим, когда будете готовы.";
  $("overview-note").textContent = error
    ? "Для запуска полива дождитесь свежих данных от датчиков."
    : enabled
      ? "Режим управляется автоматически по заданным порогам."
      : "Автоматический режим сейчас выключен.";
}
function renderThresholds() {
  const list = state.thresholds;
  $("overview-thresholds").textContent = list.length;
  $("threshold-list").innerHTML = list.length
    ? list
        .map(
          (t) =>
            '<div class="item"><div class="item-main"><span class="item-icon">◫</span><div><b>' +
            esc(t.type) +
            "</b><small>Старт ниже " +
            esc(t.min) +
            "% · стоп при " +
            esc(t.max) +
            "%<br>" +
            esc(
              {
                AVERAGE: "Средняя влажность",
                MIN: "Самое сухое",
                MAX: "Самое влажное",
              }[t.mode] || t.mode,
            ) +
            '</small></div></div><div class="item-end"><button class="btn secondary small" data-edit-threshold="' +
            esc(t.type) +
            '">Изменить</button><button class="btn danger small" data-remove-threshold="' +
            esc(t.type) +
            '">Удалить</button></div></div>',
        )
        .join("")
    : empty("Правил пока нет. Добавьте порог для датчика слева.");
}
async function refresh() {
  if (refreshing) return;
  refreshing = true;
  try {
    const [devices, sensors, thresholds, irrigation] = await Promise.all([
      command("device", { action: "list" }),
      command("sensor", { action: "list" }),
      command("threshold", { action: "list" }),
      command("irrigation", { action: "status" }),
    ]);
    state.devices = devices.connected || [];
    state.pairing = devices.pairing || [];
    state.readings = sensors.sensors || {};
    state.thresholds = thresholds.thresholds || [];
    state.irrigation = irrigation;
    renderDevices();
    renderThresholds();
    renderIrrigation();
  } catch (e) {
    connection(false, "Нет связи с хостом");
    toast(e.message, true);
  } finally {
    refreshing = false;
  }
}
function page(name) {
  document
    .querySelectorAll(".page")
    .forEach((el) => el.classList.toggle("active", el.id === name));
  document
    .querySelectorAll(".sidebar nav button")
    .forEach((el) => el.classList.toggle("active", el.dataset.page === name));
  $("breadcrumb").textContent = {
    overview: "Обзор",
    watering: "Полив",
    sensors: "Датчики",
    thresholds: "Пороги",
  }[name];
  window.scrollTo({ top: 0, behavior: "smooth" });
}
function closeModal() {
  state.selected = null;
  state.calibration = null;
  $("modal-backdrop").hidden = true;
}
async function openDevice(mac) {
  state.selected = mac;
  $("modal-backdrop").hidden = false;
  $("modal-title").textContent = "Настройки датчика";
  $("modal-mac").textContent = mac;
  $("modal-content").innerHTML = empty("Загружаем данные датчика…");
  try {
    const data = await command("device", { action: "details", mac });
    if (state.selected !== mac) return;
    const dev = data.device,
      settings = dev.settings || {},
      metrics = dev.interface?.metrics || [];
    $("modal-content").innerHTML =
      '<h3>Параметры датчика</h3><p class="hint">Новые настройки применятся после следующего пробуждения датчика.</p><form id="device-form"><div class="form-grid"><label class="field">Период сна, сек<input id="sleep-seconds" type="number" min="1" max="86400" step="1" required value="' +
      esc(settings.timeToSleepSec ?? 60) +
      '"></label><label class="field">Мощность Wi-Fi<input id="wifi-power" type="number" min="0" max="84" step="1" required value="' +
      esc(settings.wifiPower ?? 0) +
      '"></label><label class="field">Макс. пропусков<input id="skip-boots" type="number" min="0" max="255" step="1" required value="' +
      esc(settings.maxSkippedBoots ?? 0) +
      '"><small>Пропуски циклов перед передачей</small></label></div><div class="actions"><button class="btn" type="submit">Передать настройки</button><button id="disconnect-device" class="btn danger" type="button">Удалить датчик</button></div></form><hr><h3>Калибровка показаний</h3><p class="hint">Калибруйте каждый показатель отдельно.</p><div class="list">' +
      (metrics.length
        ? metrics
            .map(
              (m, i) =>
                '<div class="item"><div><b>' +
                esc(m.label || "Показание " + (i + 1)) +
                "</b><small>Диапазон: " +
                esc(m.minVal) +
                "–" +
                esc(m.maxVal) +
                " " +
                esc(m.unit || "") +
                " · точек калибровки: " +
                esc(m.calib?.pointsCount || 0) +
                '</small></div><button class="btn secondary small" data-calibrate="' +
                i +
                '">Калибровать</button></div>',
            )
            .join("")
        : empty("Датчик не сообщает измеряемые показатели.")) +
      "</div>";
    $("device-form").addEventListener("submit", saveDevice);
    $("disconnect-device").addEventListener("click", () => removeDevice(mac));
  } catch (e) {
    $("modal-content").innerHTML = empty(e.message);
  }
}
async function saveDevice(e) {
  e.preventDefault();
  const mac = state.selected;
  await run(async () => {
    await command("device", {
      action: "set_settings",
      mac,
      timeToSleepSec: Number($("sleep-seconds").value),
      wifiPower: Number($("wifi-power").value),
      maxSkippedBoots: Number($("skip-boots").value),
    });
    toast("Запрос отправлен. Разбудите датчик для применения.");
    closeModal();
  });
}
async function removeDevice(mac) {
  if (!confirm("Удалить датчик " + mac + " из системы?")) return;
  await run(async () => {
    await command("device", { action: "disconnect", mac });
    closeModal();
    await refresh();
  }, "Датчик удалён");
}
function renderCalibration() {
  const c = state.calibration;
  if (!c) return;
  const d = c.data || {},
    phase = d.state || "starting",
    raw = d.latestRawMilliVolts;
  const labels = {
    starting: "Ожидаем датчик",
    streaming: "Принимаем показания",
    finishing: "Сохраняем",
    cancelling: "Отменяем",
    completed: "Готово",
    cancelled: "Отменено",
    failed: "Ошибка",
  };
  $("modal-title").textContent = "Калибровка датчика";
  $("modal-mac").textContent = c.mac + " · показатель " + (c.index + 1);
  $("modal-content").innerHTML =
    '<div class="notice">Разбудите датчик кнопкой. Для сухой точки поместите сенсор в сухую среду, для влажной — во влажную. Дождитесь стабильного значения перед фиксацией.</div><div class="steps"><span class="step ' +
    (d.dryMilliVolts != null ? "done" : "") +
    '">1 · Сухая точка</span><span class="step ' +
    (d.wetMilliVolts != null ? "done" : "") +
    '">2 · Влажная точка</span><span class="step">3 · Сохранить</span></div><p class="hint">Состояние: ' +
    esc(labels[phase] || phase) +
    (d.error ? " · " + esc(d.error) : "") +
    '</p><div class="calibration-value">' +
    (raw != null && d.sampleFresh ? esc(raw) + " мВ" : "— мВ") +
    '</div><p class="hint">' +
    (d.sampleFresh
      ? "Показание получено от датчика."
      : "Ожидаем свежее показание…") +
    '</p><div class="actions"><button class="btn secondary" data-capture="dry" ' +
    (phase !== "streaming" || !d.sampleFresh ? "disabled" : "") +
    '>Записать сухую</button><button class="btn secondary" data-capture="wet" ' +
    (phase !== "streaming" || !d.sampleFresh ? "disabled" : "") +
    '>Записать влажную</button><button class="btn" data-cal-action="finish" ' +
    (phase !== "streaming" || d.dryMilliVolts == null || d.wetMilliVolts == null
      ? "disabled"
      : "") +
    '>Сохранить</button><button class="btn secondary" data-cal-action="cancel" ' +
    (["completed", "cancelled", "failed"].includes(phase) ? "disabled" : "") +
    ">Отменить</button></div>";
}
async function calibration(action, extra = {}) {
  const c = state.calibration;
  if (!c) return;
  await run(async () => {
    const result = await command("calibration", {
      action,
      mac: c.mac,
      ...extra,
    });
    if (result.calibration) c.data = result.calibration;
    renderCalibration();
    toast(
      action === "finish"
        ? "Сохранение запрошено. Дождитесь подтверждения."
        : action === "capture"
          ? "Точка зафиксирована"
          : "Отмена запрошена",
    );
  });
}
function connectEvents() {
  const source = new EventSource("/api/events");
  source.onopen = () => connection(true, "Хост подключён");
  source.onerror = () => connection(false, "Переподключение…");
  const listen = (event, handler) =>
    source.addEventListener(event, (e) => {
      try {
        handler(JSON.parse(e.data).payload || {});
      } catch (err) {
        console.error(err);
      }
    });
  [
    "device_discovered",
    "device_lost",
    "device_updated",
    "device_telemetry",
    "irrigation_state_changed",
  ].forEach((name) => listen(name, refresh));
  listen("settings_update_success", (p) => {
    toast("Датчик " + (p.mac || "") + " применил настройки");
    refresh();
  });
  listen("settings_update_failed", (p) =>
    toast("Датчик " + (p.mac || "") + " не применил настройки", true),
  );
  listen("calibration_sample", (p) => {
    const c = state.calibration;
    if (c && String(p.mac).toLowerCase() === c.mac.toLowerCase()) {
      c.data = {
        ...c.data,
        latestRawMilliVolts: p.rawMilliVolts,
        sampleFresh: true,
      };
      renderCalibration();
    }
  });
}
document.addEventListener("click", async (e) => {
  const nav = e.target.closest("[data-page]");
  if (nav) {
    page(nav.dataset.page);
    return;
  }
  const pair = e.target.closest("[data-pair]");
  if (pair) {
    await run(async () => {
      await command("device", { action: "connect", mac: pair.dataset.pair });
      await refresh();
    }, "Запрос подключения отправлен. Разбудите датчик.");
    return;
  }
  const detail = e.target.closest("[data-details]");
  if (detail) {
    openDevice(detail.dataset.details);
    return;
  }
  const edit = e.target.closest("[data-edit-threshold]");
  if (edit) {
    const t = state.thresholds.find(
      (x) => x.type === edit.dataset.editThreshold,
    );
    if (t) {
      $("threshold-type").value = t.type;
      $("threshold-min").value = t.min;
      $("threshold-max").value = t.max;
      $("threshold-mode").value =
        { AVERAGE: "avg", MIN: "min", MAX: "max" }[t.mode] || "avg";
      $("threshold-type").focus();
    }
    return;
  }
  const remove = e.target.closest("[data-remove-threshold]");
  if (remove) {
    const type = remove.dataset.removeThreshold;
    if (confirm("Удалить порог для " + type + "?"))
      await run(async () => {
        await command("threshold", { action: "remove", type });
        await refresh();
      }, "Порог удалён");
    return;
  }
  const cal = e.target.closest("[data-calibrate]");
  if (cal) {
    const mac = state.selected,
      index = Number(cal.dataset.calibrate);
    state.calibration = { mac, index, data: { state: "starting" } };
    renderCalibration();
    await run(async () => {
      const result = await command("calibration", {
        action: "start",
        mac,
        metricIndex: index,
      });
      state.calibration.data = result.calibration || {};
      renderCalibration();
      toast("Разбудите датчик для начала калибровки");
    });
    return;
  }
  const capture = e.target.closest("[data-capture]");
  if (capture) {
    calibration("capture", { point: capture.dataset.capture });
    return;
  }
  const action = e.target.closest("[data-cal-action]");
  if (action) calibration(action.dataset.calAction);
});
$("auto-enabled").addEventListener("change", (e) => {
  const desired = e.target.checked;
  run(
    async () => {
      await command("irrigation", { action: desired ? "enable" : "disable" });
      await refresh();
    },
    desired ? "Автополив включён" : "Автополив выключен",
  ).then(() => {
    if (state.irrigation)
      $("auto-enabled").checked = Boolean(state.irrigation.enabled);
  });
});
$("timing-form").addEventListener("submit", (e) => {
  e.preventDefault();
  run(async () => {
    await command("irrigation", {
      action: "timing",
      duration: String($("duration").value),
      cooldown: String($("cooldown").value),
    });
    await refresh();
  }, "Время полива сохранено");
});
$("threshold-form").addEventListener("submit", (e) => {
  e.preventDefault();
  const min = Number($("threshold-min").value),
    max = Number($("threshold-max").value);
  if (min >= max) {
    toast("Нижний порог должен быть меньше верхнего", true);
    return;
  }
  run(async () => {
    await command("threshold", {
      action: "set",
      type: $("threshold-type").value.trim(),
      min: String(min),
      max: String(max),
      mode: $("threshold-mode").value,
    });
    await refresh();
  }, "Порог сохранён");
});
$("manual-start").addEventListener("click", () =>
  run(async () => {
    await command("irrigation", { action: "start" });
    await refresh();
  }, "Полив запущен"),
);
$("manual-stop").addEventListener("click", () =>
  run(async () => {
    await command("irrigation", { action: "stop" });
    await refresh();
  }, "Полив остановлен"),
);
$("refresh-overview").addEventListener("click", refresh);
$("refresh-sensors").addEventListener("click", refresh);
$("modal-close").addEventListener("click", closeModal);
$("modal-backdrop").addEventListener("click", (e) => {
  if (e.target === $("modal-backdrop")) closeModal();
});
document.addEventListener("keydown", (e) => {
  if (e.key === "Escape" && !$("modal-backdrop").hidden) closeModal();
});
connectEvents();
refresh();
setInterval(() => {
  if (document.hidden) return;
  refresh();
  const c = state.calibration;
  if (
    c &&
    c.data?.state &&
    !["completed", "cancelled", "failed"].includes(c.data.state)
  )
    command("calibration", { action: "status", mac: c.mac })
      .then((d) => {
        if (state.calibration === c) {
          c.data = d.calibration;
          renderCalibration();
        }
      })
      .catch(() => {});
}, 5000);
