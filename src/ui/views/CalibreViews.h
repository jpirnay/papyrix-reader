#pragma once

#include <GfxRenderer.h>
#include <Theme.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ui {

struct CalibreView {
  static constexpr int MAX_STATUS_LEN = 64;

  enum class Status : uint8_t { Waiting, Connecting, Receiving, Complete, Error };

  char statusMsg[MAX_STATUS_LEN] = "Waiting for Calibre...";
  Status status = Status::Waiting;
  int32_t received = 0;
  int32_t total = 0;
  bool needsRender = true;

  void setWaiting() {
    status = Status::Waiting;
    strncpy(statusMsg, "Waiting for Calibre...", MAX_STATUS_LEN);
    needsRender = true;
  }

  void setConnecting() {
    status = Status::Connecting;
    strncpy(statusMsg, "Connecting to Calibre...", MAX_STATUS_LEN);
    needsRender = true;
  }

  void setReceiving(const char* filename, int recv, int tot) {
    status = Status::Receiving;
    strncpy(statusMsg, filename, MAX_STATUS_LEN - 1);
    statusMsg[MAX_STATUS_LEN - 1] = '\0';
    received = recv;
    total = tot;
    needsRender = true;
  }

  void setComplete(int bookCount) {
    status = Status::Complete;
    snprintf(statusMsg, MAX_STATUS_LEN, "Received %d book(s)", bookCount);
    needsRender = true;
  }

  void setError(const char* msg) {
    status = Status::Error;
    strncpy(statusMsg, msg, MAX_STATUS_LEN - 1);
    statusMsg[MAX_STATUS_LEN - 1] = '\0';
    needsRender = true;
  }
};

void render(const GfxRenderer& r, const Theme& t, const CalibreView& v);

}  // namespace ui
