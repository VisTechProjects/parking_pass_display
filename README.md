# Toronto Parking Permit Display

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Display](https://img.shields.io/badge/display-E--Ink%20296x128-green)
![BLE](https://img.shields.io/badge/BLE-Bluetooth%20LE-blue)

ESP32 e-ink display that shows your Toronto temporary parking permit. Syncs wirelessly via Bluetooth from the [ParkingPermitSync](https://github.com/VisTechProjects/ParkingPermitSync) Android app.

## Hardware

- [Heltec Vision Master E290](https://heltec.org/project/vision-master-e290/) (ESP32-S3 + 2.9" e-ink)
- Button on GPIO 21
- LED on GPIO 45

## How It Works

1. Android app ([ParkingPermitSync](https://github.com/VisTechProjects/ParkingPermitSync)) purchases/fetches permit automatically
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

Install [ParkingPermitSync](https://github.com/VisTechProjects/ParkingPermitSync) on your phone.

### 3. Pair

1. Open app on phone
2. Press button on ESP32
3. They'll find each other via BLE

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
- ~0.35% battery/day on Android for background BLE
- Display can be flipped 180° via app setting
