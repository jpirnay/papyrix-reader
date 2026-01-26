#include "Network.h"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>
#include <cstring>

namespace papyrix {
namespace drivers {

Result<void> Network::init() {
  if (initialized_) {
    return Ok();
  }

  WiFi.mode(WIFI_STA);
  initialized_ = true;
  connected_ = false;
  apMode_ = false;

  Serial.println("[NET] WiFi initialized (STA mode)");
  return Ok();
}

void Network::shutdown() {
  if (connected_) {
    disconnect();
  }

  if (apMode_) {
    stopAP();
  }

  if (initialized_) {
    WiFi.mode(WIFI_OFF);
    initialized_ = false;
    scanInProgress_ = false;
    Serial.println("[NET] WiFi shut down");
  }
}

Result<void> Network::connect(const char* ssid, const char* password) {
  if (apMode_) {
    stopAP();
  }

  if (!initialized_) {
    TRY(init());
  }

  Serial.printf("[NET] Connecting to %s...\n", ssid);

  WiFi.begin(ssid, password);

  // Wait for connection with timeout
  constexpr uint32_t TIMEOUT_MS = 15000;
  uint32_t startMs = millis();

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startMs > TIMEOUT_MS) {
      Serial.println("[NET] Connection timeout");
      return ErrVoid(Error::Timeout);
    }
    delay(100);
  }

  connected_ = true;
  Serial.printf("[NET] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
  return Ok();
}

void Network::disconnect() {
  if (connected_) {
    WiFi.disconnect();
    connected_ = false;
    Serial.println("[NET] Disconnected");
  }
}

int8_t Network::signalStrength() const {
  if (!connected_) {
    return 0;
  }
  return WiFi.RSSI();
}

void Network::getIpAddress(char* buffer, size_t bufferSize) const {
  if (!connected_ || bufferSize == 0) {
    if (bufferSize > 0) buffer[0] = '\0';
    return;
  }

  String ip = WiFi.localIP().toString();
  strncpy(buffer, ip.c_str(), bufferSize - 1);
  buffer[bufferSize - 1] = '\0';
}

Result<void> Network::startScan() {
  if (!initialized_) {
    TRY(init());
  }

  if (apMode_) {
    return ErrVoid(Error::InvalidOperation);
  }

  Serial.println("[NET] Starting WiFi scan...");
  WiFi.scanDelete();
  int16_t result = WiFi.scanNetworks(true);  // Async scan
  if (result == WIFI_SCAN_FAILED) {
    Serial.println("[NET] Failed to start scan");
    return ErrVoid(Error::IOError);
  }
  scanInProgress_ = true;
  return Ok();
}

bool Network::isScanComplete() const {
  if (!scanInProgress_) {
    return true;
  }

  int16_t result = WiFi.scanComplete();
  return result != WIFI_SCAN_RUNNING;
}

int Network::getScanResults(WifiNetwork* out, int maxCount) {
  if (!out || maxCount <= 0 || !scanInProgress_) {
    return 0;
  }

  int16_t result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING) {
    return 0;
  }

  scanInProgress_ = false;

  if (result == WIFI_SCAN_FAILED || result < 0) {
    Serial.println("[NET] Scan failed");
    return 0;
  }

  int count = std::min(static_cast<int>(result), maxCount);

  for (int i = 0; i < count; i++) {
    strncpy(out[i].ssid, WiFi.SSID(i).c_str(), sizeof(out[i].ssid) - 1);
    out[i].ssid[sizeof(out[i].ssid) - 1] = '\0';
    out[i].rssi = WiFi.RSSI(i);
    out[i].secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }

  // Sort by signal strength (strongest first)
  std::sort(out, out + count, [](const WifiNetwork& a, const WifiNetwork& b) { return a.rssi > b.rssi; });

  Serial.printf("[NET] Scan found %d networks\n", count);
  WiFi.scanDelete();
  return count;
}

Result<void> Network::startAP(const char* ssid, const char* password) {
  if (connected_) {
    disconnect();
  }

  Serial.printf("[NET] Starting AP: %s\n", ssid);

  WiFi.mode(WIFI_AP);

  bool success;
  if (password && strlen(password) >= 8) {
    success = WiFi.softAP(ssid, password);
  } else {
    success = WiFi.softAP(ssid);
  }

  if (!success) {
    Serial.println("[NET] Failed to start AP");
    return ErrVoid(Error::IOError);
  }

  initialized_ = true;
  apMode_ = true;
  Serial.printf("[NET] AP started, IP: %s\n", WiFi.softAPIP().toString().c_str());
  return Ok();
}

void Network::stopAP() {
  if (apMode_) {
    WiFi.softAPdisconnect(true);
    apMode_ = false;
    Serial.println("[NET] AP stopped");
  }
}

void Network::getAPIP(char* buffer, size_t bufferSize) const {
  if (!apMode_ || bufferSize == 0) {
    if (bufferSize > 0) buffer[0] = '\0';
    return;
  }

  String ip = WiFi.softAPIP().toString();
  strncpy(buffer, ip.c_str(), bufferSize - 1);
  buffer[bufferSize - 1] = '\0';
}

}  // namespace drivers
}  // namespace papyrix
