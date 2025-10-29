#ifndef WIFI_HELPER_H
#define WIFI_HELPER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "wifi_config.h"

// ANSI color codes for serial output (only reliable colors)
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_MAGENTA "\033[35m"

Preferences preferences;

struct PermitData {
  char permitNumber[20];
  char plateNumber[20];
  char validFrom[30];
  char validTo[30];
  char barcodeValue[20];
  char barcodeLabel[20];
};

// Helper function to safely copy JSON string to char array
void safeJsonCopy(char* dest, size_t destSize, JsonDocument& doc, const char* key) {
  const char* value = doc[key] | "";
  strncpy(dest, value, destSize - 1);
  dest[destSize - 1] = '\0';
}

// Load permit data from flash memory
bool loadPermitData(PermitData* data) {
  preferences.begin("permit", true); // true = read-only
  
  bool hasData = preferences.isKey("permitNum");
  
  if (hasData) {
    preferences.getString("permitNum", data->permitNumber, sizeof(data->permitNumber));
    preferences.getString("plateNum", data->plateNumber, sizeof(data->plateNumber));
    preferences.getString("validFrom", data->validFrom, sizeof(data->validFrom));
    preferences.getString("validTo", data->validTo, sizeof(data->validTo));
    preferences.getString("barcodeVal", data->barcodeValue, sizeof(data->barcodeValue));
    preferences.getString("barcodeLabel", data->barcodeLabel, sizeof(data->barcodeLabel));
    
    preferences.end();
    Serial.print(COLOR_GREEN);
    Serial.print("Loaded permit data from flash. Permit #: ");
    Serial.print(data->permitNumber);
    Serial.print(COLOR_RESET);
    Serial.println();
    return true;
  }
  
  preferences.end();
  Serial.print(COLOR_YELLOW);
  Serial.print("No saved permit data found in flash.");
  Serial.print(COLOR_RESET);
  Serial.println();
  return false;
}

// Save permit data to flash memory
void savePermitData(PermitData* data) {
  preferences.begin("permit", false); // false = read/write
  
  preferences.putString("permitNum", data->permitNumber);
  preferences.putString("plateNum", data->plateNumber);
  preferences.putString("validFrom", data->validFrom);
  preferences.putString("validTo", data->validTo);
  preferences.putString("barcodeVal", data->barcodeValue);
  preferences.putString("barcodeLabel", data->barcodeLabel);
  
  preferences.end();
  Serial.print(COLOR_GREEN);
  Serial.print("Saved permit data to flash. Permit #: ");
  Serial.print(data->permitNumber);
  Serial.print(COLOR_RESET);
  Serial.println();
}

bool tryConnectToWiFi(const char* ssid, const char* password) {
  Serial.print("Trying ");
  Serial.print(ssid);
  Serial.println("...");
  
  // Check if password is empty (open network)
  if (strlen(password) == 0) {
    WiFi.begin(ssid);
  } else {
    WiFi.begin(ssid, password);
  }
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print(COLOR_GREEN);
    Serial.print("Connected to WiFi!");
    Serial.print(COLOR_RESET);
    Serial.println();
    Serial.print("  IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  
  Serial.println();
  Serial.print(COLOR_RED);
  Serial.print("Failed.");
  Serial.print(COLOR_RESET);
  Serial.println();
  return false;
}

bool connectToWiFi() {
  Serial.println("Attempting to connect to WiFi...");
  
  // Try WiFi 1
  if (tryConnectToWiFi(WIFI_SSID_1, WIFI_PASS_1)) {
    return true;
  }
  WiFi.disconnect();
  
  // Try WiFi 2
  if (tryConnectToWiFi(WIFI_SSID_2, WIFI_PASS_2)) {
    return true;
  }
  WiFi.disconnect();
  
  // Try WiFi 3
  if (tryConnectToWiFi(WIFI_SSID_3, WIFI_PASS_3)) {
    return true;
  }
  
  Serial.print(COLOR_RED);
  Serial.print("Failed to connect to any WiFi network.");
  Serial.print(COLOR_RESET);
  Serial.println();
  return false;
}

// Returns: 0 = error, 1 = updated, 2 = already up to date
int downloadPermitData(PermitData* data, const char* currentPermitNumber) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print(COLOR_RED);
    Serial.print("Not connected to WiFi!");
    Serial.print(COLOR_RESET);
    Serial.println();
    return 0;
  }
  
  HTTPClient http;
  http.begin(SERVER_URL);
  
  Serial.print("Downloading permit data from ");
  Serial.println(SERVER_URL);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Download successful!");
    
    // Parse JSON
    // Use a JsonDocument with enough capacity for the expected JSON payload.
    // Increase size if your JSON grows.
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print(COLOR_RED);
      Serial.print("JSON parsing failed: ");
      Serial.print(error.c_str());
      Serial.print(COLOR_RESET);
      Serial.println();
      http.end();
      return 0;
    }
    
    Serial.println("Permit data raw:");
    
    Serial.print(payload);
    
    Serial.println();
    
    // Validate that required field exists and is a string
    if (!doc[JSON_PERMIT_NUMBER].is<const char*>()) {
      Serial.print(COLOR_RED);
      Serial.print("JSON missing required field: permit number");
      Serial.print(COLOR_RESET);
      Serial.println();
      http.end();
      return 0;
    }
    
    const char* newPermitNumber = doc[JSON_PERMIT_NUMBER];
    
    // Check if permit number is empty
    if (strlen(newPermitNumber) == 0) {
      Serial.print(COLOR_RED);
      Serial.print("Permit number is empty in JSON response");
      Serial.print(COLOR_RESET);
      Serial.println();
      http.end();
      return 0;
    }
    
    Serial.print(COLOR_MAGENTA);
    Serial.print("Server permit #: ");
    Serial.print(newPermitNumber);
    Serial.print(COLOR_RESET);
    Serial.println();
    Serial.print(COLOR_MAGENTA);
    Serial.print("Current permit #: ");
    Serial.print(currentPermitNumber);
    Serial.print(COLOR_RESET);
    Serial.println();
    
    // Compare permit numbers
    if (strcmp(newPermitNumber, currentPermitNumber) == 0) {
      Serial.print(COLOR_YELLOW);
      Serial.print("Permit number matches. No changes needed.");
      Serial.print(COLOR_RESET);
      Serial.println();
      http.end();
      return 2;  // Already up to date
    }
    
    Serial.print(COLOR_GREEN);
    Serial.print("New permit detected! Updating...");
    Serial.print(COLOR_RESET);
    Serial.println();
    
    // Extract data from JSON using helper function
    safeJsonCopy(data->permitNumber, sizeof(data->permitNumber), doc, JSON_PERMIT_NUMBER);
    safeJsonCopy(data->plateNumber, sizeof(data->plateNumber), doc, JSON_PLATE_NUMBER);
    safeJsonCopy(data->validFrom, sizeof(data->validFrom), doc, JSON_VALID_FROM);
    safeJsonCopy(data->validTo, sizeof(data->validTo), doc, JSON_VALID_TO);
    safeJsonCopy(data->barcodeValue, sizeof(data->barcodeValue), doc, JSON_BARCODE_VALUE);
    safeJsonCopy(data->barcodeLabel, sizeof(data->barcodeLabel), doc, JSON_BARCODE_LABEL);
    
    http.end();
    Serial.print(COLOR_GREEN);
    Serial.print("Permit data parsed successfully!");
    Serial.print(COLOR_RESET);
    Serial.println();
    
    // Save to flash memory
    savePermitData(data);
    
    return 1;  // Updated
  } else {
    Serial.print(COLOR_RED);
    Serial.print("HTTP request failed with code: ");
    Serial.print(httpCode);
    Serial.print(COLOR_RESET);
    Serial.println();
    http.end();
    return 0;  // Error
  }
}

void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disconnected to save power.");
}

#endif