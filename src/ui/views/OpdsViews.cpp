#include "OpdsViews.h"

#include <cstdio>

namespace ui {

void render(const GfxRenderer& r, const Theme& t, const OpdsServerListView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "OPDS Servers");

  if (v.serverCount == 0) {
    const int centerY = r.getScreenHeight() / 2;
    centeredText(r, t, centerY, "No servers configured");
    centeredText(r, t, centerY + 30, "Add servers in settings");
  } else {
    const int listStartY = 60;
    const int pageStart = v.getPageStart();
    const int pageEnd = v.getPageEnd();

    for (int i = pageStart; i < pageEnd; i++) {
      const int y = listStartY + (i - pageStart) * (t.itemHeight + t.itemSpacing);
      menuItem(r, t, y, v.servers[i].name, i == v.selected);
    }
  }

  buttonBar(r, t, v.buttons);

  r.displayBuffer();
}

void render(const GfxRenderer& r, const Theme& t, const OpdsBrowserView& v) {
  r.clearScreen(t.backgroundColor);

  // Title (catalog name)
  title(r, t, t.screenMarginTop, v.currentTitle);

  if (v.loading) {
    const int centerY = r.getScreenHeight() / 2;
    centeredText(r, t, centerY, "Loading...");
  } else if (v.entryCount == 0) {
    const int centerY = r.getScreenHeight() / 2;
    centeredText(r, t, centerY, "No entries");
  } else {
    const int listStartY = 60;
    const int pageStart = v.getPageStart();
    const int pageEnd = v.getPageEnd();
    const int entryHeight = t.itemHeight + 10;  // Taller entries for book info

    for (int i = pageStart; i < pageEnd; i++) {
      const int y = listStartY + (i - pageStart) * entryHeight;
      const auto& entry = v.entries[i];
      const bool isSelected = (i == v.selected);

      const int x = t.screenMarginSide;
      const int w = r.getScreenWidth() - 2 * t.screenMarginSide;

      if (isSelected) {
        r.fillRect(x, y, w, entryHeight - 2, t.selectionFillBlack);
      }

      // Entry icon/prefix
      const char* prefix = (entry.type == OpdsBrowserView::EntryType::Navigation) ? "[>] " : "[B] ";
      r.drawText(t.uiFontId, x + 4, y + 5, prefix, isSelected ? t.selectionTextBlack : t.secondaryTextBlack);

      // Title
      const int titleX = x + 35;
      const int maxTitleW = w - 45;
      const auto truncTitle = r.truncatedText(t.uiFontId, entry.title, maxTitleW);
      r.drawText(t.uiFontId, titleX, y + 5, truncTitle.c_str(), isSelected ? t.selectionTextBlack : t.primaryTextBlack);

      // Author for books
      if (entry.type == OpdsBrowserView::EntryType::Book && entry.author[0] != '\0') {
        const auto truncAuthor = r.truncatedText(t.smallFontId, entry.author, maxTitleW);
        r.drawText(t.smallFontId, titleX, y + 25, truncAuthor.c_str(),
                   isSelected ? t.selectionTextBlack : t.secondaryTextBlack);
      }
    }

    // Page indicator
    const int pageCount = (v.entryCount + OpdsBrowserView::PAGE_SIZE - 1) / OpdsBrowserView::PAGE_SIZE;
    if (pageCount > 1) {
      char pageStr[16];
      snprintf(pageStr, sizeof(pageStr), "%d/%d", v.page + 1, pageCount);
      const int pageY = r.getScreenHeight() - 50;
      centeredText(r, t, pageY, pageStr);
    }
  }

  buttonBar(r, t, v.buttons);

  r.displayBuffer();
}

void render(const GfxRenderer& r, const Theme& t, const OpdsDownloadView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "Downloading");

  const int centerY = r.getScreenHeight() / 2 - 60;

  // Filename
  const int maxW = r.getScreenWidth() - 40;
  const auto truncName = r.truncatedText(t.uiFontId, v.filename, maxW);
  centeredText(r, t, centerY, truncName.c_str());

  // Progress bar
  if (v.status == OpdsDownloadView::Status::Downloading) {
    progress(r, t, centerY + 50, v.received, v.total);

    // Size info
    char sizeStr[32];
    if (v.total > 0) {
      snprintf(sizeStr, sizeof(sizeStr), "%d / %d KB", v.received / 1024, v.total / 1024);
    } else {
      snprintf(sizeStr, sizeof(sizeStr), "%d KB", v.received / 1024);
    }
    centeredText(r, t, centerY + 100, sizeStr);
  }

  // Status message
  centeredText(r, t, centerY + 140, v.statusMsg);

  buttonBar(r, t, v.buttons);

  r.displayBuffer();
}

}  // namespace ui
