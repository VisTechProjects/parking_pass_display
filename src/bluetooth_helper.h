#ifndef BLUETOOTH_HELPER_H
#define BLUETOOTH_HELPER_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <ArduinoJson.h>

// Must match Android app UUIDs
#define BLE_SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define BLE_PERMIT_CHAR_UUID "12345678-1234-5678-1234-56789abcdef1"
#define BLE_STATUS_CHAR_UUID "12345678-1234-5678-1234-56789abcdef2"

// Status messages to send to phone
#define ESP_MSG_SYNC_COMPLETE "SYNC_OK"
#define ESP_MSG_DISPLAY_UPDATED "UPDATED"
#define ESP_MSG_BUTTON_PRESS "BUTTON"

// Scan settings
#define BLE_SCAN_TIME 5        // seconds to scan for phone
#define BLE_CONNECT_TIMEOUT 10 // seconds to wait for connection

// Forward declaration
struct PermitData;

// ANSI colors (should match wifi_helper.h)
#ifndef COLOR_RESET
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_MAGENTA "\033[35m"
#endif

static BLEClient *bleClient = nullptr;
static BLEAdvertisedDevice *targetDevice = nullptr;
static bool deviceFound = false;

class PermitScanCallback : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        if (advertisedDevice.haveServiceUUID() &&
            advertisedDevice.isAdvertisingService(BLEUUID(BLE_SERVICE_UUID)))
        {
            Serial.print(COLOR_GREEN);
            Serial.print("Found Parking Permit Sync phone!");
            Serial.print(COLOR_RESET);
            Serial.println();

            // Store device and stop scan
            targetDevice = new BLEAdvertisedDevice(advertisedDevice);
            deviceFound = true;
            BLEDevice::getScan()->stop();
        }
    }
};

// Scan for the Android phone
bool scanForPhone()
{
    Serial.println("\n=== Bluetooth Scan ===");
    Serial.println("Looking for Parking Permit Sync app...");

    deviceFound = false;
    if (targetDevice)
    {
        delete targetDevice;
        targetDevice = nullptr;
    }

    BLEDevice::init("ParkingDisplay");
    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new PermitScanCallback());
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    scan->start(BLE_SCAN_TIME, false);

    if (!deviceFound)
    {
        Serial.print(COLOR_YELLOW);
        Serial.print("Phone not found in range");
        Serial.print(COLOR_RESET);
        Serial.println();
        BLEDevice::deinit(false);
        return false;
    }

    return true;
}

// Connect to phone and read permit data
// Returns: 0 = error, 1 = updated, 2 = already up to date
int downloadPermitViaBluetooth(PermitData *data, const char *currentPermitNumber)
{
    if (!targetDevice)
    {
        Serial.print(COLOR_RED);
        Serial.print("No device to connect to");
        Serial.print(COLOR_RESET);
        Serial.println();
        return 0;
    }

    Serial.print("Connecting to ");
    Serial.println(targetDevice->getAddress().toString().c_str());

    bleClient = BLEDevice::createClient();

    // Connect with timeout
    unsigned long startTime = millis();
    bool connected = false;

    while (millis() - startTime < BLE_CONNECT_TIMEOUT * 1000)
    {
        if (bleClient->connect(targetDevice))
        {
            connected = true;
            break;
        }
        delay(500);
    }

    if (!connected)
    {
        Serial.print(COLOR_RED);
        Serial.print("Connection failed");
        Serial.print(COLOR_RESET);
        Serial.println();
        delete bleClient;
        bleClient = nullptr;
        BLEDevice::deinit(false);
        return 0;
    }

    Serial.print(COLOR_GREEN);
    Serial.print("Connected!");
    Serial.print(COLOR_RESET);
    Serial.println();

    // Get the permit service
    BLERemoteService *service = bleClient->getService(BLEUUID(BLE_SERVICE_UUID));
    if (!service)
    {
        Serial.print(COLOR_RED);
        Serial.print("Service not found");
        Serial.print(COLOR_RESET);
        Serial.println();
        bleClient->disconnect();
        delete bleClient;
        bleClient = nullptr;
        BLEDevice::deinit(false);
        return 0;
    }

    // Get the permit characteristic
    BLERemoteCharacteristic *permitChar = service->getCharacteristic(BLEUUID(BLE_PERMIT_CHAR_UUID));
    if (!permitChar)
    {
        Serial.print(COLOR_RED);
        Serial.print("Characteristic not found");
        Serial.print(COLOR_RESET);
        Serial.println();
        bleClient->disconnect();
        delete bleClient;
        bleClient = nullptr;
        BLEDevice::deinit(false);
        return 0;
    }

    // Read the permit JSON
    String permitJson = permitChar->readValue();

    Serial.println("Received permit data:");
    Serial.println(permitJson.c_str());

    // Try to get status characteristic and send SYNC_OK
    BLERemoteCharacteristic *statusChar = service->getCharacteristic(BLEUUID(BLE_STATUS_CHAR_UUID));
    if (statusChar && statusChar->canWrite())
    {
        Serial.println("Sending SYNC_OK to phone...");
        statusChar->writeValue(ESP_MSG_SYNC_COMPLETE);
    }

    // Disconnect
    bleClient->disconnect();
    delete bleClient;
    bleClient = nullptr;
    BLEDevice::deinit(false);

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, permitJson);

    if (error)
    {
        Serial.print(COLOR_RED);
        Serial.print("JSON parse error: ");
        Serial.print(error.c_str());
        Serial.print(COLOR_RESET);
        Serial.println();
        return 0;
    }

    // Check if permit number exists
    if (!doc["permitNumber"].is<const char *>())
    {
        Serial.print(COLOR_RED);
        Serial.print("No permit number in response");
        Serial.print(COLOR_RESET);
        Serial.println();
        return 0;
    }

    const char *newPermitNumber = doc["permitNumber"];

    // Check for empty permit
    if (strlen(newPermitNumber) == 0)
    {
        Serial.print(COLOR_YELLOW);
        Serial.print("Empty permit received (phone may not have synced yet)");
        Serial.print(COLOR_RESET);
        Serial.println();
        return 0;
    }

    // Check if permit changed
    if (strcmp(newPermitNumber, currentPermitNumber) == 0)
    {
        Serial.print(COLOR_YELLOW);
        Serial.print("Permit unchanged");
        Serial.print(COLOR_RESET);
        Serial.println();
        return 2; // Already up to date
    }

    // Copy new permit data
    strncpy(data->permitNumber, doc["permitNumber"] | "", sizeof(data->permitNumber) - 1);
    strncpy(data->plateNumber, doc["plateNumber"] | "", sizeof(data->plateNumber) - 1);
    strncpy(data->validFrom, doc["validFrom"] | "", sizeof(data->validFrom) - 1);
    strncpy(data->validTo, doc["validTo"] | "", sizeof(data->validTo) - 1);
    strncpy(data->barcodeValue, doc["barcodeValue"] | "", sizeof(data->barcodeValue) - 1);
    strncpy(data->barcodeLabel, doc["barcodeLabel"] | "", sizeof(data->barcodeLabel) - 1);

    Serial.print(COLOR_GREEN);
    Serial.print("New permit received: ");
    Serial.print(data->permitNumber);
    Serial.print(COLOR_RESET);
    Serial.println();

    return 1; // Updated
}

// Clean up BLE resources
void cleanupBluetooth()
{
    if (targetDevice)
    {
        delete targetDevice;
        targetDevice = nullptr;
    }
    deviceFound = false;
}

#endif
