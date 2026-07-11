#include "file_manager_view.hpp"

#include "display_driver.h"

#include <cstring>

namespace fm {

namespace {

constexpr std::uint16_t kHeaderHeight = 34U;
constexpr std::uint16_t kRowHeight = 30U;
constexpr std::uint16_t kStatusY = 184U;
constexpr std::uint16_t kToolbarY = 202U;

struct KeyRow {
  const char* characters;
  std::uint16_t x;
  std::uint16_t y;
  std::uint16_t keyWidth;
};

constexpr KeyRow kKeyRows[] = {
    {"1234567890", 0U, 38U, 32U},
    {"QWERTYUIOP", 0U, 72U, 32U},
    {"ASDFGHJKL", 16U, 106U, 32U},
    {"ZXCVBNM", 48U, 140U, 32U},
};

std::size_t TextLength(const char* text) {
  return (text == nullptr) ? 0U : std::strlen(text);
}

}  // namespace

FileManagerView::FileManagerView(FileManagerController& controller)
    : controller_(controller),
      screen_(Screen::Browser),
      renameBuffer_{},
      renameLength_(0U) {}

void FileManagerView::start() {
  controller_.start();
  drawBrowser();
}

void FileManagerView::handleTouch(std::uint16_t x, std::uint16_t y) {
  if (screen_ == Screen::Browser) {
    handleBrowserTouch(x, y);
  } else {
    handleRenameTouch(x, y);
  }
}

void FileManagerView::drawButton(std::uint16_t x, std::uint16_t y,
                                 std::uint16_t width, std::uint16_t height,
                                 const char* label, bool active) {
  const std::uint16_t background = active ? DISPLAY_COLOR_WHITE
                                           : DISPLAY_COLOR_LIGHT_GREY;
  DisplayDriver_FillRect(x, y, width, height, background);
  DisplayDriver_DrawRect(x, y, width, height, DISPLAY_COLOR_LINE_GREY);

  const std::size_t length = TextLength(label);
  const std::uint16_t textWidth = static_cast<std::uint16_t>(length * 6U);
  const std::uint16_t textX = (textWidth < width)
                                  ? static_cast<std::uint16_t>(x + (width - textWidth) / 2U)
                                  : static_cast<std::uint16_t>(x + 2U);
  DisplayDriver_DrawText(textX, static_cast<std::uint16_t>(y + (height - 7U) / 2U),
                         label, DISPLAY_COLOR_BLACK, background, 1U);
}

void FileManagerView::drawBrowser() {
  screen_ = Screen::Browser;
  DisplayDriver_FillScreen(DISPLAY_COLOR_WHITE);
  drawHeader();
  drawList();
  drawStatus();
  drawToolbar();
}

void FileManagerView::drawHeader() {
  DisplayDriver_FillRect(0U, 0U, DISPLAY_WIDTH, kHeaderHeight,
                         DISPLAY_COLOR_WHITE);
  drawButton(0U, 0U, 48U, kHeaderHeight, "BACK",
             std::strcmp(controller_.currentPath(), "/") != 0);
  DisplayDriver_DrawText(52U, 10U, controller_.currentPath(),
                         DISPLAY_COLOR_BLACK, DISPLAY_COLOR_WHITE, 2U);
  drawButton(280U, 0U, 40U, kHeaderHeight, "R", true);
}

void FileManagerView::drawList() {
  for (std::size_t i = 0U; i < kEntriesPerPage; ++i) {
    drawEntryRow(i);
  }
}

void FileManagerView::drawEntryRow(std::size_t index) {
  if (index >= kEntriesPerPage) return;

  const std::uint16_t y = static_cast<std::uint16_t>(
      kHeaderHeight + index * kRowHeight);
  const bool selected = controller_.selectedIndex() == static_cast<int>(index);
  const std::uint16_t background = selected ? DISPLAY_COLOR_LIGHT_GREY
                                             : DISPLAY_COLOR_WHITE;
  DisplayDriver_FillRect(0U, y, DISPLAY_WIDTH, kRowHeight, background);
  DisplayDriver_DrawRect(0U, y, DISPLAY_WIDTH, kRowHeight,
                         DISPLAY_COLOR_LINE_GREY);
  if (index >= controller_.entryCount()) return;

  const Entry& entry = controller_.entries()[index];
  DisplayDriver_DrawText(4U, static_cast<std::uint16_t>(y + 11U),
                         entry.isDirectory ? "DIR" : "FILE",
                         entry.isDirectory ? DISPLAY_COLOR_BLUE
                                           : DISPLAY_COLOR_DARK_GREY,
                         background, 1U);
  DisplayDriver_DrawText(36U, static_cast<std::uint16_t>(y + 11U),
                         entry.name, DISPLAY_COLOR_BLACK, background, 1U);
  if (entry.isDirectory) {
    DisplayDriver_DrawText(306U, static_cast<std::uint16_t>(y + 11U), ">",
                           DISPLAY_COLOR_BLACK, background, 1U);
  }
}

void FileManagerView::drawToolbar() {
  if (controller_.pendingOperation() == PendingOperation::None) {
    drawButton(0U, kToolbarY, 64U, 38U, "PREV", controller_.pageOffset() > 0U);
    drawButton(64U, kToolbarY, 64U, 38U, "NEXT", controller_.hasMore());
    drawButton(128U, kToolbarY, 64U, 38U, "COPY", controller_.selectedIndex() >= 0);
    drawButton(192U, kToolbarY, 64U, 38U, "MOVE", controller_.selectedIndex() >= 0);
    drawButton(256U, kToolbarY, 64U, 38U, "RENAME", controller_.selectedIndex() >= 0);
  } else {
    drawButton(0U, kToolbarY, 64U, 38U, "PREV", controller_.pageOffset() > 0U);
    drawButton(64U, kToolbarY, 64U, 38U, "NEXT", controller_.hasMore());
    drawButton(128U, kToolbarY, 96U, 38U, "PASTE", true);
    drawButton(224U, kToolbarY, 96U, 38U, "CANCEL", true);
  }
}

void FileManagerView::drawStatus() {
  DisplayDriver_FillRect(0U, kStatusY, DISPLAY_WIDTH,
                         static_cast<std::uint16_t>(kToolbarY - kStatusY),
                         DISPLAY_COLOR_WHITE);
  DisplayDriver_DrawText(4U, static_cast<std::uint16_t>(kStatusY + 5U),
                         statusText(),
                         controller_.lastResult() == Result::Ok
                             ? DISPLAY_COLOR_DARK_GREY
                             : DISPLAY_COLOR_RED,
                         DISPLAY_COLOR_WHITE, 1U);
}

const char* FileManagerView::statusText() const {
  if (controller_.lastResult() == Result::Ok) {
    if (controller_.pendingOperation() == PendingOperation::Copy) {
      return "COPY: OPEN TARGET FOLDER AND PASTE";
    }
    if (controller_.pendingOperation() == PendingOperation::Move) {
      return "MOVE: OPEN TARGET FOLDER AND PASTE";
    }
    return "SD READY";
  }
  switch (controller_.lastResult()) {
    case Result::NotMounted: return "SD ERROR: INSERT FAT32 CARD, PRESS R";
    case Result::NotFound: return "FILE OR FOLDER NOT FOUND";
    case Result::AlreadyExists: return "NAME ALREADY EXISTS";
    case Result::InvalidName: return "INVALID FILE NAME";
    case Result::InvalidOperation: return "SELECT A FILE FIRST";
    case Result::PathTooLong: return "PATH IS TOO LONG";
    default: return "SD INPUT OUTPUT ERROR";
  }
}

void FileManagerView::handleBrowserTouch(std::uint16_t x, std::uint16_t y) {
  if (y < kHeaderHeight) {
    if (x < 48U) {
      controller_.goBack();
    } else if (x >= 280U) {
      controller_.start();
    } else {
      return;
    }
    drawHeader();
    drawList();
    drawStatus();
    drawToolbar();
    return;
  }

  if ((y >= kHeaderHeight) && (y < kStatusY)) {
    const std::size_t index = (y - kHeaderHeight) / kRowHeight;
    if (index < controller_.entryCount()) {
      const bool directory = controller_.entries()[index].isDirectory;
      const int previousSelection = controller_.selectedIndex();
      controller_.activate(index);
      if (directory) {
        drawHeader();
        drawList();
      } else {
        if (previousSelection >= 0) {
          drawEntryRow(static_cast<std::size_t>(previousSelection));
        }
        if (previousSelection != static_cast<int>(index)) {
          drawEntryRow(index);
        }
      }
      drawStatus();
      drawToolbar();
    }
    return;
  }

  if (y < kToolbarY) return;

  if (controller_.pendingOperation() == PendingOperation::None) {
    const std::uint16_t button = x / 64U;
    if (button == 0U) {
      if (controller_.pageOffset() == 0U) return;
      controller_.previousPage();
      drawList();
    } else if (button == 1U) {
      if (!controller_.hasMore()) return;
      controller_.nextPage();
      drawList();
    } else if ((button == 2U) || (button == 3U)) {
      const int previousSelection = controller_.selectedIndex();
      if (button == 2U) controller_.beginCopy();
      else controller_.beginMove();
      if (previousSelection >= 0) {
        drawEntryRow(static_cast<std::size_t>(previousSelection));
      }
    } else if (button == 4U) {
      openRenameKeyboard();
      return;
    }
  } else {
    if (x < 64U) {
      if (controller_.pageOffset() == 0U) return;
      controller_.previousPage();
      drawList();
    } else if (x < 128U) {
      if (!controller_.hasMore()) return;
      controller_.nextPage();
      drawList();
    } else if (x < 224U) {
      controller_.paste();
      drawList();
    } else {
      controller_.cancelPendingOperation();
    }
  }
  drawStatus();
  drawToolbar();
}

void FileManagerView::openRenameKeyboard() {
  const int selected = controller_.selectedIndex();
  if ((selected < 0) ||
      (static_cast<std::size_t>(selected) >= controller_.entryCount())) {
    return;
  }
  std::strncpy(renameBuffer_, controller_.entries()[selected].name, kMaxNameLength);
  renameBuffer_[kMaxNameLength] = '\0';
  renameLength_ = std::strlen(renameBuffer_);
  screen_ = Screen::Rename;
  drawRenameKeyboard();
}

void FileManagerView::drawRenameKeyboard() {
  DisplayDriver_FillScreen(DISPLAY_COLOR_WHITE);
  drawRenameInput();

  for (const KeyRow& row : kKeyRows) {
    for (std::size_t i = 0U; row.characters[i] != '\0'; ++i) {
      char label[2] = {row.characters[i], '\0'};
      drawButton(static_cast<std::uint16_t>(row.x + i * row.keyWidth), row.y,
                 static_cast<std::uint16_t>(row.keyWidth - 2U), 32U, label, true);
    }
  }
  drawButton(0U, 176U, 78U, 28U, "CANCEL", true);
  drawButton(80U, 176U, 38U, 28U, ".", true);
  drawButton(120U, 176U, 88U, 28U, "SPACE", true);
  drawButton(210U, 176U, 48U, 28U, "DEL", true);
  drawButton(260U, 176U, 60U, 28U, "OK", renameLength_ > 0U);
  DisplayDriver_DrawText(4U, 216U, "RENAME FILE", DISPLAY_COLOR_DARK_GREY,
                         DISPLAY_COLOR_WHITE, 2U);
}

void FileManagerView::drawRenameInput() {
  DisplayDriver_FillRect(2U, 2U, 316U, 34U, DISPLAY_COLOR_WHITE);
  DisplayDriver_DrawRect(2U, 2U, 316U, 34U, DISPLAY_COLOR_LINE_GREY);
  const char* visibleName = renameBuffer_;
  if (renameLength_ > 25U) visibleName += renameLength_ - 25U;
  DisplayDriver_DrawText(6U, 10U, visibleName, DISPLAY_COLOR_BLACK,
                         DISPLAY_COLOR_WHITE, 2U);
}

void FileManagerView::appendCharacter(char character) {
  if (renameLength_ >= kMaxNameLength) return;
  renameBuffer_[renameLength_++] = character;
  renameBuffer_[renameLength_] = '\0';
}

void FileManagerView::handleRenameTouch(std::uint16_t x, std::uint16_t y) {
  for (const KeyRow& row : kKeyRows) {
    if ((y >= row.y) && (y < (row.y + 32U)) && (x >= row.x)) {
      const std::size_t index = (x - row.x) / row.keyWidth;
      if (index < std::strlen(row.characters)) {
        appendCharacter(row.characters[index]);
        drawRenameInput();
        drawButton(260U, 176U, 60U, 28U, "OK", renameLength_ > 0U);
        return;
      }
    }
  }

  if ((y >= 176U) && (y < 204U)) {
    if (x < 78U) {
      screen_ = Screen::Browser;
      drawBrowser();
      return;
    }
    if ((x >= 80U) && (x < 118U)) appendCharacter('.');
    else if ((x >= 120U) && (x < 208U)) appendCharacter(' ');
    else if ((x >= 210U) && (x < 258U) && (renameLength_ > 0U)) {
      renameBuffer_[--renameLength_] = '\0';
    } else if ((x >= 260U) && (renameLength_ > 0U)) {
      controller_.renameSelected(renameBuffer_);
      screen_ = Screen::Browser;
      drawBrowser();
      return;
    }
    drawRenameInput();
    drawButton(260U, 176U, 60U, 28U, "OK", renameLength_ > 0U);
  }
}

}  // namespace fm
