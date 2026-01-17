#include "ConfirmActionActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "ThemeManager.h"

void ConfirmActionActivity::onEnter() {
  Activity::onEnter();
  render();
}

void ConfirmActionActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
      mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    if (selection > 0) {
      selection--;
      render();
    }
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Right) ||
             mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    if (selection < 1) {
      selection++;
      render();
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selection == 0) {
      onConfirm();
    } else {
      onCancel();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onCancel();
    return;
  }
}

void ConfirmActionActivity::render() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto lineHeight = renderer.getLineHeight(THEME.uiFontId);
  const auto top = (pageHeight - lineHeight * 3) / 2;

  renderer.clearScreen(THEME.backgroundColor);

  // Title
  renderer.drawCenteredText(THEME.readerFontId, top - 40, title, THEME.primaryTextBlack, BOLD);

  // Description lines
  renderer.drawCenteredText(THEME.uiFontId, top, line1, THEME.primaryTextBlack);
  if (line2 && line2[0] != '\0') {
    renderer.drawCenteredText(THEME.uiFontId, top + lineHeight, line2, THEME.primaryTextBlack);
  }

  // Yes/No buttons
  const int buttonY = top + lineHeight * 3;
  constexpr int buttonWidth = 80;
  constexpr int buttonHeight = 36;
  constexpr int buttonSpacing = 20;
  constexpr int totalWidth = buttonWidth * 2 + buttonSpacing;
  const int startX = (pageWidth - totalWidth) / 2;

  const char* buttonLabels[] = {"Yes", "No"};
  const int buttonPositions[] = {startX, startX + buttonWidth + buttonSpacing};

  for (int i = 0; i < 2; i++) {
    const bool isSelected = (selection == i);
    const int btnX = buttonPositions[i];

    if (isSelected) {
      renderer.fillRect(btnX, buttonY, buttonWidth, buttonHeight, THEME.selectionFillBlack);
    } else {
      renderer.drawRect(btnX, buttonY, buttonWidth, buttonHeight, THEME.primaryTextBlack);
    }

    const bool textColor = isSelected ? THEME.selectionTextBlack : THEME.primaryTextBlack;
    const int textWidth = renderer.getTextWidth(THEME.uiFontId, buttonLabels[i]);
    const int textX = btnX + (buttonWidth - textWidth) / 2;
    const int textY = buttonY + (buttonHeight - renderer.getFontAscenderSize(THEME.uiFontId)) / 2;
    renderer.drawText(THEME.uiFontId, textX, textY, buttonLabels[i], textColor);
  }

  // Button hints
  const auto btnLabels = mappedInput.mapLabels("Back", "Confirm", "Left", "Right");
  renderer.drawButtonHints(THEME.uiFontId, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4,
                           THEME.primaryTextBlack);

  renderer.displayBuffer();
}
