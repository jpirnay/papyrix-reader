#include "SystemInfoActivity.h"

#include <GfxRenderer.h>
#undef FILE_READ
#undef FILE_WRITE
#include <LittleFS.h>
#include <SDCardManager.h>
#include <WiFi.h>

#include "MappedInputManager.h"
#include "ThemeManager.h"
#include "config.h"

static String formatBytes(uint64_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  if (bytes < 1024ULL * 1024) return String(bytes / 1024) + " KB";
  if (bytes < 1024ULL * 1024 * 1024) return String(bytes / (1024 * 1024)) + " MB";
  return String(bytes / (1024.0 * 1024 * 1024), 1) + " GB";
}

static String getSdCardInfo() { return SdMan.ready() ? "Ready" : "Not available"; }

static String getInternalInfo() {
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  return formatBytes(used) + " / " + formatBytes(total);
}

void SystemInfoActivity::onEnter() {
  Activity::onEnter();
  render();
}

void SystemInfoActivity::loop() {
  // Handle back/cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    onComplete();
    return;
  }
}

void SystemInfoActivity::render() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto lineHeight = renderer.getLineHeight(THEME.uiFontId) + 5;

  renderer.clearScreen(THEME.backgroundColor);

  // Title
  renderer.drawCenteredText(THEME.readerFontId, 10, "System Info", THEME.primaryTextBlack, BOLD);

  // Gather system information
  const int startY = 60;
  int currentY = startY;
  constexpr int labelX = 20;
  const int valueX = pageWidth / 2;

  // Firmware version
  renderer.drawText(THEME.uiFontId, labelX, currentY, "Version:", THEME.primaryTextBlack);
  renderer.drawText(THEME.uiFontId, valueX, currentY, CROSSPOINT_VERSION, THEME.primaryTextBlack);
  currentY += lineHeight;

  // Uptime
  const unsigned long uptimeSeconds = millis() / 1000;
  const unsigned long hours = uptimeSeconds / 3600;
  const unsigned long minutes = (uptimeSeconds % 3600) / 60;
  const unsigned long seconds = uptimeSeconds % 60;
  char uptimeStr[24];
  snprintf(uptimeStr, sizeof(uptimeStr), "%luh %lum %lus", hours, minutes, seconds);
  renderer.drawText(THEME.uiFontId, labelX, currentY, "Uptime:", THEME.primaryTextBlack);
  renderer.drawText(THEME.uiFontId, valueX, currentY, uptimeStr, THEME.primaryTextBlack);
  currentY += lineHeight;

  // WiFi status
  renderer.drawText(THEME.uiFontId, labelX, currentY, "WiFi:", THEME.primaryTextBlack);
  if (WiFi.status() == WL_CONNECTED) {
    renderer.drawText(THEME.uiFontId, valueX, currentY, WiFi.SSID().c_str(), THEME.primaryTextBlack);
  } else {
    renderer.drawText(THEME.uiFontId, valueX, currentY, "Not connected", THEME.primaryTextBlack);
  }
  currentY += lineHeight;

  // MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[24];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  renderer.drawText(THEME.uiFontId, labelX, currentY, "MAC Address:", THEME.primaryTextBlack);
  renderer.drawText(THEME.uiFontId, valueX, currentY, macStr, THEME.primaryTextBlack);
  currentY += lineHeight;

  // Free heap memory
  char heapStr[24];
  snprintf(heapStr, sizeof(heapStr), "%u KB", ESP.getFreeHeap() / 1024);
  renderer.drawText(THEME.uiFontId, labelX, currentY, "Free Memory:", THEME.primaryTextBlack);
  renderer.drawText(THEME.uiFontId, valueX, currentY, heapStr, THEME.primaryTextBlack);
  currentY += lineHeight;

  // Internal flash storage
  renderer.drawText(THEME.uiFontId, labelX, currentY, "Internal Disk:", THEME.primaryTextBlack);
  renderer.drawText(THEME.uiFontId, valueX, currentY, getInternalInfo().c_str(), THEME.primaryTextBlack);
  currentY += lineHeight;

  // SD Card storage
  renderer.drawText(THEME.uiFontId, labelX, currentY, "SD Card:", THEME.primaryTextBlack);
  renderer.drawText(THEME.uiFontId, valueX, currentY, getSdCardInfo().c_str(), THEME.primaryTextBlack);

  // Button hints at bottom
  const auto btnLabels = mappedInput.mapLabels("Back", "", "", "");
  renderer.drawButtonHints(THEME.uiFontId, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4,
                           THEME.primaryTextBlack);

  renderer.displayBuffer();
}
