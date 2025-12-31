#include "BootActivity.h"

#include <GfxRenderer.h>

#include "ThemeManager.h"
#include "config.h"
#include "images/PapyrixLogo.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen(THEME.backgroundColor);
  renderer.drawImage(PapyrixLogo, (pageWidth + 128) / 2, (pageHeight - 128) / 2, 128, 128);
  renderer.drawCenteredText(THEME.uiFontId, pageHeight / 2 + 70, "Papyrix", THEME.primaryTextBlack, BOLD);
  renderer.drawCenteredText(THEME.smallFontId, pageHeight / 2 + 110, "BOOTING", THEME.primaryTextBlack);
  renderer.drawCenteredText(THEME.smallFontId, pageHeight - 30, CROSSPOINT_VERSION, THEME.primaryTextBlack);
  renderer.displayBuffer();
}
