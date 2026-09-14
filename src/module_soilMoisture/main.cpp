#include <StorageManager.h>
#include <EspNowManager.h>
#include <Logger.h>
#include <Led.h>
#include "fsm/DeviceContext.h"
#include "fsm/StartState.h"
#include "Config.h"
#include "AppConfig.h"
#include "controllers/SleepController.h"
#include "hardware/SoilMoistureSensor.h"
#include <Button2.h>

StorageManager storage("sensor_cfg");
EspNowManager network(WIFI_STA);
Led led(Config::Pins::LED_PIN, true);
Button2 button;

SleepController sleepController(Config::Pins::BUTTON_PIN);
SoilMoistureSensor soilSensor(
    Config::Pins::SENSOR_POWER_PIN,
    Config::Pins::SENSOR_ANALOG_PIN);
TelemetryController telemetryController(network, soilSensor);
DeviceContext device(network, storage, led, button, sleepController, telemetryController);

void setup()
{
    Logger::begin();
    AppConfig::begin();

    const auto &calibration = AppConfig::interface().metrics[0].calib;
    if (calibration.pointsCount == 2)
    {
        telemetryController.setCalibration(
            static_cast<int>(calibration.rawValues[0]),
            static_cast<int>(calibration.rawValues[1]));
    }

    led.begin();
    button.begin(Config::Pins::BUTTON_PIN);
    button.setLongClickTime(5000);

    network.begin();
    network.setWifiPower(AppConfig::wifiPower());

    sleepController.wakeUp(device);
}

void loop()
{
    led.update();
    device.update();
    vTaskDelay(pdMS_TO_TICKS(10));
}
