#pragma once

#include <GfxRenderer.h>
#include <Theme.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../Elements.h"

namespace ui {

// ============================================================================
// OpdsServerListView - List of configured OPDS servers
// ============================================================================

struct OpdsServerListView {
  static constexpr int MAX_SERVERS = 16;
  static constexpr int NAME_LEN = 48;
  static constexpr int URL_LEN = 128;
  static constexpr int PAGE_SIZE = 10;

  struct Server {
    char name[NAME_LEN];
    char url[URL_LEN];
  };

  ButtonBar buttons{"Back", "Open", "Add", "Delete"};
  Server servers[MAX_SERVERS];
  uint8_t serverCount = 0;
  uint8_t selected = 0;
  uint8_t page = 0;
  bool needsRender = true;

  void clear() {
    serverCount = 0;
    selected = 0;
    page = 0;
    needsRender = true;
  }

  bool addServer(const char* name, const char* url) {
    if (serverCount < MAX_SERVERS) {
      strncpy(servers[serverCount].name, name, NAME_LEN - 1);
      servers[serverCount].name[NAME_LEN - 1] = '\0';
      strncpy(servers[serverCount].url, url, URL_LEN - 1);
      servers[serverCount].url[URL_LEN - 1] = '\0';
      serverCount++;
      return true;
    }
    return false;
  }

  int getPageStart() const { return page * PAGE_SIZE; }

  int getPageEnd() const {
    int end = (page + 1) * PAGE_SIZE;
    return end > serverCount ? serverCount : end;
  }

  void moveUp() {
    if (selected > 0) {
      selected--;
      if (selected < getPageStart()) {
        page--;
      }
      needsRender = true;
    }
  }

  void moveDown() {
    if (selected < serverCount - 1) {
      selected++;
      if (selected >= getPageEnd()) {
        page++;
      }
      needsRender = true;
    }
  }

  const Server* getSelectedServer() const {
    if (selected < serverCount) {
      return &servers[selected];
    }
    return nullptr;
  }
};

void render(const GfxRenderer& r, const Theme& t, const OpdsServerListView& v);

// ============================================================================
// OpdsBrowserView - Browse OPDS catalog entries
// ============================================================================

struct OpdsBrowserView {
  static constexpr int MAX_ENTRIES = 32;
  static constexpr int TITLE_LEN = 64;
  static constexpr int AUTHOR_LEN = 48;
  static constexpr int URL_LEN = 256;
  static constexpr int PAGE_SIZE = 8;

  enum class EntryType : uint8_t { Navigation, Book };

  struct Entry {
    char title[TITLE_LEN];
    char author[AUTHOR_LEN];  // For books
    char url[URL_LEN];
    EntryType type;
  };

  ButtonBar buttons{"Back", "Open", "", ""};
  char currentTitle[TITLE_LEN] = "OPDS";
  Entry entries[MAX_ENTRIES];
  uint8_t entryCount = 0;
  uint8_t selected = 0;
  uint8_t page = 0;
  bool loading = false;
  bool needsRender = true;

  void clear() {
    entryCount = 0;
    selected = 0;
    page = 0;
    needsRender = true;
  }

  void setTitle(const char* t) {
    strncpy(currentTitle, t, TITLE_LEN - 1);
    currentTitle[TITLE_LEN - 1] = '\0';
    needsRender = true;
  }

  bool addEntry(const char* title, const char* author, const char* url, EntryType type) {
    if (entryCount < MAX_ENTRIES) {
      strncpy(entries[entryCount].title, title, TITLE_LEN - 1);
      entries[entryCount].title[TITLE_LEN - 1] = '\0';
      if (author) {
        strncpy(entries[entryCount].author, author, AUTHOR_LEN - 1);
        entries[entryCount].author[AUTHOR_LEN - 1] = '\0';
      } else {
        entries[entryCount].author[0] = '\0';
      }
      strncpy(entries[entryCount].url, url, URL_LEN - 1);
      entries[entryCount].url[URL_LEN - 1] = '\0';
      entries[entryCount].type = type;
      entryCount++;
      return true;
    }
    return false;
  }

  void setLoading(bool l) {
    loading = l;
    needsRender = true;
  }

  int getPageStart() const { return page * PAGE_SIZE; }

  int getPageEnd() const {
    int end = (page + 1) * PAGE_SIZE;
    return end > entryCount ? entryCount : end;
  }

  void moveUp() {
    if (selected > 0) {
      selected--;
      if (selected < getPageStart()) {
        page--;
      }
      needsRender = true;
    }
  }

  void moveDown() {
    if (selected < entryCount - 1) {
      selected++;
      if (selected >= getPageEnd()) {
        page++;
      }
      needsRender = true;
    }
  }

  const Entry* getSelectedEntry() const {
    if (selected < entryCount) {
      return &entries[selected];
    }
    return nullptr;
  }
};

void render(const GfxRenderer& r, const Theme& t, const OpdsBrowserView& v);

// ============================================================================
// OpdsDownloadView - Download progress for OPDS books
// ============================================================================

struct OpdsDownloadView {
  static constexpr int MAX_FILENAME_LEN = 64;
  static constexpr int MAX_STATUS_LEN = 48;

  enum class Status : uint8_t { Downloading, Complete, Failed };

  ButtonBar buttons{"Cancel", "", "", ""};
  char filename[MAX_FILENAME_LEN] = {0};
  char statusMsg[MAX_STATUS_LEN] = "Downloading...";
  Status status = Status::Downloading;
  int32_t received = 0;
  int32_t total = 0;
  bool needsRender = true;

  void setFile(const char* name) {
    strncpy(filename, name, MAX_FILENAME_LEN - 1);
    filename[MAX_FILENAME_LEN - 1] = '\0';
    needsRender = true;
  }

  void setProgress(int recv, int tot) {
    received = recv;
    total = tot;
    needsRender = true;
  }

  void setComplete() {
    status = Status::Complete;
    strncpy(statusMsg, "Download complete!", MAX_STATUS_LEN);
    buttons = ButtonBar{"Back", "Open", "", ""};
    needsRender = true;
  }

  void setFailed(const char* reason) {
    status = Status::Failed;
    strncpy(statusMsg, reason, MAX_STATUS_LEN - 1);
    statusMsg[MAX_STATUS_LEN - 1] = '\0';
    buttons = ButtonBar{"Back", "Retry", "", ""};
    needsRender = true;
  }
};

void render(const GfxRenderer& r, const Theme& t, const OpdsDownloadView& v);

}  // namespace ui
