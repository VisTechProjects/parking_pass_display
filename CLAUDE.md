# Parking Pass Display - ESP32 E-Ink

## Build/Upload Commands

### Standard ESP32 (testing, no display)
```bash
pio run -e esp32dev --target upload
pio device monitor
```
- Uses `test_main.cpp` (serial output only, no display hardware)
- Button: GPIO 0 (BOOT button)
- LED: GPIO 2

### Heltec Vision Master E290 (production)
```bash
pio run -e vision_e290 --target upload
pio device monitor
```
- Uses `main.cpp` with e-ink display
- Button: GPIO 21
- LED: GPIO 45
- Requires 3s USB CDC delay for serial

## Button Controls
- Short press: BLE sync (only updates if permit changed)
- Long press (3s): Force update display

## BLE Sync
- ESP32 acts as BLE client
- Scans for Android phone running ParkingPermitSync app
- Service UUID: `12345678-1234-5678-1234-56789abcdef0`
- Permit Characteristic: `12345678-1234-5678-1234-56789abcdef1`

## Branch
- `ble-only` - BLE-only version (no WiFi)
- `bluetooth-integration` - Original with both WiFi and BLE
