# Калибровка датчика влажности

## Общая схема

Калибровка состоит из двух независимых частей:

1. Клиент отправляет HTTP-запросы на ESP32-хост.
2. Хост обменивается с датчиком бинарными пакетами ESP-NOW.

Клиент напрямую с датчиком не общается. Поэтому веб-интерфейс, Postman и `curl`
могут использовать одну и ту же команду `/api/command`.

Хост создаёт Wi-Fi-точку доступа:

```text
SSID: AutoWateringHost
Password: waterplants
Host API: http://192.168.4.1
```

Значения находятся в `src/host/Config.h` в секции `Config::Network`.

```text
кнопка датчика
      ↓
WakeupState (окно 30 секунд)
      ↓ CALIBRATION_START_REQUEST
CalibrationState (измерение раз в секунду)
      ↓ CALIBRATION_FINISH_REQUEST или CALIBRATION_CANCEL_REQUEST
WorkingState → обычная телеметрия → deep sleep
```

## Где находится логика

### Общий радиопротокол

`lib/network/Packets.h` содержит все ESP-NOW-пакеты. Для калибровки добавлены:

- `WakeupReportPacket` — сообщает хосту, что датчик разбужен кнопкой;
- `CalibrationStartRequestPacket` и `CalibrationStartResponsePacket`;
- `CalibrationSamplePacket` — сырое напряжение АЦП в милливольтах;
- `CalibrationFinishRequestPacket` и `CalibrationFinishResponsePacket`;
- `CalibrationCancelRequestPacket` и `CalibrationCancelResponsePacket`.

Во всех командах используется `sessionId`. Он защищает устройство от применения
пакета, оставшегося от предыдущей калибровки. Запросы и ответы разделены, потому
что успешная постановка ESP-NOW-пакета в очередь ещё не означает, что приложение
на второй плате обработало его.

### Прошивка датчика

`src/module_soilMoisture/fsm/WakeupState.*` держит датчик активным 30 секунд после
пробуждения кнопкой. Каждые 2 секунды он повторяет `WakeupReportPacket`. Если за
30 секунд команда запуска не пришла, датчик возвращается в штатный режим.

`src/module_soilMoisture/fsm/CalibrationState.*`:

- включает постоянное питание физического датчика;
- подтверждает запуск сессии;
- раз в секунду читает АЦП и отправляет `CalibrationSamplePacket`;
- при завершении проверяет точки и сохраняет их в NVS через `AppConfig`;
- при отмене ничего не сохраняет;
- через 5 минут без завершения автоматически выходит в штатный режим.

После `finish`/`cancel` состояние ещё 2 секунды отвечает на повторные пакеты.
Это нужно на случай потери первого подтверждения ESP-NOW.

`src/module_soilMoisture/controllers/TelemetryController.*` теперь использует
настоящий `SoilMoistureSensor`, а не константу `52`. Контроллер также предоставляет
операции для чтения сырого напряжения и управления непрерывным режимом.

`src/module_soilMoisture/main.cpp` создаёт `SoilMoistureSensor` и при загрузке
передаёт ему сохранённые точки калибровки.

Пины находятся в `src/module_soilMoisture/Config.h`:

```cpp
SENSOR_POWER_PIN = 10;
SENSOR_ANALOG_PIN = 1;
```

Если датчик подключён иначе, менять нужно только эти две константы.

### Прошивка хоста

`src/host/controllers/CalibrationController.h` хранит сессии и выполняет всю
предметную логику:

- запоминает окно пробуждения;
- запускает не более одной сессии на устройство;
- принимает и буферизует последние 5 измерений;
- фиксирует медиану буфера как точку `dry` или `wet`;
- повторяет управляющие пакеты до 5 раз с интервалом 750 мс;
- после подтверждения записывает калибровку в реестр хоста и NVS;
- обнаруживает пропадание потока измерений.

`src/host/commands/CalibrationCommand.h` переводит JSON или CLI-команду в вызов
`CalibrationController`. В ней нет работы с ESP-NOW или NVS — только разбор полей.

`src/host/main.cpp` создаёт контроллер, подписывает его на пакеты, регистрирует
команду `calibration` и вызывает `update()` в основном цикле.

## Живой поток для будущего UI

Каждый принятый `CalibrationSamplePacket` публикуется во внутреннюю шину событий
как `EventType::CALIBRATION_SAMPLE`. `WebAdapter` пересылает его браузеру через
уже существующий SSE-endpoint:

```text
GET /api/events
event: calibration_sample
```

Формат SSE-данных:

```json
{
  "payload": {
    "mac": "AA:BB:CC:DD:EE:FF",
    "sessionId": 123456,
    "sequence": 17,
    "metricIndex": 0,
    "rawMilliVolts": 3021
  }
}
```

Пример подписки будущего интерфейса:

```javascript
const events = new EventSource('/api/events');

events.addEventListener('calibration_sample', event => {
  const { payload } = JSON.parse(event.data);

  if (payload.mac !== selectedSensorMac) return;
  if (payload.sessionId !== activeCalibrationSessionId) return;

  currentRawValue.textContent = `${payload.rawMilliVolts} мВ`;
});
```

Интерфейс использует событие только для отображения. При нажатии кнопки
«Зафиксировать» он должен отправлять `calibration capture` без значения АЦП.
Точку выбирает хост как медиану собственного буфера. Это предотвращает сохранение
устаревшего значения, которое могло задержаться в браузере.

## Порядок калибровки через HTTP

Во всех примерах замените MAC-адрес на адрес своего датчика.

### 1. Разбудить датчик

Нажмите физическую кнопку. После этого есть 30 секунд для команды `start`.

### 2. Запустить поток

```bash
curl -X POST http://192.168.4.1/api/command \
  -H 'Content-Type: application/json' \
  -d '{
    "cmd": "calibration",
    "payload": {
      "action": "start",
      "mac": "AA:BB:CC:DD:EE:FF",
      "metricIndex": 0
    }
  }'
```

Сначала состояние будет `starting`, после ответа датчика — `streaming`.

### 3. Посмотреть текущее значение

```bash
curl -X POST http://192.168.4.1/api/command \
  -H 'Content-Type: application/json' \
  -d '{
    "cmd": "calibration",
    "payload": {
      "action": "status",
      "mac": "AA:BB:CC:DD:EE:FF"
    }
  }'
```

Пример данных ответа:

```json
{
  "success": true,
  "message": "OK",
  "data": {
    "calibration": {
      "sessionId": 123456,
      "state": "streaming",
      "metricIndex": 0,
      "startWindowOpen": true,
      "latestRawMilliVolts": 3021,
      "sampleAgeMs": 184,
      "sampleFresh": true,
      "dryMilliVolts": null,
      "wetMilliVolts": null
    }
  }
}
```

### 4. Зафиксировать сухую точку

Поместите датчик в состояние, соответствующее 0% влажности, дождитесь нескольких
измерений и отправьте:

```bash
curl -X POST http://192.168.4.1/api/command \
  -H 'Content-Type: application/json' \
  -d '{
    "cmd": "calibration",
    "payload": {
      "action": "capture",
      "mac": "AA:BB:CC:DD:EE:FF",
      "point": "dry"
    }
  }'
```

Фиксируется медиана последних пяти значений, а не одно случайное измерение.

### 5. Зафиксировать влажную точку

```bash
curl -X POST http://192.168.4.1/api/command \
  -H 'Content-Type: application/json' \
  -d '{
    "cmd": "calibration",
    "payload": {
      "action": "capture",
      "mac": "AA:BB:CC:DD:EE:FF",
      "point": "wet"
    }
  }'
```

Используются названия `dry` и `wet`, а не `lower` и `upper`: у ёмкостного датчика
сухая точка часто численно больше влажной.

### 6. Сохранить и закончить

```bash
curl -X POST http://192.168.4.1/api/command \
  -H 'Content-Type: application/json' \
  -d '{
    "cmd": "calibration",
    "payload": {
      "action": "finish",
      "mac": "AA:BB:CC:DD:EE:FF"
    }
  }'
```

Дождитесь состояния `completed` через запрос `status`. Только это состояние
означает, что датчик подтвердил запись и хост сохранил свою копию.

### Отмена без сохранения

```bash
curl -X POST http://192.168.4.1/api/command \
  -H 'Content-Type: application/json' \
  -d '{
    "cmd": "calibration",
    "payload": {
      "action": "cancel",
      "mac": "AA:BB:CC:DD:EE:FF"
    }
  }'
```

## Проверки и ограничения

- `capture` разрешён только при состоянии `streaming` и свежем измерении;
- измерение считается устаревшим через 2,5 секунды;
- `finish` требует обе точки;
- точки должны отличаться минимум на 100 мВ;
- допустимый диапазон АЦП: 0–4095 мВ;
- одновременно поддерживается одна активная сессия на один датчик;
- HTTP-сервер сейчас всегда отвечает кодом 200, поэтому результат операции нужно
  проверять по JSON-полю `success`.

## CLI

Та же команда доступна через Serial CLI:

```text
calibration start AA:BB:CC:DD:EE:FF 0
calibration status AA:BB:CC:DD:EE:FF
calibration capture AA:BB:CC:DD:EE:FF dry
calibration capture AA:BB:CC:DD:EE:FF wet
calibration finish AA:BB:CC:DD:EE:FF
calibration cancel AA:BB:CC:DD:EE:FF
```

## Что проверять при проблемах

1. `start` сообщает, что датчик не пробуждён — нажать кнопку и повторить запрос.
2. Состояние осталось `starting` — проверить MAC, канал ESP-NOW и питание датчика.
3. `sampleFresh: false` — датчик перестал передавать поток либо пакеты теряются.
4. `finish` отклонён — проверить наличие обеих точек и разницу не менее 100 мВ.
5. Значения всегда одинаковые или равны нулю — проверить пины 10/1 и питание
   физического датчика.
