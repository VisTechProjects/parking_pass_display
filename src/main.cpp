// Parking Permit Display - BLE Only Version
#include <Arduino.h>
#include "heltec-eink-modules.h"
#include <Preferences.h>

#include "Fonts/FreeSansBold8pt7b.h"
#include "Fonts/FreeSansBold13pt7b.h"

#include "Code39Generator.h"
#include "imgs/toronto_logo.h"
#include "permit_config.h"
#include "bluetooth_helper.h"

// Create display pointer locally
EInkDisplay_VisionMasterE290 *display = nullptr;

const int LED_PIN = 45;
const int BUTTON_PIN = 21; // User button on Heltec Vision Master E290

// Preferences for storing permit data
Preferences preferences;

// Global permit data
PermitData currentPermit;

void displayPermit(const char *permitNumber, const char *plateNumber,
                   const char *validFrom, const char *validTo,
                   const char *barcodeValue, const char *barcodeLabel)
{
  display->clearMemory();
  display->setFont((GFXfont *)&FreeSansBold8pt7b);
  display->setTextSize(1);

  const int PLATE_X = PERMIT_X;
  const int PLATE_Y = PERMIT_Y + PLATE_Y_OFFSET;
  const int VALID_FROM_X = PERMIT_X;
  const int VALID_FROM_Y = PLATE_Y + VALID_FROM_Y_OFFSET;
  const int VALID_TO_X = PERMIT_X;
  const int VALID_TO_Y = VALID_FROM_Y + VALID_TO_Y_OFFSET;

  char permit_no[40];
  char plate_no[40];
  sprintf(permit_no, "Permit #: %s", permitNumber);
  sprintf(plate_no, "Plate #: %s", plateNumber);

  display->setCursor(PERMIT_X, PERMIT_Y);
  display->print(permit_no);

  display->setCursor(PLATE_X, PLATE_Y);
  display->print(plate_no);

  int lineY = PLATE_Y + HORIZONTAL_LINE_Y_OFFSET;
  display->drawLine(PERMIT_X, lineY, SCREEN_W - 5, lineY, 0x0000);

  display->setCursor(VALID_FROM_X, VALID_FROM_Y);
  display->print(validFrom);

  display->setCursor(VALID_TO_X, VALID_TO_Y);
  display->print(validTo);

  Code39Generator barcodeGen(display);
  barcodeGen.drawBarcode(barcodeValue, BARCODE_X, BARCODE_Y, BARCODE_HEIGHT, NARROW_BAR_WIDTH);

  int barcodePixelWidth = barcodeGen.getBarcodeWidth(barcodeValue, NARROW_BAR_WIDTH);
  int16_t x3, y3;
  uint16_t w, h;
  display->setFont(&FreeSansBold13pt7b);
  display->getTextBounds(barcodeLabel, 0, 0, &x3, &y3, &w, &h);
  int labelX = BARCODE_X + (barcodePixelWidth / 2) - (w / 2);
  display->setCursor(labelX, BARCODE_Y + BARCODE_HEIGHT + BARCODE_LABEL_Y_OFFSET);
  display->print(barcodeLabel);

  int logoX = BARCODE_X + (barcodePixelWidth / 2) - (LOGO_WIDTH / 2);
  int logoY = BARCODE_Y + BARCODE_HEIGHT + LOGO_Y_OFFSET;

  display->fillRect(logoX, logoY, LOGO_WIDTH, LOGO_HEIGHT, 0x0000);
  display->drawBitmap(logoX, logoY, logo_toronto, LOGO_WIDTH, LOGO_HEIGHT, 0xFFFF);

  const char *permitText1 = "Temporary parking";
  const char *permitText2 = "permit";
  int permitTextX = logoX + LOGO_WIDTH + TEMP_PARKING_X_OFFSET;
  int permitTextY1 = logoY + TEMP_PARKING_Y1_OFFSET;
  int permitTextY2 = permitTextY1 + TEMP_PARKING_Y2_OFFSET;
  display->setFont(&FreeSansBold8pt7b);
  display->setTextSize(1);
  display->setCursor(permitTextX, permitTextY1);
  display->print(permitText1);
  display->setCursor(permitTextX, permitTextY2);
  display->print(permitText2);

  display->update();
}

void displayMessage(const char *message, int textSize = 1)
{
  display->clearMemory();
  display->setFont(&FreeSansBold8pt7b);
  display->setTextSize(textSize);

  int16_t x1, y1;
  uint16_t w, h;
  display->getTextBounds((char *)message, 0, 0, &x1, &y1, &w, &h);

  int x = (SCREEN_W - w) / 2;
  int y = (SCREEN_H + h) / 2;

  display->setCursor(x, y);
  display->print(message);
  display->update();
}

bool displayInit()
{
  if (!display)
  {
    display = new EInkDisplay_VisionMasterE290();
    if (!display)
    {
      Serial.println("Library constructor failed, trying explicit fallback...");
      display = new DEPG0290BNS800(4, 3, 6);
      return false;
    }
  }
  display->landscape();
  return true;
}

// Save permit data to flash
void savePermitData(PermitData *data)
{
  preferences.begin("permit", false);
  preferences.putString("permitNum", data->permitNumber);
  preferences.putString("plateNum", data->plateNumber);
  preferences.putString("validFrom", data->validFrom);
  preferences.putString("validTo", data->validTo);
  preferences.putString("barcode", data->barcodeValue);
  preferences.putString("barLabel", data->barcodeLabel);
  preferences.end();
  Serial.println("Permit data saved to flash");
}

// Load permit data from flash
bool loadPermitData(PermitData *data)
{
  preferences.begin("permit", true);
  String permitNum = preferences.getString("permitNum", "");
  if (permitNum.length() == 0)
  {
    preferences.end();
    return false;
  }

  strncpy(data->permitNumber, permitNum.c_str(), sizeof(data->permitNumber) - 1);
  strncpy(data->plateNumber, preferences.getString("plateNum", "").c_str(), sizeof(data->plateNumber) - 1);
  strncpy(data->validFrom, preferences.getString("validFrom", "").c_str(), sizeof(data->validFrom) - 1);
  strncpy(data->validTo, preferences.getString("validTo", "").c_str(), sizeof(data->validTo) - 1);
  strncpy(data->barcodeValue, preferences.getString("barcode", "").c_str(), sizeof(data->barcodeValue) - 1);
  strncpy(data->barcodeLabel, preferences.getString("barLabel", "").c_str(), sizeof(data->barcodeLabel) - 1);
  preferences.end();

  Serial.print("Loaded permit from flash: ");
  Serial.println(data->permitNumber);
  return true;
}

// Sync permit via Bluetooth
// silent = true means don't update display unless permit changed (for boot sync)
void syncViaBluetooth(bool forceUpdate = false, bool silent = false)
{
  Serial.println("\n=== Bluetooth Sync ===");

  // Determine sync type for phone notification
  uint8_t syncType;
  if (forceUpdate)
  {
    syncType = SYNC_TYPE_FORCE;
    Serial.println("FORCE UPDATE - Will update display regardless of permit number");
    displayMessage("Force syncing...", 1);
  }
  else if (silent)
  {
    syncType = SYNC_TYPE_AUTO;
    Serial.println("Silent sync mode (auto)");
  }
  else
  {
    syncType = SYNC_TYPE_MANUAL;
    Serial.println("Normal sync (manual)");
    displayMessage("Syncing...", 1);
  }

  if (!scanForPhone())
  {
    if (!silent)
    {
      displayMessage("Phone not found", 1);
      delay(2000);
      // Redisplay current permit if we have one
      if (strlen(currentPermit.permitNumber) > 0)
      {
        displayPermit(currentPermit.permitNumber, currentPermit.plateNumber,
                      currentPermit.validFrom, currentPermit.validTo,
                      currentPermit.barcodeValue, currentPermit.barcodeLabel);
      }
    }
    else
    {
      Serial.println("Phone not found - keeping current display");
    }
    cleanupBluetooth();
    return;
  }

  PermitData newPermit;
  int result = 0;

  // Retry logic for connection failures
  for (int attempt = 1; attempt <= BLE_MAX_RETRIES; attempt++)
  {
    result = downloadPermitViaBluetooth(&newPermit, currentPermit.permitNumber, syncType);
    if (result != 0)
    {
      break; // Success or already up to date
    }
    if (attempt < BLE_MAX_RETRIES)
    {
      Serial.printf("Retry %d/%d...\n", attempt, BLE_MAX_RETRIES);
      delay(500);
    }
  }

  if (result == 1 || (result == 2 && forceUpdate))
  {
    // New permit received or force update
    Serial.println("Permit received!");

    currentPermit = newPermit;
    savePermitData(&currentPermit);

    displayPermit(currentPermit.permitNumber, currentPermit.plateNumber,
                  currentPermit.validFrom, currentPermit.validTo,
                  currentPermit.barcodeValue, currentPermit.barcodeLabel);

    Serial.println("Display updated!");
  }
  else if (result == 2)
  {
    // Already up to date - restore permit display if we showed "Syncing..."
    Serial.println("Already up to date");
    if (!silent && strlen(currentPermit.permitNumber) > 0)
    {
      displayPermit(currentPermit.permitNumber, currentPermit.plateNumber,
                    currentPermit.validFrom, currentPermit.validTo,
                    currentPermit.barcodeValue, currentPermit.barcodeLabel);
    }
    delay(100);  // Let BLE fully cleanup before continuing
  }
  else
  {
    // Error
    Serial.println("Sync failed");
    if (!silent)
    {
      displayMessage("Sync failed", 1);
      delay(2000);

      if (strlen(currentPermit.permitNumber) > 0)
      {
        displayPermit(currentPermit.permitNumber, currentPermit.plateNumber,
                      currentPermit.validFrom, currentPermit.validTo,
                      currentPermit.barcodeValue, currentPermit.barcodeLabel);
      }
    }
    else
    {
      Serial.println("Keeping current display");
    }
  }

  cleanupBluetooth();
}

void setup()
{
  // Early pin setup before Serial
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // LED on immediately

  Serial.begin(115200);

  // Longer delay for USB CDC
  for (int i = 0; i < 30; i++) {
    delay(100);
    Serial.print("."); // Try to get serial working
  }
  Serial.println();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("\n=== Parking Permit Display (BLE) ===");
  Serial.println("Initializing display...");
  Serial.flush();

  if (!displayInit())
  {
    Serial.println("Display initialization failed!");
    while (1)
      ;
  }
  Serial.println("Display ready.");

  // Load saved permit data
  bool hasSavedData = loadPermitData(&currentPermit);

  if (!hasSavedData)
  {
    displayMessage("No permit data\nPress button to sync", 1);
    Serial.println("No saved permit. Press button to sync via Bluetooth.");
    strcpy(currentPermit.permitNumber, "");
  }
  else
  {
    Serial.println("Permit loaded from flash.");
    // E-ink retains image, no need to redraw unless data changed
  }

  Serial.println("\nReady!");
  Serial.println("Short press (BOOT): Sync via Bluetooth");
  Serial.println("Long press (3s): Force update");

  // Auto-sync on boot (silent if we already have a permit displayed)
  Serial.println("\nAuto-syncing on boot...");
  bool silentSync = (strlen(currentPermit.permitNumber) > 0);
  syncViaBluetooth(false, silentSync);

  // Start BLE server to listen for commands from phone
  startBleServer();
}

// Helper to perform sync (stops server, syncs, restarts server)
void doSync(bool forceUpdate)
{
  stopBleServer();
  syncViaBluetooth(forceUpdate);
  startBleServer();
}

void loop()
{
  // Check for commands from phone
  int cmd = getPendingCommand();
  if (cmd == 1)
  {
    Serial.println("Sync command received from phone");
    doSync(false);
  }
  else if (cmd == 2)
  {
    Serial.println("Force sync command received from phone");
    doSync(true);
  }

  // Check if button is pressed (LOW = pressed due to pullup)
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW)
    {
      unsigned long pressStart = millis();
      bool longPressTriggered = false;

      // Wait for button release or 3 second timeout
      while (digitalRead(BUTTON_PIN) == LOW)
      {
        if (!longPressTriggered && (millis() - pressStart) >= 3000)
        {
          Serial.println("Long press detected - Force update!");
          longPressTriggered = true;
          doSync(true); // Force update
          break;
        }
        delay(10);
      }

      // Wait for button release
      while (digitalRead(BUTTON_PIN) == LOW)
      {
        delay(10);
      }

      // Short press - normal sync
      if (!longPressTriggered)
      {
        Serial.println("Short press detected - Normal sync");
        doSync(false);
      }
    }
  }

  delay(10);
}
