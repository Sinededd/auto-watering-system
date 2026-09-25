# AutoWateringSys

An automated plant watering system.

## Веб-интерфейс хоста

Страница управления находится в `src/host/data` и работает без подключения к интернету. Для установки нужно загрузить **прошивку и образ LittleFS**:

```sh
pio run -e host -t upload
pio run -e host -t uploadfs
```

Подключитесь к точке доступа `AutoWateringHost` и откройте `http://192.168.4.1/`. Параметры точки доступа заданы в `src/host/Config.h`. Веб-страница отправляет команды через `POST /api/command` и получает события через `GET /api/events`.
