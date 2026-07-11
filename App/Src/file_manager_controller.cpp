#include "file_manager_controller.hpp"

#include <cstring>

namespace fm {

namespace {

bool IsValidName(const char* name) {
  if ((name == nullptr) || (name[0] == '\0')) {
    return false;
  }

  const std::size_t length = std::strlen(name);
  if ((length > kMaxNameLength) || (std::strcmp(name, ".") == 0) ||
      (std::strcmp(name, "..") == 0)) {
    return false;
  }

  for (std::size_t i = 0U; i < length; ++i) {
    const char c = name[i];
    if ((c < 32) || (c == '/') || (c == '\\') || (c == ':') ||
        (c == '*') || (c == '?') || (c == '"') || (c == '<') ||
        (c == '>') || (c == '|')) {
      return false;
    }
  }
  return true;
}

}  // namespace

FileManagerController::FileManagerController(IFileSystem& fileSystem)
    : fileSystem_(fileSystem),
      currentPath_{'/', '\0'},
      pendingSource_{},
      pendingName_{},
      entries_{},
      entryCount_(0U),
      pageOffset_(0U),
      hasMore_(false),
      selectedIndex_(-1),
      pendingOperation_(PendingOperation::None),
      lastResult_(Result::NotMounted) {}

Result FileManagerController::start() {
  lastResult_ = fileSystem_.mount();
  if (lastResult_ == Result::Ok) {
    lastResult_ = refresh();
  }
  return lastResult_;
}

Result FileManagerController::refresh() {
  entryCount_ = 0U;
  selectedIndex_ = -1;
  hasMore_ = false;
  lastResult_ = fileSystem_.listDirectory(
      currentPath_, pageOffset_, entries_, kEntriesPerPage, entryCount_, hasMore_);
  return lastResult_;
}

Result FileManagerController::activate(std::size_t index) {
  if (index >= entryCount_) {
    return lastResult_ = Result::InvalidOperation;
  }
  if (entries_[index].isDirectory) {
    return enterDirectory(entries_[index].name);
  }
  return select(index);
}

Result FileManagerController::select(std::size_t index) {
  if ((index >= entryCount_) || entries_[index].isDirectory) {
    return lastResult_ = Result::InvalidOperation;
  }
  selectedIndex_ = static_cast<int>(index);
  return lastResult_ = Result::Ok;
}

Result FileManagerController::enterDirectory(const char* name) {
  char nextPath[kMaxPathLength + 1U] = {};
  Result result = buildPath(currentPath_, name, nextPath, sizeof(nextPath));
  if (result != Result::Ok) {
    return lastResult_ = result;
  }
  std::strcpy(currentPath_, nextPath);
  pageOffset_ = 0U;
  return refresh();
}

Result FileManagerController::goBack() {
  if (std::strcmp(currentPath_, "/") == 0) {
    return lastResult_ = Result::Ok;
  }

  char* separator = std::strrchr(currentPath_, '/');
  if ((separator == nullptr) || (separator == currentPath_)) {
    std::strcpy(currentPath_, "/");
  } else {
    *separator = '\0';
  }
  pageOffset_ = 0U;
  return refresh();
}

Result FileManagerController::nextPage() {
  if (!hasMore_) {
    return lastResult_ = Result::Ok;
  }
  pageOffset_ += kEntriesPerPage;
  return refresh();
}

Result FileManagerController::previousPage() {
  if (pageOffset_ == 0U) {
    return lastResult_ = Result::Ok;
  }
  pageOffset_ = (pageOffset_ > kEntriesPerPage)
                    ? pageOffset_ - kEntriesPerPage
                    : 0U;
  return refresh();
}

const Entry* FileManagerController::selectedEntry() const {
  if ((selectedIndex_ < 0) ||
      (static_cast<std::size_t>(selectedIndex_) >= entryCount_)) {
    return nullptr;
  }
  return &entries_[selectedIndex_];
}

Result FileManagerController::beginOperation(PendingOperation operation) {
  const Entry* entry = selectedEntry();
  if (entry == nullptr) {
    return lastResult_ = Result::InvalidOperation;
  }

  Result result = buildPath(currentPath_, entry->name, pendingSource_,
                            sizeof(pendingSource_));
  if (result != Result::Ok) {
    return lastResult_ = result;
  }
  std::strcpy(pendingName_, entry->name);
  pendingOperation_ = operation;
  selectedIndex_ = -1;
  return lastResult_ = Result::Ok;
}

Result FileManagerController::beginCopy() {
  return beginOperation(PendingOperation::Copy);
}

Result FileManagerController::beginMove() {
  return beginOperation(PendingOperation::Move);
}

Result FileManagerController::paste() {
  if (pendingOperation_ == PendingOperation::None) {
    return lastResult_ = Result::InvalidOperation;
  }

  char destination[kMaxPathLength + 1U] = {};
  Result result = buildPath(currentPath_, pendingName_, destination,
                            sizeof(destination));
  if (result != Result::Ok) {
    return lastResult_ = result;
  }
  if (std::strcmp(pendingSource_, destination) == 0) {
    return lastResult_ = Result::InvalidOperation;
  }

  result = (pendingOperation_ == PendingOperation::Copy)
               ? fileSystem_.copyFile(pendingSource_, destination)
               : fileSystem_.move(pendingSource_, destination);
  lastResult_ = result;
  if (result == Result::Ok) {
    cancelPendingOperation();
    return refresh();
  }
  return result;
}

Result FileManagerController::renameSelected(const char* newName) {
  const Entry* entry = selectedEntry();
  if ((entry == nullptr) || !IsValidName(newName)) {
    return lastResult_ = (entry == nullptr) ? Result::InvalidOperation
                                            : Result::InvalidName;
  }

  char source[kMaxPathLength + 1U] = {};
  char destination[kMaxPathLength + 1U] = {};
  Result result = buildPath(currentPath_, entry->name, source, sizeof(source));
  if (result == Result::Ok) {
    result = buildPath(currentPath_, newName, destination, sizeof(destination));
  }
  if (result == Result::Ok) {
    result = fileSystem_.rename(source, destination);
  }
  lastResult_ = result;
  if (result == Result::Ok) {
    return refresh();
  }
  return result;
}

void FileManagerController::cancelPendingOperation() {
  pendingOperation_ = PendingOperation::None;
  pendingSource_[0] = '\0';
  pendingName_[0] = '\0';
}

Result FileManagerController::buildPath(const char* directory, const char* name,
                                        char* output,
                                        std::size_t outputSize) const {
  if ((directory == nullptr) || (name == nullptr) || (output == nullptr) ||
      (outputSize == 0U)) {
    return Result::InvalidOperation;
  }

  const std::size_t directoryLength = std::strlen(directory);
  const std::size_t nameLength = std::strlen(name);
  const bool root = (directoryLength == 1U) && (directory[0] == '/');
  const std::size_t needed = directoryLength + (root ? 0U : 1U) + nameLength + 1U;
  if ((needed > outputSize) || (needed > (kMaxPathLength + 1U))) {
    return Result::PathTooLong;
  }

  std::strcpy(output, directory);
  if (!root) {
    std::strcat(output, "/");
  }
  std::strcat(output, name);
  return Result::Ok;
}

}  // namespace fm
