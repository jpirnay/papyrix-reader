#include "NetworkState.h"

#include <Arduino.h>
#include <GfxRenderer.h>

#include <new>

#include "../config.h"
#include "../core/Core.h"
#include "../network/PapyrixWebServer.h"
#include "../network/WifiCredentialStore.h"
#include "../ui/Elements.h"
#include "ThemeManager.h"

namespace papyrix {

namespace {
constexpr const char* AP_SSID = "Papyrix";
}  // namespace

NetworkState::NetworkState(GfxRenderer& renderer)
    : renderer_(renderer),
      currentScreen_(NetworkScreen::ModeSelect),
      needsRender_(true),
      goBack_(false),
      server_(nullptr),
      passwordJustEntered_(false) {
  selectedSSID_[0] = '\0';
}

NetworkState::~NetworkState() {
  delete server_;
  server_ = nullptr;
}

void NetworkState::enter(Core& core) {
  Serial.println("[NET-STATE] Entering");

  currentScreen_ = NetworkScreen::ModeSelect;
  modeView_.selected = 0;
  modeView_.needsRender = true;
  needsRender_ = true;
  goBack_ = false;
  passwordJustEntered_ = false;
  selectedSSID_[0] = '\0';

  // Load saved credentials
  WIFI_STORE.loadFromFile();
}

void NetworkState::exit(Core& core) {
  Serial.println("[NET-STATE] Exiting");

  // Stop web server if running
  stopWebServer(core);

  // Shut down WiFi
  core.network.shutdown();
}

StateTransition NetworkState::update(Core& core) {
  // Handle server client processing if running
  if (server_ && currentScreen_ == NetworkScreen::ServerRunning) {
    server_->handleClient();
  }

  // Check for scan completion
  if (currentScreen_ == NetworkScreen::WifiList && wifiListView_.scanning) {
    if (core.network.isScanComplete()) {
      wifiListView_.clear();
      drivers::WifiNetwork networks[ui::WifiListView::MAX_NETWORKS];
      int count = core.network.getScanResults(networks, ui::WifiListView::MAX_NETWORKS);

      for (int i = 0; i < count; i++) {
        // Convert RSSI to percentage (roughly -100 to -30 dBm -> 0-100%)
        int signal = constrain(map(networks[i].rssi, -100, -30, 0, 100), 0, 100);
        wifiListView_.addNetwork(networks[i].ssid, signal, networks[i].secured);
      }

      wifiListView_.setScanning(false);
      needsRender_ = true;
    }
  }

  Event e;
  while (core.events.pop(e)) {
    if (e.type != EventType::ButtonPress) continue;

    switch (currentScreen_) {
      case NetworkScreen::ModeSelect:
        handleModeSelect(core, e.button);
        break;
      case NetworkScreen::WifiList:
        handleWifiList(core, e.button);
        break;
      case NetworkScreen::PasswordEntry:
        handlePasswordEntry(core, e.button);
        break;
      case NetworkScreen::Connecting:
        handleConnecting(core, e.button);
        break;
      case NetworkScreen::SavePrompt:
        handleSavePrompt(core, e.button);
        break;
      case NetworkScreen::ServerRunning:
        handleServerRunning(core, e.button);
        break;
    }
  }

  if (goBack_) {
    goBack_ = false;
    return StateTransition::to(StateId::Settings);
  }

  return StateTransition::stay(StateId::Network);
}

void NetworkState::render(Core& core) {
  if (!needsRender_) {
    bool viewNeedsRender = false;
    switch (currentScreen_) {
      case NetworkScreen::ModeSelect:
        viewNeedsRender = modeView_.needsRender;
        break;
      case NetworkScreen::WifiList:
        viewNeedsRender = wifiListView_.needsRender;
        break;
      case NetworkScreen::PasswordEntry:
        viewNeedsRender = keyboardView_.needsRender;
        break;
      case NetworkScreen::Connecting:
        viewNeedsRender = connectingView_.needsRender;
        break;
      case NetworkScreen::SavePrompt:
        viewNeedsRender = confirmView_.needsRender;
        break;
      case NetworkScreen::ServerRunning:
        viewNeedsRender = serverView_.needsRender;
        break;
    }
    if (!viewNeedsRender) return;
  }

  switch (currentScreen_) {
    case NetworkScreen::ModeSelect:
      ui::render(renderer_, THEME, modeView_);
      modeView_.needsRender = false;
      break;
    case NetworkScreen::WifiList:
      ui::render(renderer_, THEME, wifiListView_);
      wifiListView_.needsRender = false;
      break;
    case NetworkScreen::PasswordEntry:
      ui::render(renderer_, THEME, keyboardView_);
      keyboardView_.needsRender = false;
      break;
    case NetworkScreen::Connecting:
      ui::render(renderer_, THEME, connectingView_);
      connectingView_.needsRender = false;
      break;
    case NetworkScreen::SavePrompt:
      ui::render(renderer_, THEME, confirmView_);
      confirmView_.needsRender = false;
      break;
    case NetworkScreen::ServerRunning:
      ui::render(renderer_, THEME, serverView_);
      serverView_.needsRender = false;
      break;
  }

  needsRender_ = false;
  core.display.markDirty();
}

void NetworkState::handleModeSelect(Core& core, Button button) {
  switch (button) {
    case Button::Up:
      modeView_.moveUp();
      needsRender_ = true;
      break;

    case Button::Down:
      modeView_.moveDown();
      needsRender_ = true;
      break;

    case Button::Center:
      if (modeView_.selected == 0) {
        // Join Network - start WiFi scan
        startWifiScan(core);
        currentScreen_ = NetworkScreen::WifiList;
        needsRender_ = true;
      } else {
        // Create Hotspot
        startHotspot(core);
      }
      break;

    case Button::Left:
    case Button::Back:
      goBack_ = true;
      break;

    default:
      break;
  }
}

void NetworkState::handleWifiList(Core& core, Button button) {
  switch (button) {
    case Button::Up:
      wifiListView_.moveUp();
      needsRender_ = true;
      break;

    case Button::Down:
      wifiListView_.moveDown();
      needsRender_ = true;
      break;

    case Button::Center:
      if (wifiListView_.networkCount > 0 && !wifiListView_.scanning &&
          wifiListView_.selected < wifiListView_.networkCount) {
        // Select this network
        strncpy(selectedSSID_, wifiListView_.networks[wifiListView_.selected].ssid, sizeof(selectedSSID_) - 1);
        selectedSSID_[sizeof(selectedSSID_) - 1] = '\0';

        // Check if we have saved credentials
        const auto* cred = WIFI_STORE.findCredential(selectedSSID_);
        if (cred) {
          // Try to connect with saved password
          passwordJustEntered_ = false;
          connectToNetwork(core, cred->ssid, cred->password);
        } else if (wifiListView_.networks[wifiListView_.selected].secured) {
          // Need password - show keyboard
          keyboardView_.setTitle("Enter Password");
          keyboardView_.setPassword(false);
          keyboardView_.clear();
          keyboardView_.needsRender = true;
          currentScreen_ = NetworkScreen::PasswordEntry;
          needsRender_ = true;
        } else {
          // Open network - connect directly
          passwordJustEntered_ = false;
          connectToNetwork(core, selectedSSID_, "");
        }
      }
      break;

    case Button::Right:
      // Rescan
      startWifiScan(core);
      needsRender_ = true;
      break;

    case Button::Left:
    case Button::Back:
      core.network.shutdown();
      currentScreen_ = NetworkScreen::ModeSelect;
      modeView_.needsRender = true;
      needsRender_ = true;
      break;

    default:
      break;
  }
}

void NetworkState::handlePasswordEntry(Core& core, Button button) {
  switch (button) {
    case Button::Up:
      keyboardView_.moveUp();
      needsRender_ = true;
      break;

    case Button::Down:
      keyboardView_.moveDown();
      needsRender_ = true;
      break;

    case Button::Left:
      keyboardView_.moveLeft();
      needsRender_ = true;
      break;

    case Button::Right:
      keyboardView_.moveRight();
      needsRender_ = true;
      break;

    case Button::Center:
      if (keyboardView_.confirmKey()) {
        // Input confirmed - try to connect
        passwordJustEntered_ = true;
        connectToNetwork(core, selectedSSID_, keyboardView_.input);
      }
      needsRender_ = true;
      break;

    case Button::Back:
      currentScreen_ = NetworkScreen::WifiList;
      wifiListView_.needsRender = true;
      needsRender_ = true;
      break;

    default:
      break;
  }
}

void NetworkState::handleConnecting(Core& core, Button button) {
  if (button == Button::Left || button == Button::Back) {
    if (connectingView_.status == ui::WifiConnectingView::Status::Failed) {
      // Go back to network list
      currentScreen_ = NetworkScreen::WifiList;
      wifiListView_.needsRender = true;
      needsRender_ = true;
    }
  } else if (button == Button::Center) {
    if (connectingView_.status == ui::WifiConnectingView::Status::Connected) {
      // Ask to save credentials if this was a new password
      if (!WIFI_STORE.hasSavedCredential(selectedSSID_) && passwordJustEntered_) {
        confirmView_.setTitle("Save Password?");
        confirmView_.setMessage("Save password for this network?");
        confirmView_.selectYes();
        confirmView_.needsRender = true;
        currentScreen_ = NetworkScreen::SavePrompt;
        needsRender_ = true;
      } else {
        // Start web server
        startWebServer(core);
      }
    } else if (connectingView_.status == ui::WifiConnectingView::Status::Failed) {
      // Retry - go back to password entry
      keyboardView_.clear();
      keyboardView_.needsRender = true;
      currentScreen_ = NetworkScreen::PasswordEntry;
      needsRender_ = true;
    }
  }
}

void NetworkState::handleSavePrompt(Core& core, Button button) {
  switch (button) {
    case Button::Left:
      confirmView_.selectYes();
      needsRender_ = true;
      break;

    case Button::Right:
      confirmView_.selectNo();
      needsRender_ = true;
      break;

    case Button::Center:
      if (confirmView_.isYesSelected()) {
        WIFI_STORE.addCredential(selectedSSID_, keyboardView_.input);
      }
      // Start web server
      startWebServer(core);
      break;

    case Button::Back:
      // Skip saving, start web server anyway
      startWebServer(core);
      break;

    default:
      break;
  }
}

void NetworkState::handleServerRunning(Core& core, Button button) {
  if (button == Button::Left || button == Button::Back) {
    stopWebServer(core);
    goBack_ = true;
  }
}

void NetworkState::startWifiScan(Core& core) {
  Serial.println("[NET-STATE] Starting WiFi scan");

  wifiListView_.clear();
  wifiListView_.setScanning(true);

  auto result = core.network.startScan();
  if (!result.ok()) {
    Serial.println("[NET-STATE] Failed to start scan");
    wifiListView_.setScanning(false);
  }
}

void NetworkState::connectToNetwork(Core& core, const char* ssid, const char* password) {
  Serial.printf("[NET-STATE] Connecting to: %s\n", ssid);

  connectingView_.setSsid(ssid);
  connectingView_.setConnecting();
  currentScreen_ = NetworkScreen::Connecting;
  needsRender_ = true;

  // Render the connecting screen before blocking connect
  ui::render(renderer_, THEME, connectingView_);
  core.display.markDirty();

  auto result = core.network.connect(ssid, password);

  if (result.ok()) {
    char ip[46];  // INET6_ADDRSTRLEN = 46 for IPv6 addresses
    core.network.getIpAddress(ip, sizeof(ip));
    connectingView_.setConnected(ip);
    Serial.printf("[NET-STATE] Connected, IP: %s\n", ip);
  } else {
    connectingView_.setFailed("Connection failed");
    Serial.println("[NET-STATE] Connection failed");
  }

  needsRender_ = true;
}

void NetworkState::startHotspot(Core& core) {
  Serial.println("[NET-STATE] Starting hotspot");

  // Show connecting message
  connectingView_.setSsid(AP_SSID);
  connectingView_.setConnecting();
  currentScreen_ = NetworkScreen::Connecting;
  needsRender_ = true;

  // Render before blocking operation
  ui::render(renderer_, THEME, connectingView_);
  core.display.markDirty();

  auto result = core.network.startAP(AP_SSID);

  if (result.ok()) {
    char ip[16];
    core.network.getAPIP(ip, sizeof(ip));
    connectingView_.setConnected(ip);
    Serial.printf("[NET-STATE] AP started, IP: %s\n", ip);

    // Small delay then start web server
    delay(500);
    startWebServer(core);
  } else {
    connectingView_.setFailed("Failed to start hotspot");
    Serial.println("[NET-STATE] Failed to start AP");
    needsRender_ = true;
  }
}

void NetworkState::startWebServer(Core& core) {
  Serial.println("[NET-STATE] Starting web server");

  if (!server_) {
    server_ = new (std::nothrow) PapyrixWebServer();
    if (!server_) {
      Serial.println("[NET-STATE] Failed to allocate web server");
      goBack_ = true;
      return;
    }
  }

  server_->begin();

  // Set up server view
  char ip[16];
  bool isApMode = core.network.isAPMode();
  if (isApMode) {
    core.network.getAPIP(ip, sizeof(ip));
    serverView_.setServerInfo(AP_SSID, ip, true);
  } else {
    core.network.getIpAddress(ip, sizeof(ip));
    serverView_.setServerInfo(selectedSSID_, ip, false);
  }

  currentScreen_ = NetworkScreen::ServerRunning;
  needsRender_ = true;
}

void NetworkState::stopWebServer(Core& /* core */) {
  if (server_) {
    Serial.println("[NET-STATE] Stopping web server");
    server_->stop();
    delete server_;
    server_ = nullptr;
  }

  serverView_.setStopped();
}

}  // namespace papyrix
