#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <functional>

#include "../Activity.h"

class HomeActivity final : public Activity {
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;

  // Cover generation task state
  TaskHandle_t coverGenTaskHandle = nullptr;
  std::atomic<bool> coverGenComplete{false};
  std::string pendingBookPath;
  int selectorIndex = 0;
  bool updateRequired = false;
  bool hasContinueReading = false;
  std::string lastBookTitle;
  std::string lastBookAuthor;
  const std::function<void()> onContinueReading;
  const std::function<void()> onReaderOpen;
  const std::function<void()> onSettingsOpen;

  // Cover image state
  bool hasCoverImage = false;
  bool coverRendered = false;
  bool coverBufferStored = false;
  uint8_t* coverBuffer = nullptr;
  std::string coverBmpPath;

  bool storeCoverBuffer();
  bool restoreCoverBuffer();
  void freeCoverBuffer();

  static void taskTrampoline(void* param);
  static void coverGenTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void coverGenTask();
  void render();

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        const std::function<void()>& onContinueReading, const std::function<void()>& onReaderOpen,
                        const std::function<void()>& onSettingsOpen)
      : Activity("Home", renderer, mappedInput),
        onContinueReading(onContinueReading),
        onReaderOpen(onReaderOpen),
        onSettingsOpen(onSettingsOpen) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};
