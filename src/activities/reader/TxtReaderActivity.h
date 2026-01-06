/**
 * TxtReaderActivity.h
 *
 * Plain text reader activity for Papyrix Reader
 * Displays TXT files with streaming page rendering
 */

#pragma once

#include <Txt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <vector>

#include "activities/ActivityWithSubactivity.h"

class TxtReaderActivity final : public ActivityWithSubactivity {
  std::shared_ptr<Txt> txt;
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;

  // Page index: stores byte offsets where each page starts
  std::vector<size_t> pageIndex;
  uint32_t currentPage = 0;
  int pagesUntilFullRefresh = 0;
  bool updateRequired = false;
  bool indexBuilt = false;

  // Cache validation
  size_t cachedFileSize = 0;
  int cachedViewportWidth = 0;
  int cachedLinesPerPage = 0;

  const std::function<void()> onGoBack;
  const std::function<void()> onGoHome;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderScreen();
  void renderPage();
  void renderStatusBar(int orientedMarginRight, int orientedMarginBottom, int orientedMarginLeft) const;
  void saveProgress() const;
  void loadProgress();
  bool loadPageIndex();
  bool buildPageIndex();
  bool savePageIndex() const;
  bool validatePageIndexCache() const;

  // Text rendering helpers
  size_t getNextUtf8Char(const uint8_t* text, size_t offset, size_t maxLen) const;

 public:
  explicit TxtReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::unique_ptr<Txt> txt,
                             const std::function<void()>& onGoBack, const std::function<void()>& onGoHome)
      : ActivityWithSubactivity("TxtReader", renderer, mappedInput),
        txt(std::move(txt)),
        onGoBack(onGoBack),
        onGoHome(onGoHome) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};
