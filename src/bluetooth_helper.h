#ifndef BLUETOOTH_HELPER_H
#define BLUETOOTH_HELPER_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

// UUIDs for ESP32 as client (connecting to phone to get permit)
#define BLE_SERVICE_UUID "0000ff00-0000-1000-8000-00805f9b34fb"
#define BLE_PERMIT_CHAR_UUID "0000ff01-0000-1000-8000-00805f9b34fb"
#define BLE_SYNC_TYPE_CHAR_UUID "0000ff02-0000-1000-8000-00805f9b34fb"

// Sync types - written to phone before reading permit
#define SYNC_TYPE_AUTO 1    // Reboot/auto sync - no notification if same permit
#define SYNC_TYPE_MANUAL 2  // Button press - always show notification
#define SYNC_TYPE_FORCE 3   // Long press - always show notification

// UUIDs for ESP32 as server (receiving commands from phone)
#define BLE_DISPLAY_SERVICE_UUID "0000ff10-0000-1000-8000-00805f9b34fb"
#define BLE_COMMAND_CHAR_UUID "0000ff11-0000-1000-8000-00805f9b34fb"

// Commands from phone
#define CMD_SYNC "SYNC"
#define CMD_FORCE "FORCE"

// Scan settings
#define BLE_SCAN_TIME 10       // seconds to scan for phone
#define BLE_CONNECT_TIMEOUT 10 // seconds to wait for connection
#define BLE_MAX_RETRIES 2      // number of connection retries

// Command received flag (checked in main loop)
static volatile int pendingCommand = 0;  // 0=none, 1=sync, 2=force

// Permit data structure
struct PermitData {
  char permitNumber[20];
  char plateNumber[20];
  char validFrom[30];
  char validTo[30];
  char barcodeValue[20];
  char barcodeLabel[20];
};

static BLEClient *bleClient = nullptr;
static BLEAdvertisedDevice *targetDevice = nullptr;
static bool deviceFound = false;

class PermitScanCallback : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        // Debug: print all devices found
        Serial.print("  Found: ");
        if (advertisedDevice.haveName()) {
            Serial.print(advertisedDevice.getName().c_str());
        } else {
            Serial.print(advertisedDevice.getAddress().toString().c_str());
        }
        if (advertisedDevice.haveServiceUUID()) {
            Serial.print(" UUID: ");
            Serial.print(advertisedDevice.getServiceUUID().toString().c_str());
        }
        Serial.println();

        if (advertisedDevice.haveServiceUUID() &&
            advertisedDevice.isAdvertisingService(BLEUUID(BLE_SERVICE_UUID)))
        {
            Serial.println("Found Parking Permit Sync phone!");

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
        Serial.println("Phone not found in range");
        BLEDevice::deinit(false);
        return false;
    }

    return true;
}

// Connect to phone and read permit data
// Returns: 0 = error, 1 = updated, 2 = already up to date
// syncType: 1=auto, 2=manual, 3=force
int downloadPermitViaBluetooth(PermitData *data, const char *currentPermitNumber, uint8_t syncType = SYNC_TYPE_AUTO)
{
    if (!targetDevice)
    {
        Serial.println("No device to connect to");
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
        Serial.println("Connection failed");
        delete bleClient;
        bleClient = nullptr;
        BLEDevice::deinit(false);
        return 0;
    }

    Serial.println("Connected!");

    // Get the permit service
    BLERemoteService *service = bleClient->getService(BLEUUID(BLE_SERVICE_UUID));
    if (!service)
    {
        Serial.println("Service not found");
        bleClient->disconnect();
        delete bleClient;
        bleClient = nullptr;
        BLEDevice::deinit(false);
        return 0;
    }

    // Write sync type before reading permit (so phone knows what kind of sync this is)
    BLERemoteCharacteristic *syncTypeChar = service->getCharacteristic(BLEUUID(BLE_SYNC_TYPE_CHAR_UUID));
    if (syncTypeChar)
    {
        Serial.print("Writing sync type: ");
        Serial.println(syncType);
        syncTypeChar->writeValue(&syncType, 1, false);  // false = no response needed
        delay(50);  // Let write complete
    }
    else
    {
        Serial.println("Sync type characteristic not found (old app version?)");
    }

    // Get the permit characteristic
    BLERemoteCharacteristic *permitChar = service->getCharacteristic(BLEUUID(BLE_PERMIT_CHAR_UUID));
    if (!permitChar)
    {
        Serial.println("Characteristic not found");
        bleClient->disconnect();
        delete bleClient;
        bleClient = nullptr;
        BLEDevice::deinit(false);
        return 0;
    }

    // Read the permit JSON
    std::string permitJsonStd = permitChar->readValue();
    String permitJson = permitJsonStd.c_str();

    Serial.println("Received permit data:");
    Serial.println(permitJson.c_str());

    // Disconnect first before any other operations
    bleClient->disconnect();
    delay(50);  // Let disconnect complete
    delete bleClient;
    bleClient = nullptr;
    BLEDevice::deinit(false);
    delay(100);  // Let BLE deinit fully complete before any Serial operations

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, permitJson);

    if (error)
    {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return 0;
    }

    // Check if permit number exists
    if (!doc["permitNumber"].is<const char *>())
    {
        Serial.println("No permit number in response");
        return 0;
    }

    const char *newPermitNumber = doc["permitNumber"];

    // Check for empty permit
    if (strlen(newPermitNumber) == 0)
    {
        Serial.println("Empty permit received (phone may not have synced yet)");
        return 0;
    }

    // Validate required fields exist
    const char *plateNum = doc["plateNumber"] | "";
    const char *validFrom = doc["validFrom"] | "";
    const char *validTo = doc["validTo"] | "";

    if (strlen(plateNum) == 0 || strlen(validFrom) == 0 || strlen(validTo) == 0)
    {
        Serial.println("ERROR: Incomplete permit data - missing required fields");
        return 0;
    }

    // Copy permit data - zero out struct first to ensure null termination
    memset(data, 0, sizeof(PermitData));
    strncpy(data->permitNumber, newPermitNumber, sizeof(data->permitNumber) - 1);
    strncpy(data->plateNumber, plateNum, sizeof(data->plateNumber) - 1);
    strncpy(data->validFrom, validFrom, sizeof(data->validFrom) - 1);
    strncpy(data->validTo, validTo, sizeof(data->validTo) - 1);
    strncpy(data->barcodeValue, doc["barcodeValue"] | "", sizeof(data->barcodeValue) - 1);
    strncpy(data->barcodeLabel, doc["barcodeLabel"] | "", sizeof(data->barcodeLabel) - 1);

    // Check if permit changed (with null safety)
    if (currentPermitNumber != nullptr && strcmp(data->permitNumber, currentPermitNumber) == 0)
    {
        Serial.println("Permit unchanged");
        delay(10);  // Let serial flush
        return 2; // Already up to date (but data is still populated)
    }

    Serial.print("New permit received: ");
    Serial.println(data->permitNumber);

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

// ============ BLE Server (for receiving commands from phone) ============

static BLEServer *bleServer = nullptr;
static BLECharacteristic *commandChar = nullptr;
static bool serverRunning = false;

class CommandCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            Serial.print("Received command: ");
            Serial.println(value.c_str());

            if (value == CMD_SYNC)
            {
                pendingCommand = 1;
            }
            else if (value == CMD_FORCE)
            {
                pendingCommand = 2;
            }
        }
    }
};

class ServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        Serial.println("Phone connected to display");
    }

    void onDisconnect(BLEServer *pServer)
    {
        Serial.println("Phone disconnected from display");
        // Restart advertising
        pServer->startAdvertising();
    }
};

// Start BLE server to listen for commands
void startBleServer()
{
    if (serverRunning)
    {
        Serial.println("BLE server already running");
        return;
    }

    Serial.println("Starting BLE server...");

    BLEDevice::init("ParkingDisplay");

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());

    BLEService *service = bleServer->createService(BLE_DISPLAY_SERVICE_UUID);

    commandChar = service->createCharacteristic(
        BLE_COMMAND_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);

    commandChar->setCallbacks(new CommandCallbacks());

    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_DISPLAY_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    serverRunning = true;
    Serial.println("BLE server started, waiting for commands...");
}

// Stop BLE server (before doing client operations)
void stopBleServer()
{
    if (!serverRunning)
    {
        return;
    }

    Serial.println("Stopping BLE server...");
    BLEDevice::stopAdvertising();
    BLEDevice::deinit(false);
    bleServer = nullptr;
    commandChar = nullptr;
    serverRunning = false;
    delay(100);
}

// Check if there's a pending command from phone
int getPendingCommand()
{
    int cmd = pendingCommand;
    pendingCommand = 0;
    return cmd;
}

#endif
