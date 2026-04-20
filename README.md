# KGUARD — Standalone Boiling Detector

Plug it in. Point it at the pot. Walk away.

No internet. No app. No cloud. Just a camera, an ESP32, and a buzzer.

## How it works

On boot it captures a reference frame of the pot. Every 2 seconds it compares
the new frame against the reference. If enough pixels changed significantly,
the liquid is moving — alarm fires.

## Hardware

- AI Thinker ESP32-CAM
- Passive buzzer on GPIO 13
- Any 5V USB power source

## Tuning

MOTION_THRESHOLD — how much a single pixel must change to count (default 50)
CHANGED_PIXELS_LIMIT — how many such pixels trigger the alarm (default 1800)

## Flash

Arduino IDE — Board: AI Thinker ESP32-CAM — Partition: Huge APP
