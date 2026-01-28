#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <string>

#include "../ui/views/HomeView.h"
#include "State.h"

class GfxRenderer;

namespace papyrix {

class HomeState : public State {
 public:
  explicit HomeState(GfxRenderer& renderer);
  ~HomeState() override;

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::Home; }

 private:
  GfxRenderer& renderer_;
  ui::HomeView view_;

  // Cover image state
  std::string coverBmpPath_;
  bool hasCoverImage_ = false;
  bool coverLoadFailed_ = false;

  // Compressed thumbnail caching (replaces 48KB full buffer with ~2-4KB compressed)
  static constexpr uint16_t COVER_CACHE_WIDTH = 120;
  static constexpr uint16_t COVER_CACHE_HEIGHT = 180;
  static constexpr size_t MAX_COVER_CACHE_SIZE = 4096;
  uint8_t* compressedThumb_ = nullptr;
  size_t compressedSize_ = 0;
  int16_t thumbX_ = 0;  // Position where thumbnail was captured
  int16_t thumbY_ = 0;
  bool coverBufferStored_ = false;
  bool coverRendered_ = false;

  // Async cover generation
  TaskHandle_t coverGenTaskHandle_ = nullptr;
  std::atomic<bool> coverGenComplete_{false};
  std::string pendingBookPath_;
  std::string pendingCacheDir_;
  std::string generatedCoverPath_;  // Written by task before release store; read after acquire exchange

  void loadLastBook(Core& core);
  void updateBattery();
  void renderCoverToCard();
  void startCoverGenTask(const char* bookPath, const char* cacheDir);
  void stopCoverGenTask();
  static void coverGenTrampoline(void* arg);
  void coverGenTask();

  // Compressed thumbnail caching methods
  bool storeCoverThumbnail();
  bool restoreCoverThumbnail();
  void freeCoverThumbnail();
};

}  // namespace papyrix
