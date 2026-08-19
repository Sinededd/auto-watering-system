# EspNowManager

A lightweight ESP32 wrapper for ESP-NOW that simplifies peer management, packet sending, and typed callback routing.

## Usage Example
```C++
#include <Arduino.h>
#include "EspNowManager.h"

// Initialize manager (default: WIFI_STA, MAX Power)
EspNowManager net;

// 1. Define your packet (must include PacketHeader as first member)
struct __attribute__((packed)) CustomPacket {
    PacketHeader header = { PacketType::HELLO };
    uint32_t counter;
};

void setup() {
    Serial.begin(115200);

    // 2. Start ESP-NOW
    net.begin();

    // 3. Register peer MAC address
    uint8_t receiverMac[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56};
    net.addPeer(receiverMac);

    // 4. Register a callback for incoming HELLO packets
    net.onPacket<CustomPacket>(PacketType::HELLO, [](const esp_now_recv_info_t *info, const CustomPacket &packet) {
        Serial.printf("Got packet! Counter: %u\n", packet.counter);
    });

    // 5. Send a packet
    CustomPacket pkt;
    pkt.counter = 42;
    net.send(receiverMac, pkt);
}

void loop() {
    // Non-blocking processing handled via callbacks
}
```