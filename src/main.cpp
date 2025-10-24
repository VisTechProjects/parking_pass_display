// demo_font.cpp
#include <Arduino.h>
#include "heltec-eink-modules.h"

// https://rop.nl/truetype2gfx/ extracts fonts to GFX format
// Fonts in src/Fonts/ -- adjust names to match the files you have
#include "Fonts/FreeSansBold8pt7b.h"
#include "Fonts/Org_01.h"
#include "Fonts/FreeSans12pt7b.h"
#include "Fonts/FreeSansBold12pt7b.h"
#include "Fonts/FreeSansBold13pt7b.h"
#include "Fonts/FreeSansBold14pt7b.h"

#include "Code39Generator.h"
#include "imgs/toronto_logo.h"
#include "permit_config.h"

// Create display pointer locally (not extern)
EInkDisplay_VisionMasterE290 *display = nullptr;

const int LED_PIN = 45;

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

  // Build permit strings
  char permit_no[30];
  char plate_no[30];
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

void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Attempting to create display instance...");

  if (!display)
  {
    display = new EInkDisplay_VisionMasterE290();

    if (!display)
    {
      Serial.println("Library constructor failed, trying explicit fallback...");
      display = new DEPG0290BNS800(4, 3, 6);
    }
  }

  Serial.println("Display instance created.");

  display->landscape();
  display->clearMemory();

  Serial.println("Displaying permit...");

  // Call displayPermit with the data from config file
  displayPermit(PERMIT_NUMBER, PLATE_NUMBER, VALID_FROM, VALID_TO, BARCODE_VALUE, BARCODE_LABEL);

  Serial.println("Permit displayed.");
}

void loop()
{
  // nothing - demo ran in setup
}