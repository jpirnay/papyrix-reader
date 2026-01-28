#include "CalibreViews.h"

#include "../Elements.h"

namespace ui {

void render(const GfxRenderer& r, const Theme& t, const CalibreView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "Calibre");

  const int centerY = r.getScreenHeight() / 2 - 60;

  centeredText(r, t, centerY, v.statusMsg);

  if (v.status == CalibreView::Status::Receiving && v.total > 0) {
    progress(r, t, centerY + 50, v.received, v.total);

    char sizeStr[32];
    snprintf(sizeStr, sizeof(sizeStr), "%d / %d KB", v.received / 1024, v.total / 1024);
    centeredText(r, t, centerY + 100, sizeStr);
  }

  if (v.status == CalibreView::Status::Complete || v.status == CalibreView::Status::Error) {
    buttonBar(r, t, "Back", "", "", "");
  } else {
    buttonBar(r, t, "Cancel", "", "", "");
  }

  r.displayBuffer();
}

}  // namespace ui
