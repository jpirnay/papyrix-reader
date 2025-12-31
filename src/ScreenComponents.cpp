#include "ScreenComponents.h"

#include <GfxRenderer.h>

#include "Battery.h"
#include "ThemeManager.h"

void ScreenComponents::drawBattery(const GfxRenderer& renderer, int x, int y) {
  const uint16_t percentage = battery.readPercentage();

  // Battery icon dimensions
  constexpr int batteryWidth = 15;
  constexpr int batteryHeight = 10;
  constexpr int spacing = 5;

  // Draw battery icon
  // Top line
  renderer.drawLine(x, y, x + batteryWidth - 4, y, THEME.primaryTextBlack);
  // Bottom line
  renderer.drawLine(x, y + batteryHeight - 1, x + batteryWidth - 4, y + batteryHeight - 1, THEME.primaryTextBlack);
  // Left line
  renderer.drawLine(x, y, x, y + batteryHeight - 1, THEME.primaryTextBlack);
  // Right line
  renderer.drawLine(x + batteryWidth - 4, y, x + batteryWidth - 4, y + batteryHeight - 1, THEME.primaryTextBlack);
  // Battery nub (right side)
  renderer.drawLine(x + batteryWidth - 3, y + 2, x + batteryWidth - 1, y + 2, THEME.primaryTextBlack);
  renderer.drawLine(x + batteryWidth - 3, y + batteryHeight - 3, x + batteryWidth - 1, y + batteryHeight - 3,
                    THEME.primaryTextBlack);
  renderer.drawLine(x + batteryWidth - 1, y + 2, x + batteryWidth - 1, y + batteryHeight - 3, THEME.primaryTextBlack);

  // Fill level (proportional to percentage)
  int filledWidth = percentage * (batteryWidth - 5) / 100 + 1;
  if (filledWidth > batteryWidth - 5) {
    filledWidth = batteryWidth - 5;
  }
  renderer.fillRect(x + 1, y + 1, filledWidth, batteryHeight - 2, THEME.primaryTextBlack);

  // Draw percentage text to the right of the icon
  char percentageText[8];
  snprintf(percentageText, sizeof(percentageText), "%u%%", percentage);
  renderer.drawText(THEME.smallFontId, x + batteryWidth + spacing, y, percentageText, THEME.primaryTextBlack);
}
