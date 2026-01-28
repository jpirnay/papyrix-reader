#include "CalibreSyncState.h"

#include <Arduino.h>
#include <GfxRenderer.h>

#include "../core/Core.h"
#include "../ui/Elements.h"
#include "ThemeManager.h"

namespace papyrix {

namespace {
constexpr const char* BOOKS_DIR = "/Books";
constexpr uint16_t CALIBRE_PORT = 9090;
constexpr uint32_t PROCESS_TIMEOUT_MS = 50;

inline int32_t saturateToInt32(uint64_t value) { return value > INT32_MAX ? INT32_MAX : static_cast<int32_t>(value); }
}  // namespace

CalibreSyncState::CalibreSyncState(GfxRenderer& renderer)
    : renderer_(renderer),
      needsRender_(true),
      goBack_(false),
      syncComplete_(false),
      conn_(nullptr),
      libraryInitialized_(false),
      booksReceived_(0) {}

CalibreSyncState::~CalibreSyncState() { cleanup(); }

void CalibreSyncState::enter(Core& core) {
  Serial.println("[CAL-STATE] Entering");

  needsRender_ = true;
  goBack_ = false;
  syncComplete_ = false;
  libraryInitialized_ = false;
  booksReceived_ = 0;

  // Clear pending sync mode now that we've entered
  core.pendingSync = SyncMode::None;

  calibreView_.setWaiting();

  // Initialize Calibre library
  calibre_err_t err = calibre_init();
  if (err != CALIBRE_OK) {
    Serial.printf("[CAL-STATE] Failed to init library: %s\n", calibre_err_str(err));
    calibreView_.setError("Failed to initialize");
    needsRender_ = true;
    return;
  }
  libraryInitialized_ = true;

  // Configure device
  calibre_device_config_t config;
  calibre_device_config_init(&config);

  snprintf(config.device_name, sizeof(config.device_name), "Papyrix Reader");
  snprintf(config.manufacturer, sizeof(config.manufacturer), "Papyrix");
  snprintf(config.model, sizeof(config.model), "X4");

  // Add supported formats
  calibre_device_config_add_ext(&config, "epub");
  calibre_device_config_add_ext(&config, "txt");

  // Safety: don't allow deletion from Calibre
  config.can_delete_books = 0;

  // Set up callbacks with this pointer as context
  calibre_callbacks_t callbacks = {
      .on_progress = onProgress, .on_book = onBook, .on_message = onMessage, .user_ctx = this};

  // Create connection
  conn_ = calibre_conn_create(&config, &callbacks);
  if (!conn_) {
    Serial.println("[CAL-STATE] Failed to create connection");
    calibreView_.setError("Connection failed");
    needsRender_ = true;
    calibre_deinit();
    libraryInitialized_ = false;
    return;
  }

  // Set books directory
  calibre_set_books_dir(conn_, BOOKS_DIR);

  // Get IP address to display
  char ip[46];
  core.network.getIpAddress(ip, sizeof(ip));

  // Update view with waiting message including IP
  snprintf(calibreView_.statusMsg, sizeof(calibreView_.statusMsg), "IP: %s", ip);

  // Start discovery listener
  err = calibre_start_discovery(conn_, CALIBRE_PORT);
  if (err != CALIBRE_OK) {
    Serial.printf("[CAL-STATE] Failed to start discovery: %s\n", calibre_err_str(err));
    calibreView_.setError("Discovery failed");
    needsRender_ = true;
    cleanup();
    return;
  }

  Serial.printf("[CAL-STATE] Discovery started on port %d, IP: %s\n", CALIBRE_PORT, ip);
}

void CalibreSyncState::exit(Core& core) {
  Serial.println("[CAL-STATE] Exiting");

  cleanup();
  core.network.shutdown();
}

StateTransition CalibreSyncState::update(Core& core) {
  // Poll Calibre protocol if connection active
  if (conn_) {
    calibre_err_t err = calibre_process(conn_, PROCESS_TIMEOUT_MS);

    if (err != CALIBRE_OK && err != CALIBRE_ERR_TIMEOUT) {
      Serial.printf("[CAL-STATE] Process error: %s\n", calibre_err_str(err));

      if (err == CALIBRE_ERR_DISCONNECTED) {
        if (booksReceived_ > 0) {
          // Sync complete - Calibre disconnected after sending books
          syncComplete_ = true;
          calibreView_.setComplete(booksReceived_);
          needsRender_ = true;
        } else {
          // Re-enable discovery for reconnection
          calibre_err_t discErr = calibre_start_discovery(conn_, CALIBRE_PORT);
          if (discErr != CALIBRE_OK) {
            calibreView_.setError("Discovery restart failed");
          } else {
            calibreView_.setWaiting();
          }
          needsRender_ = true;
        }
      } else if (err != CALIBRE_ERR_BUSY) {
        calibreView_.setError(calibre_err_str(err));
        needsRender_ = true;
      }
    }

    // Update connecting status if we detect connection
    if (calibre_is_connected(conn_) && calibreView_.status == ui::CalibreView::Status::Waiting) {
      calibreView_.setConnecting();
      needsRender_ = true;
    }
  }

  // Process button events
  Event e;
  while (core.events.pop(e)) {
    if (e.type != EventType::ButtonPress) continue;
    handleInput(core, e.button);
  }

  if (goBack_) {
    goBack_ = false;
    // exit() will handle restart
    return StateTransition::to(StateId::Settings);
  }

  return StateTransition::stay(StateId::CalibreSync);
}

void CalibreSyncState::render(Core& core) {
  if (!needsRender_ && !calibreView_.needsRender) return;

  ui::render(renderer_, THEME, calibreView_);
  calibreView_.needsRender = false;
  needsRender_ = false;
  core.display.markDirty();
}

void CalibreSyncState::handleInput(Core& /* core */, Button button) {
  switch (button) {
    case Button::Left:
    case Button::Back:
      if (calibreView_.status == ui::CalibreView::Status::Complete ||
          calibreView_.status == ui::CalibreView::Status::Error ||
          calibreView_.status == ui::CalibreView::Status::Waiting) {
        goBack_ = true;
      }
      break;

    case Button::Center:
      if (calibreView_.status == ui::CalibreView::Status::Complete) {
        goBack_ = true;
      }
      break;

    default:
      break;
  }
}

void CalibreSyncState::cleanup() {
  if (conn_) {
    calibre_stop_discovery(conn_);
    calibre_disconnect(conn_);
    calibre_conn_destroy(conn_);
    conn_ = nullptr;
  }
  if (libraryInitialized_) {
    calibre_deinit();
    libraryInitialized_ = false;
  }
}

// Static callbacks - bridge C library to C++ class

bool CalibreSyncState::onProgress(void* ctx, uint64_t current, uint64_t total) {
  auto* self = static_cast<CalibreSyncState*>(ctx);
  if (!self) return true;

  // Use current statusMsg if it contains a book title, otherwise show generic message
  const char* title = self->calibreView_.statusMsg;
  if (!title[0] || strncmp(title, "IP:", 3) == 0) {
    title = "Receiving...";
  }
  self->calibreView_.setReceiving(title, saturateToInt32(current), saturateToInt32(total));

  return true;  // Continue transfer
}

void CalibreSyncState::onBook(void* ctx, const calibre_book_meta_t* meta, const char* path) {
  auto* self = static_cast<CalibreSyncState*>(ctx);
  if (!self || !meta) return;

  self->booksReceived_++;
  Serial.printf("[CAL-STATE] Book received: \"%s\" -> %s\n", meta->title ? meta->title : "(null)",
                path ? path : "(null)");

  // Update view with book title (fallback if title is empty)
  const char* title = (meta->title && meta->title[0]) ? meta->title : "Unknown";
  self->calibreView_.setReceiving(title, 0, 0);
  self->needsRender_ = true;
}

void CalibreSyncState::onMessage(void* ctx, const char* message) {
  const auto* self = static_cast<const CalibreSyncState*>(ctx);
  if (!self || !message) return;

  Serial.printf("[CAL-STATE] Calibre message: %s\n", message);
}

}  // namespace papyrix
