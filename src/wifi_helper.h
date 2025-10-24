#ifndef WIFI_HELPER_H
#define WIFI_HELPER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "wifi_config.h"

Preferences preferences;

struct PermitData {
  char permitNumber[20];
  char plateNumber[20];
  char validFrom[30];
  char validTo[30];
  char barcodeValue[20];
  char barcodeLabel[20];
};

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
    Serial.print("Loaded permit data from flash. Permit #: ");
    Serial.println(data->permitNumber);
    return true;
  }
  
  preferences.end();
  Serial.println("No saved permit data found in flash.");
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
  Serial.print("Saved permit data to flash. Permit #: ");
  Serial.println(data->permitNumber);
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
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  
  Serial.println("\nFailed.");
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
  
  Serial.println("Failed to connect to any WiFi network.");
  return false;
}

// Returns: 0 = error, 1 = updated, 2 = already up to date
int downloadPermitData(PermitData* data, const char* currentPermitNumber) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi!");
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
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      http.end();
      return 0;
    }
    
    // Check permit number first
    const char* newPermitNumber = doc[JSON_PERMIT_NUMBER] | "T6103268";
    Serial.print("Server permit #: ");
    Serial.println(newPermitNumber);
    Serial.print("Current permit #: ");
    Serial.println(currentPermitNumber);
    
    // Compare permit numbers
    if (strcmp(newPermitNumber, currentPermitNumber) == 0) {
      Serial.println("Permit number matches. No changes needed.");
      http.end();
      return 2;  // Already up to date
    }
    
    Serial.println("New permit detected! Updating...");
    
    // Extract data from JSON
    strncpy(data->permitNumber, newPermitNumber, sizeof(data->permitNumber) - 1);
    strncpy(data->plateNumber, doc[JSON_PLATE_NUMBER] | "CSEB187", sizeof(data->plateNumber) - 1);
    strncpy(data->validFrom, doc[JSON_VALID_FROM] | "Sep 05, 2025: 01:08", sizeof(data->validFrom) - 1);
    strncpy(data->validTo, doc[JSON_VALID_TO] | "Sep 12, 2025: 01:08", sizeof(data->validTo) - 1);
    strncpy(data->barcodeValue, doc[JSON_BARCODE_VALUE] | "6103268", sizeof(data->barcodeValue) - 1);
    strncpy(data->barcodeLabel, doc[JSON_BARCODE_LABEL] | "00435", sizeof(data->barcodeLabel) - 1);
    
    http.end();
    Serial.println("Permit data parsed successfully!");
    
    // Save to flash memory
    savePermitData(data);
    
    return 1;  // Updated
  } else {
    Serial.print("HTTP request failed with code: ");
    Serial.println(httpCode);
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