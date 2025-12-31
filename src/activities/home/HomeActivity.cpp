#include "HomeActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "ThemeManager.h"
#include "config.h"

void HomeActivity::taskTrampoline(void* param) {
  auto* self = static_cast<HomeActivity*>(param);
  self->displayTaskLoop();
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Check if we have a book to continue reading
  hasContinueReading = !APP_STATE.openEpubPath.empty() && SdMan.exists(APP_STATE.openEpubPath.c_str());

  // Start at READ (0) if continue available, otherwise FILES (1)
  selectorIndex = hasContinueReading ? 0 : 1;

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&HomeActivity::taskTrampoline, "HomeActivityTask",
              2048,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

int HomeActivity::getMenuItemCount() const { return hasContinueReading ? 4 : 3; }

void HomeActivity::loop() {
  const bool prevPressed = mappedInput.wasPressed(MappedInputManager::Button::Up) ||
                           mappedInput.wasPressed(MappedInputManager::Button::Left);
  const bool nextPressed = mappedInput.wasPressed(MappedInputManager::Button::Down) ||
                           mappedInput.wasPressed(MappedInputManager::Button::Right);

  const bool isGridLayout = THEME.homeLayout == HOME_GRID;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (isGridLayout) {
      // Grid positions: 0=Continue/READ, 1=Browse/FILES, 2=Transfer/SYNC, 3=Settings/SETUP
      if (selectorIndex == 0 && hasContinueReading) {
        onContinueReading();
      } else if (selectorIndex == 1) {
        onReaderOpen();
      } else if (selectorIndex == 2) {
        onFileTransferOpen();
      } else if (selectorIndex == 3) {
        onSettingsOpen();
      }
    } else {
      // List mode: dynamic menu based on hasContinueReading
      if (hasContinueReading) {
        if (selectorIndex == 0) onContinueReading();
        else if (selectorIndex == 1) onReaderOpen();
        else if (selectorIndex == 2) onFileTransferOpen();
        else if (selectorIndex == 3) onSettingsOpen();
      } else {
        if (selectorIndex == 0) onReaderOpen();
        else if (selectorIndex == 1) onFileTransferOpen();
        else if (selectorIndex == 2) onSettingsOpen();
      }
    }
  } else if (prevPressed) {
    if (isGridLayout) {
      int newIndex = selectorIndex - 1;
      if (newIndex < 0) newIndex = 3;
      if (newIndex == 0 && !hasContinueReading) newIndex = 3;
      selectorIndex = newIndex;
    } else {
      const int menuCount = getMenuItemCount();
      selectorIndex = (selectorIndex + menuCount - 1) % menuCount;
    }
    updateRequired = true;
  } else if (nextPressed) {
    if (isGridLayout) {
      int newIndex = selectorIndex + 1;
      if (newIndex > 3) newIndex = 0;
      if (newIndex == 0 && !hasContinueReading) newIndex = 1;
      selectorIndex = newIndex;
    } else {
      const int menuCount = getMenuItemCount();
      selectorIndex = (selectorIndex + 1) % menuCount;
    }
    updateRequired = true;
  }
}

void HomeActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void HomeActivity::render() const {
  renderer.clearScreen(THEME.backgroundColor);

  if (THEME.homeLayout == HOME_GRID) {
    renderGrid();
  } else {
    renderList();
  }

  const auto btnLabels = mappedInput.mapLabels("Back", "Confirm", "Left", "Right");
  renderer.drawButtonHints(THEME.uiFontId, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4,
                           THEME.primaryTextBlack);

  renderer.displayBuffer();
}

void HomeActivity::renderGrid() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.drawCenteredText(THEME.readerFontId, 10, "Papyrix Reader", THEME.primaryTextBlack, BOLD);

  // Grid layout constants
  constexpr int cellWidth = 180;
  constexpr int cellHeight = 140;
  constexpr int gapX = 40;
  constexpr int gapY = 40;

  // Center the 2x2 grid
  const int gridWidth = cellWidth * 2 + gapX;
  const int gridHeight = cellHeight * 2 + gapY;
  const int startX = (pageWidth - gridWidth) / 2;
  const int startY = (pageHeight - gridHeight) / 2 - 20;

  // Menu items: READ, FILES, SYNC, SETUP (positions 0-3)
  const char* labels[] = {"READ", "FILES", "SYNC", "SETUP"};

  for (int i = 0; i < 4; i++) {
    const int row = i / 2;
    const int col = i % 2;
    const int cellX = startX + col * (cellWidth + gapX);
    const int cellY = startY + row * (cellHeight + gapY);

    const bool isSelected = (selectorIndex == i);
    const bool isDisabled = (i == 0 && !hasContinueReading);

    if (isDisabled) {
      // Draw disabled N/A cell (outline only)
      renderer.drawRect(cellX, cellY, cellWidth, cellHeight, THEME.primaryTextBlack);
      // Center "N/A" text in cell
      const int textWidth = renderer.getTextWidth(THEME.readerFontId, "N/A", BOLD);
      const int textX = cellX + (cellWidth - textWidth) / 2;
      const int textY = cellY + cellHeight / 2 - renderer.getFontAscenderSize(THEME.readerFontId) / 2;
      renderer.drawText(THEME.readerFontId, textX, textY, "N/A", THEME.secondaryTextBlack, BOLD);
    } else if (isSelected) {
      // Draw selected cell (filled with selection color)
      renderer.fillRect(cellX, cellY, cellWidth, cellHeight, THEME.selectionFillBlack);
      // Center text in cell
      const int textWidth = renderer.getTextWidth(THEME.readerFontId, labels[i], BOLD);
      const int textX = cellX + (cellWidth - textWidth) / 2;
      const int textY = cellY + cellHeight / 2 - renderer.getFontAscenderSize(THEME.readerFontId) / 2;
      renderer.drawText(THEME.readerFontId, textX, textY, labels[i], THEME.selectionTextBlack, BOLD);
    } else {
      // Draw unselected cell (outline with primary text color)
      renderer.drawRect(cellX, cellY, cellWidth, cellHeight, THEME.primaryTextBlack);
      // Center text in cell
      const int textWidth = renderer.getTextWidth(THEME.readerFontId, labels[i], BOLD);
      const int textX = cellX + (cellWidth - textWidth) / 2;
      const int textY = cellY + cellHeight / 2 - renderer.getFontAscenderSize(THEME.readerFontId) / 2;
      renderer.drawText(THEME.readerFontId, textX, textY, labels[i], THEME.primaryTextBlack, BOLD);
    }
  }
}

void HomeActivity::renderList() const {
  const auto pageWidth = renderer.getScreenWidth();

  renderer.drawCenteredText(THEME.readerFontId, 10, "Papyrix Reader", THEME.primaryTextBlack, BOLD);

  // Draw selection highlight
  renderer.fillRect(0, 60 + selectorIndex * THEME.itemHeight - 2, pageWidth - 1, THEME.itemHeight, THEME.selectionFillBlack);

  int menuY = 60;
  int menuIndex = 0;

  if (hasContinueReading) {
    // Extract filename from path for display
    std::string bookName = APP_STATE.openEpubPath;
    const size_t lastSlash = bookName.find_last_of('/');
    if (lastSlash != std::string::npos) {
      bookName = bookName.substr(lastSlash + 1);
    }
    // Remove .epub extension
    if (bookName.length() > 5 && bookName.substr(bookName.length() - 5) == ".epub") {
      bookName.resize(bookName.length() - 5);
    }

    // Truncate if too long
    std::string continueLabel = "Continue: " + bookName;
    int itemWidth = renderer.getTextWidth(THEME.uiFontId, continueLabel.c_str());
    while (itemWidth > renderer.getScreenWidth() - 40 && continueLabel.length() > 13) {
      continueLabel.resize(continueLabel.length() - 4);
      continueLabel += "...";
      itemWidth = renderer.getTextWidth(THEME.uiFontId, continueLabel.c_str());
    }

    const bool isSelected = (selectorIndex == menuIndex);
    renderer.drawText(THEME.uiFontId, 20, menuY, continueLabel.c_str(), isSelected ? THEME.selectionTextBlack : THEME.primaryTextBlack);
    menuY += THEME.itemHeight;
    menuIndex++;
  }

  auto drawMenuItem = [&](const char* label) {
    const bool isSelected = (selectorIndex == menuIndex);
    renderer.drawText(THEME.uiFontId, 20, menuY, label, isSelected ? THEME.selectionTextBlack : THEME.primaryTextBlack);
    menuY += THEME.itemHeight;
    menuIndex++;
  };

  drawMenuItem("Browse");
  drawMenuItem("File transfer");
  drawMenuItem("Settings");
}
