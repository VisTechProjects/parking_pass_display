// Parking Permit Display
#include <Arduino.h>
#include "heltec-eink-modules.h"

#include "Fonts/FreeSansBold8pt7b.h"
#include "Fonts/FreeSansBold13pt7b.h"

#include "Code39Generator.h"
#include "imgs/toronto_logo.h"
#include "permit_config.h"
#include "wifi_helper.h"
#include "bluetooth_helper.h"

// Create display pointer locally (not extern)
EInkDisplay_VisionMasterE290 *display = nullptr;

const int LED_PIN = 45;

// Global permit data
PermitData currentPermit;

void displayPermit(const char *permitNumber, const char *plateNumber,
                   const char *validFrom, const char *validTo,
                   const char *barcodeValue, const char *barcodeLabel)
{
  // clear display memory / framebuffer
  display->clearMemory();

  // choose font/size
  display->setFont((GFXfont *)&FreeSansBold8pt7b);
  display->setTextSize(1);

  // ========== CALCULATE TEXT POSITIONS ==========
  const int PLATE_X = PERMIT_X;
  const int PLATE_Y = PERMIT_Y + PLATE_Y_OFFSET;

  const int VALID_FROM_X = PERMIT_X;
  const int VALID_FROM_Y = PLATE_Y + VALID_FROM_Y_OFFSET;

  const int VALID_TO_X = PERMIT_X;
  const int VALID_TO_Y = VALID_FROM_Y + VALID_TO_Y_OFFSET;

  // Build permit strings (FIXED: increased buffer size to prevent overflow)
  char permit_no[40];  // "Permit #: " (10) + permitNumber (20) + null (1) = 31 minimum
  char plate_no[40];   // "Plate #: " (9) + plateNumber (20) + null (1) = 30 minimum
  sprintf(permit_no, "Permit #: %s", permitNumber);
  sprintf(plate_no, "Plate #: %s", plateNumber);

  display->setCursor(PERMIT_X, PERMIT_Y);
  display->print(permit_no);

  display->setCursor(PLATE_X, PLATE_Y);
  display->print(plate_no);

  // Draw horizontal line separator between permit/plate and dates
  int lineY = PLATE_Y + HORIZONTAL_LINE_Y_OFFSET;
  display->drawLine(PERMIT_X, lineY, SCREEN_W - 5, lineY, 0x0000);

  display->setCursor(VALID_FROM_X, VALID_FROM_Y);
  display->print(validFrom);

  display->setCursor(VALID_TO_X, VALID_TO_Y);
  display->print(validTo);

  // ========== DRAW BARCODE ==========
  Code39Generator barcodeGen(display);
  barcodeGen.drawBarcode(barcodeValue, BARCODE_X, BARCODE_Y, BARCODE_HEIGHT, NARROW_BAR_WIDTH);

  // Draw the human-readable label below the barcode (centered)
  int barcodePixelWidth = barcodeGen.getBarcodeWidth(barcodeValue, NARROW_BAR_WIDTH);
  int16_t x3, y3;
  uint16_t w, h;
  display->setFont(&FreeSansBold13pt7b);
  display->getTextBounds(barcodeLabel, 0, 0, &x3, &y3, &w, &h);
  int labelX = BARCODE_X + (barcodePixelWidth / 2) - (w / 2);
  display->setCursor(labelX, BARCODE_Y + BARCODE_HEIGHT + BARCODE_LABEL_Y_OFFSET);
  display->print(barcodeLabel);

  // ========== DRAW LOGO ==========
  int logoX = BARCODE_X + (barcodePixelWidth / 2) - (LOGO_WIDTH / 2);
  int logoY = BARCODE_Y + BARCODE_HEIGHT + LOGO_Y_OFFSET;

  // Invert image (white logo on black background)
  display->fillRect(logoX, logoY, LOGO_WIDTH, LOGO_HEIGHT, 0x0000);
  display->drawBitmap(logoX, logoY, logo_toronto, LOGO_WIDTH, LOGO_HEIGHT, 0xFFFF);

  // ========== DRAW TEMPORARY PARKING TEXT ==========
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

  // Push to e-ink
  display->update();
}

void displayMessage(const char *message, int textSize = 1)
{
  // Clear display memory
  display->clearMemory();

  // Set font
  display->setFont(&FreeSansBold8pt7b);
  display->setTextSize(textSize);

  // Get text bounds to center the message
  int16_t x1, y1;
  uint16_t w, h;
  display->getTextBounds((char *)message, 0, 0, &x1, &y1, &w, &h);

  // Center the text on screen
  int x = (SCREEN_W - w) / 2;
  int y = (SCREEN_H + h) / 2;

  display->setCursor(x, y);
  display->print(message);

  // Push to e-ink
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

void checkForUpdate(bool forceUpdate = false)
{
  Serial.println("\n=== Manual Update Triggered ===");
  if (forceUpdate)
  {
    Serial.println("FORCE UPDATE - Will download regardless of permit number");
    displayMessage("Force update...", 1);
  }
  else
  {
    displayMessage("Checking for\nupdate...", 1);
  }

  if (connectToWiFi())
  {
    displayMessage("Downloading...", 1);

    PermitData newPermit;
    int result = downloadPermitData(&newPermit, currentPermit.permitNumber, forceUpdate);

    if (result == 1 || (result == 2 && forceUpdate))
    {
      // Updated (or force update even if same)
      Serial.println("Permit downloaded!");
      displayMessage("Updated!", 2);
      delay(1000);

      // Update current permit and display
      currentPermit = newPermit;
      displayPermit(currentPermit.permitNumber, currentPermit.plateNumber,
                    currentPermit.validFrom, currentPermit.validTo,
                    currentPermit.barcodeValue, currentPermit.barcodeLabel);
    }
    else if (result == 2)
    {
      // Already up to date
      Serial.println("Already up to date!");
      displayMessage("Already up\nto date", 1);
      delay(2000);

      // Redisplay current permit
      displayPermit(currentPermit.permitNumber, currentPermit.plateNumber,
                    currentPermit.validFrom, currentPermit.validTo,
                    currentPermit.barcodeValue, currentPermit.barcodeLabel);
    }
    else
    {
      // Error
      Serial.println("Update failed!");
      displayMessage("Update failed", 1);
      delay(2000);

      // Redisplay current permit
      displayPermit(currentPermit.permitNumber, currentPermit.plateNumber,
                    currentPermit.validFrom, currentPermit.validTo,
                    currentPermit.barcodeValue, currentPermit.barcodeLabel);
    }

    disconnectWiFi();
  }
  else
  {
    Serial.println("WiFi not available!");
    displayMessage("No WiFi", 1);
    delay(2000);

    // Redisplay current permit
    displayPermit(currentPermit.permitNumber, currentPermit.plateNumber,
                  currentPermit.validFrom, currentPermit.validTo,
                  currentPermit.barcodeValue, currentPermit.barcodeLabel);
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Setup button with internal pullup
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Attempting to create display instance...");

  if (!displayInit())
  {
    Serial.println("Display initialization failed!");
    while (1)
      ;
  }
  else
  {
    Serial.println("Display instance created.");
  }

  // Try to load saved permit data first
  bool hasSavedData = loadPermitData(&currentPermit);

  // Note: Don't display yet - e-ink retains image when powered off
  // We'll only update display if data changes or if this is first boot

  if (!hasSavedData)
  {
    // No saved data, show message
    displayMessage("No permit data", 1);
    Serial.println("No saved permit data available.");
    strcpy(currentPermit.permitNumber, ""); // Empty permit number
  }
  else
  {
    Serial.println("Saved permit data loaded. Display should already show this data.");
  }

  // Try to connect to WiFi and update
  if (!hasSavedData)
  {
    displayMessage("Checking WiFi...", 1);
  }

  if (connectToWiFi())
  {
    if (!hasSavedData)
    {
      displayMessage("Updating...", 1);
    }

    PermitData newPermit;
    // FIXED: Add explicit false parameter for normal update
    int result = downloadPermitData(&newPermit, currentPermit.permitNumber, false);

    if (result == 1)
    {
      Serial.println("Successfully downloaded new permit data!");
      currentPermit = newPermit;

      // Display the updated permit
      displayPermit(currentPermit.permitNumber, currentPermit.plateNumber,
                    currentPermit.validFrom, currentPermit.validTo,
                    currentPermit.barcodeValue, currentPermit.barcodeLabel);
    }
    else if (result == 2)
    {
      Serial.println("Permit is already current.");
      // Permit already displayed, no action needed
    }
    else
    {
      Serial.println("Failed to download permit data. Using saved data.");
      // If no saved data, show error
      if (!hasSavedData)
      {
        displayMessage("Update failed", 1);
      }
    }

    // Disconnect WiFi to save power
    disconnectWiFi();
  }
  else
  {
    Serial.println("WiFi not available. Using saved data.");
    // If no saved data, show error
    if (!hasSavedData)
    {
      displayMessage("No WiFi", 1);
    }
  }

  Serial.println("Setup complete. Press button to check for updates.");
}

void loop()
{
  // Check if button is pressed (LOW = pressed due to pullup)
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW)
    {
      // Button is pressed - measure how long
      unsigned long pressStart = millis();
      bool longPressTriggered = false;

      // Wait for button release or 3 second timeout
      while (digitalRead(BUTTON_PIN) == LOW)
      {
        if (!longPressTriggered && (millis() - pressStart) >= 3000)
        {
          // Long press threshold reached - trigger immediately
          Serial.println("Long press detected - Force update!");
          longPressTriggered = true;
          checkForUpdate(true); // Force update
          break;                // Exit loop
        }
        delay(10);
      }

      // Wait for button release
      while (digitalRead(BUTTON_PIN) == LOW)
      {
        delay(10);
      }

      // If it was a short press (long press wasn't triggered)
      if (!longPressTriggered)
      {
        Serial.println("Short press detected - Normal update check");
        checkForUpdate(false); // Normal update
      }
    }
  }

  delay(10); // Small delay to reduce CPU usage
}