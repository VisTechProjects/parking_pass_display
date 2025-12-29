# Toronto Parking Permit Display

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![PlatformIO](https://img.shields.io/badge/PlatformIO-IDE-orange)
![Display](https://img.shields.io/badge/display-E--Ink%20296x128-green)
![ArduinoJson](https://img.shields.io/badge/ArduinoJson-v7%2B-yellow)
![WiFi](https://img.shields.io/badge/WiFi-Multi--Network-brightgreen)

ESP32 e-ink display that automatically updates your Toronto temporary parking permit from GitHub.

## Hardware

- ESP32 with e-ink display (296x128) [Heltec E290 e-ink display](https://heltec.org/project/vision-master-e290/)
- Button on GPIO 21

## Setup

### 1. Configure WiFi

Edit `wifi_config.h`:
```cpp
const char* WIFI_SSID_1 = "YourWiFi";
const char* WIFI_PASS_1 = "password";
```

### 2. GitHub Setup

Create `permit.json` on `permit` branch:
```json
{
  "permitNumber": "T6146330",
  "plateNumber": "CSEB123",
  "validFrom": "Oct 25, 2025: 12:00",
  "validTo": "Nov 01, 2025: 11:59",
  "barcodeValue": "6146330",
  "barcodeLabel": "00435" // different from barcode value
}
```

Update GitHub URL in `wifi_config.h`:
```cpp
const char* SERVER_URL = "https://raw.githubusercontent.com/YOUR_USER/parking_pass_display/permit/permit.json";
```

### 3. Upload Code

Install libraries:
- Heltec E-Ink Modules
- ArduinoJson v7+

Upload `main.cpp` to ESP32.

## Usage

**Short press** - Check for updates (only downloads if permit number changed)
**Long press (3+ sec)** - Force update (downloads all data, bypasses CDN cache)

Device auto-checks for updates on boot.

### Why Force Update?

Use force update when you:
- Changed permit details (dates, plate) but kept same permit number
- Need to bypass GitHub's CDN cache (updates immediately, no 5-10 min wait)

## Files

- `main.cpp` - Main code
- `wifi_helper.h` - WiFi and download
- `wifi_config.h` - WiFi settings
- `permit_config.h` - Display layout
- `Code39Generator.h` - Barcode

## Notes

- Permit data stored in flash memory (persists after power loss)
- WiFi scan-first optimization (3-5 sec connection)
- E-ink retains image when powered off
- Force update adds cache-busting parameter to bypass GitHub CDN
- Multiple WiFi support with automatic fallback