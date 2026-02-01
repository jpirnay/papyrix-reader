#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <memory>

#include "../content/ReaderNavigation.h"
#include "../core/Types.h"
#include "../rendering/XtcPageRenderer.h"
#include "../ui/views/HomeView.h"
#include "State.h"

class GfxRenderer;
class PageCache;
class Page;
struct RenderConfig;

namespace papyrix {

// Forward declarations
class Core;
struct Event;

// ReaderState - unified reader for all content types
// Uses ContentHandle to abstract Epub/Xtc/Txt/Markdown differences
// Uses PageCache for all formats with partial caching support
// Delegates to: XtcPageRenderer (binary rendering), ProgressManager (persistence),
//               ReaderNavigation (page traversal)
class ReaderState : public State {
 public:
  explicit ReaderState(GfxRenderer& renderer);
  ~ReaderState() override;

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::Reader; }

  // Set content path before entering state
  void setContentPath(const char* path);

  // Reading position
  uint32_t currentPage() const { return currentPage_; }
  void setCurrentPage(uint32_t page) { currentPage_ = page; }

 private:
  GfxRenderer& renderer_;
  XtcPageRenderer xtcRenderer_;
  char contentPath_[256];
  uint32_t currentPage_;
  bool needsRender_;
  bool contentLoaded_;
  bool loadFailed_ = false;  // Track if content loading failed (for error state transition)

  // Reading position (maps to ReaderNavigation::Position)
  int currentSpineIndex_;
  int currentSectionPage_;

  // Last successfully rendered position (for accurate progress saving)
  int lastRenderedSpineIndex_ = 0;
  int lastRenderedSectionPage_ = 0;

  // Whether book has a valid cover image
  bool hasCover_ = false;

  // First text content spine index (from EPUB guide, 0 if not specified)
  int textStartIndex_ = 0;

  // Unified page cache for all content types (protected by cacheMutex_)
  std::unique_ptr<PageCache> pageCache_;
  SemaphoreHandle_t cacheMutex_ = nullptr;  // Protects pageCache_ access
  uint8_t pagesUntilFullRefresh_;

  // Background caching (FreeRTOS task for page cache building)
  TaskHandle_t cacheTaskHandle_ = nullptr;
  std::atomic<bool> cacheTaskComplete_{false};
  std::atomic<bool> cacheTaskStopRequested_{false};
  Core* coreForCacheTask_ = nullptr;
  static void cacheTaskTrampoline(void* param);
  void cacheTaskLoop();
  void startBackgroundCaching(Core& core);
  void stopBackgroundCaching();

  // Navigation helpers (delegates to ReaderNavigation)
  void navigateNext(Core& core);
  void navigatePrev(Core& core);
  void applyNavResult(const ReaderNavigation::NavResult& result);

  // Rendering
  void renderCurrentPage(Core& core);
  void renderCachedPage(Core& core);
  void renderXtcPage(Core& core);
  bool renderCoverPage(Core& core);

  // Helpers
  void renderPageContents(Core& core, Page& page, int marginTop, int marginRight, int marginBottom, int marginLeft);
  void renderStatusBar(Core& core, int marginRight, int marginBottom, int marginLeft);

  // Cache management
  bool ensurePageCached(Core& core, uint16_t pageNum);
  void loadCacheFromDisk(Core& core);
  void createOrExtendCache(Core& core);

  // Template helper for cache creation/extension (reduces duplication)
  template <typename ParserT>
  void createOrExtendCacheImpl(ParserT& parser, const std::string& cachePath, const RenderConfig& config);

  // Background caching template helper
  template <typename ParserT>
  void backgroundCacheImpl(ParserT& parser, const std::string& cachePath, const RenderConfig& config);

  // Display helpers
  void displayWithRefresh(Core& core);

  // Viewport calculation
  struct Viewport {
    int marginTop;
    int marginRight;
    int marginBottom;
    int marginLeft;
    int width;
    int height;
  };
  Viewport getReaderViewport() const;

  // Get first content spine index (skips cover document when appropriate)
  static int calcFirstContentSpine(bool hasCover, int textStartIndex, size_t spineCount);

  // Source state (where reader was opened from)
  StateId sourceState_ = StateId::Home;

  // TOC overlay mode
  bool tocMode_ = false;
  ui::ChapterListView tocView_;

  void enterTocMode(Core& core);
  void exitTocMode();
  void handleTocInput(Core& core, const Event& e);
  void renderTocOverlay(Core& core);
  void populateTocView(Core& core);
  int findCurrentTocEntry(Core& core);
  void jumpToTocEntry(Core& core, int tocIndex);

  // Boot mode transition - exit to UI via restart
  void exitToUI(Core& core);
};

}  // namespace papyrix
