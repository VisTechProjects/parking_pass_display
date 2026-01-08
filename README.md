# Toronto Parking Permit Display

![Platform](https://img.shields.io/badge/platform-ESP32-orange)
![Display](https://img.shields.io/badge/display-E--Ink%20296x128-green)
![BLE](https://img.shields.io/badge/BLE-Bluetooth%20LE-blue)

![Display Photo](docs/display.jpg)

ESP32 e-ink display that shows your Toronto temporary parking permit. Syncs wirelessly via Bluetooth from the [parking-permit-android](https://github.com/VisTechProjects/parking-permit-android) app.

## Hardware

- [Heltec Vision Master E290](https://heltec.org/project/vision-master-e290/) (ESP32-S3 + 2.9" e-ink)
- Button on GPIO 21
- LED on GPIO 45

## How It Works

1. [parking-permit-buyer](https://github.com/VisTechProjects/parking-permit-buyer) script purchases permit and syncs to Android app
2. Press button on ESP32 or trigger from app
3. ESP32 connects to phone via BLE and downloads permit data
4. Display updates with permit info and barcode

## Usage

| Action | Result |
|--------|--------|
| **Short press** | Sync via BLE (only updates if permit changed) |
| **Long press (3s)** | Force update display |
| **From app** | Tap sync button to push to display |

Device auto-syncs on boot if phone is nearby.

## Setup

### 1. Upload Firmware

```bash
pio run -e vision_e290 --target upload
```

### 2. Install Android App

Install [parking-permit-android](https://github.com/VisTechProjects/parking-permit-android) on your phone.

### 3. Sync

1. Open app on phone
2. ESP32 auto-syncs on boot when phone is nearby
3. Button press as backup/manual trigger

## BLE Protocol

- Service UUID: `12345678-1234-5678-1234-56789abcdef0`
- Permit Characteristic: `12345678-1234-5678-1234-56789abcdef1`

ESP32 scans for the Android app's BLE advertisement, connects, and reads permit JSON.

## Files

- `src/main.cpp` - Main firmware
- `src/bluetooth_helper.h` - BLE client/server
- `src/permit_config.h` - Display layout constants
- `src/Code39Generator.h` - Barcode rendering

## Branches

- `main` / `dev` - BLE version (current)
- `wifi-only` - Original WiFi version (pulls from GitHub)
- `permit` - Permit JSON data

## Notes

- Permit data persists in flash memory
- E-ink retains image when powered off
- Display can be flipped 180° via app setting
