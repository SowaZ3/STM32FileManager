#ifndef FILE_MANAGER_VIEW_HPP
#define FILE_MANAGER_VIEW_HPP

#include "file_manager_controller.hpp"

namespace fm {

class FileManagerView {
 public:
  explicit FileManagerView(FileManagerController& controller);

  void start();
  void handleTouch(std::uint16_t x, std::uint16_t y);

 private:
  enum class Screen : std::uint8_t { Browser, Rename };

  void drawBrowser();
  void drawHeader();
  void drawList();
  void drawEntryRow(std::size_t index);
  void drawToolbar();
  void drawRenameKeyboard();
  void drawRenameInput();
  void drawButton(std::uint16_t x, std::uint16_t y, std::uint16_t width,
                  std::uint16_t height, const char* label, bool active = true);
  void drawStatus();
  void handleBrowserTouch(std::uint16_t x, std::uint16_t y);
  void handleRenameTouch(std::uint16_t x, std::uint16_t y);
  void openRenameKeyboard();
  void appendCharacter(char character);
  const char* statusText() const;

  FileManagerController& controller_;
  Screen screen_;
  char renameBuffer_[kMaxNameLength + 1U];
  std::size_t renameLength_;
};

}  // namespace fm

#endif
