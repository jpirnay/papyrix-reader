#pragma once

#include <GfxRenderer.h>
#include <Theme.h>

#include <cstdint>

#include "../Elements.h"

namespace ui {

struct SyncMenuView {
  static constexpr const char* const ITEMS[] = {"File Transfer", "Net Library", "Calibre Wireless"};
  static constexpr int ITEM_COUNT = 3;

  ButtonBar buttons{"Back", "Run", "", ""};
  int8_t selected = 0;
  bool needsRender = true;

  void moveUp() {
    selected = (selected == 0) ? ITEM_COUNT - 1 : selected - 1;
    needsRender = true;
  }

  void moveDown() {
    selected = (selected + 1) % ITEM_COUNT;
    needsRender = true;
  }
};

void render(const GfxRenderer& r, const Theme& t, const SyncMenuView& v);

}  // namespace ui
