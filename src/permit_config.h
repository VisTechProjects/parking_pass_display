#ifndef PERMIT_CONFIG_H
#define PERMIT_CONFIG_H

// ========== SCREEN SETTINGS ==========
const int SCREEN_W = 296;
const int SCREEN_H = 128;

// ========== PERMIT DATA ==========
const char *PERMIT_NUMBER = "T6103268";
const char *PLATE_NUMBER = "CSEB187";
const char *VALID_FROM = "Sep 05, 2025: 01:08";
const char *VALID_TO = "Sep 12, 2025: 01:08";
const char *BARCODE_VALUE = "6103268";
const char *BARCODE_LABEL = "00435";

// ========== TEXT POSITIONING ==========
const int PERMIT_X = 150;
const int PERMIT_Y = 12;
const int PLATE_Y_OFFSET = 17;        // Offset from PERMIT_Y
const int VALID_FROM_Y_OFFSET = 28;   // Offset from PLATE_Y
const int VALID_TO_Y_OFFSET = 17;     // Offset from VALID_FROM_Y

// ========== BARCODE SETTINGS ==========
const int BARCODE_X = 0;
const int BARCODE_Y = 0;
const int BARCODE_HEIGHT = 52;
const int NARROW_BAR_WIDTH = 1;
const int BARCODE_LABEL_Y_OFFSET = 22;  // Offset below barcode

// ========== LOGO SETTINGS ==========
const int LOGO_Y_OFFSET = 27;  // Offset below barcode label

// ========== TEMPORARY PARKING TEXT SETTINGS ==========
const int TEMP_PARKING_X_OFFSET = 10;   // Offset from logo right edge
const int TEMP_PARKING_Y1_OFFSET = 28;  // First line Y offset from logo top
const int TEMP_PARKING_Y2_OFFSET = 16;  // Second line offset from first line

// ========== SEPARATOR LINE SETTINGS ==========
const int HORIZONTAL_LINE_Y_OFFSET = 8;  // Offset below plate text

#endif