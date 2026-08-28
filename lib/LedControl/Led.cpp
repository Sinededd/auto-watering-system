#include "Led.h"

Led::Led(uint8_t pin, bool activeLow) : pin(pin), activeLow(activeLow) {}

void Led::begin() {
    pinMode(pin, OUTPUT);
    turnOff();
}

void Led::setRawState(bool state) {
    active = state;
    digitalWrite(pin, activeLow ? !active : active);
}

void Led::turnOn() {
    isBlinking = 0;
    setRawState(true);
}

void Led::turnOff() {
    isBlinking = 0;
    setRawState(false);
}

void Led::toggle() {
    setRawState(!active);
}

void Led::slowBlinking(unsigned int times) {
    isSlowBlinking = true;
    isBlinking = true;
    blinkCount = times * 2;
    lastBlinkTime = millis();
    setRawState(true);
    if (blinkCount > 0) blinkCount--;
}

void Led::fastBlinking(unsigned int times) {
    isSlowBlinking = false;
    isBlinking = true;
    blinkCount = times * 2; 
    lastBlinkTime = millis();
    setRawState(true);
    if (blinkCount > 0) blinkCount--;
}

void Led::update() {
    if (!isBlinking) return;

    unsigned long currentTime = millis();
    uint32_t interval = isSlowBlinking ? 1000 : 200;

    if (currentTime - lastBlinkTime >= interval) {
        lastBlinkTime = currentTime;
        toggle();

        if (blinkCount > 0) {
            blinkCount--;
            if (blinkCount == 0) {
                turnOff();
            }
        }
    }
}

void Led::blinkBlocking(unsigned int times, uint32_t intervalMs) {
    isBlinking = false;

    for (unsigned int i = 0; i < times; ++i) {
        setRawState(true);
        delay(intervalMs);
        setRawState(false);
        delay(intervalMs);
    }
}

void Led::slowBlinkingBlocking(unsigned int times) {
    blinkBlocking(times, 1000);
}

void Led::fastBlinkingBlocking(unsigned int times) {
    blinkBlocking(times, 200);
}