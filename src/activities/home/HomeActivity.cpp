#include "HomeActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "ScreenComponents.h"
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

  // Load book metadata if enabled and we have a book to continue
  lastBookTitle.clear();
  lastBookAuthor.clear();
  if (hasContinueReading) {
    // Extract filename as fallback
    lastBookTitle = APP_STATE.openEpubPath;
    const size_t lastSlash = lastBookTitle.find_last_of('/');
    if (lastSlash != std::string::npos) {
      lastBookTitle = lastBookTitle.substr(lastSlash + 1);
    }

    // Check file extension and try to load metadata
    const std::string ext5 = lastBookTitle.length() >= 5 ? lastBookTitle.substr(lastBookTitle.length() - 5) : "";
    const std::string ext4 = lastBookTitle.length() >= 4 ? lastBookTitle.substr(lastBookTitle.length() - 4) : "";

    if (ext5 == ".epub" && SETTINGS.showBookMetadata) {
      // Try to load EPUB metadata from cache (don't build if missing)
      Epub epub(APP_STATE.openEpubPath, "/.crosspoint");
      if (epub.load(false)) {
        if (!epub.getTitle().empty()) {
          lastBookTitle = std::string(epub.getTitle());
        }
        if (!epub.getAuthor().empty()) {
          lastBookAuthor = std::string(epub.getAuthor());
        }
      }
    } else if (ext5 == ".xtch") {
      // Strip .xtch extension
      lastBookTitle.resize(lastBookTitle.length() - 5);
    } else if (ext4 == ".xtc") {
      // Strip .xtc extension
      lastBookTitle.resize(lastBookTitle.length() - 4);
    }
  }

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

  // Battery indicator - top right
  const int batteryX = renderer.getScreenWidth() - 60;
  const int batteryY = 10;
  ScreenComponents::drawBattery(renderer, batteryX, batteryY);

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

    // Determine text color based on state
    const bool textColor = isDisabled ? THEME.secondaryTextBlack : (isSelected ? THEME.selectionTextBlack : THEME.primaryTextBlack);

    // Draw cell background
    if (isDisabled) {
      renderer.drawRect(cellX, cellY, cellWidth, cellHeight, THEME.primaryTextBlack);
    } else if (isSelected) {
      renderer.fillRect(cellX, cellY, cellWidth, cellHeight, THEME.selectionFillBlack);
    } else {
      renderer.drawRect(cellX, cellY, cellWidth, cellHeight, THEME.primaryTextBlack);
    }

    // Special handling for READ cell (i==0) with book metadata
    if (i == 0 && hasContinueReading && !lastBookTitle.empty()) {
      // Show book title (truncated to fit cell)
      const int maxTextWidth = cellWidth - 20;
      std::string displayTitle = renderer.truncatedText(THEME.uiFontId, lastBookTitle.c_str(), maxTextWidth);
      const int titleWidth = renderer.getTextWidth(THEME.uiFontId, displayTitle.c_str());
      const int titleX = cellX + (cellWidth - titleWidth) / 2;

      if (!lastBookAuthor.empty()) {
        // Show title and author on two lines
        std::string displayAuthor = renderer.truncatedText(THEME.uiFontId, lastBookAuthor.c_str(), maxTextWidth);
        const int authorWidth = renderer.getTextWidth(THEME.uiFontId, displayAuthor.c_str());
        const int authorX = cellX + (cellWidth - authorWidth) / 2;
        const int lineHeight = renderer.getLineHeight(THEME.uiFontId);
        const int totalHeight = lineHeight * 2;
        const int titleY = cellY + (cellHeight - totalHeight) / 2;
        const int authorY = titleY + lineHeight;
        renderer.drawText(THEME.uiFontId, titleX, titleY, displayTitle.c_str(), textColor);
        renderer.drawText(THEME.uiFontId, authorX, authorY, displayAuthor.c_str(), textColor);
      } else {
        // Show title only, centered
        const int titleY = cellY + cellHeight / 2 - renderer.getFontAscenderSize(THEME.uiFontId) / 2;
        renderer.drawText(THEME.uiFontId, titleX, titleY, displayTitle.c_str(), textColor);
      }
    } else {
      // Show standard label
      const char* label = isDisabled ? "N/A" : labels[i];
      const int textWidth = renderer.getTextWidth(THEME.readerFontId, label, BOLD);
      const int textX = cellX + (cellWidth - textWidth) / 2;
      const int textY = cellY + cellHeight / 2 - renderer.getFontAscenderSize(THEME.readerFontId) / 2;
      renderer.drawText(THEME.readerFontId, textX, textY, label, textColor, BOLD);
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
    // Use lastBookTitle which was loaded in onEnter() (contains title or filename)
    std::string continueLabel = "Continue: " + lastBookTitle;
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
