#ifndef FILE_MANAGER_CONTROLLER_HPP
#define FILE_MANAGER_CONTROLLER_HPP

#include "i_file_system.hpp"

namespace fm {

class FileManagerController {
 public:
  explicit FileManagerController(IFileSystem& fileSystem);

  Result start();
  Result refresh();
  Result activate(std::size_t index);
  Result goBack();
  Result nextPage();
  Result previousPage();
  Result select(std::size_t index);
  Result beginCopy();
  Result beginMove();
  Result paste();
  Result renameSelected(const char* newName);
  void cancelPendingOperation();

  const char* currentPath() const { return currentPath_; }
  const Entry* entries() const { return entries_; }
  std::size_t entryCount() const { return entryCount_; }
  std::size_t pageOffset() const { return pageOffset_; }
  bool hasMore() const { return hasMore_; }
  int selectedIndex() const { return selectedIndex_; }
  PendingOperation pendingOperation() const { return pendingOperation_; }
  Result lastResult() const { return lastResult_; }

 private:
  Result enterDirectory(const char* name);
  Result beginOperation(PendingOperation operation);
  Result buildPath(const char* directory, const char* name,
                   char* output, std::size_t outputSize) const;
  const Entry* selectedEntry() const;

  IFileSystem& fileSystem_;
  char currentPath_[kMaxPathLength + 1U];
  char pendingSource_[kMaxPathLength + 1U];
  char pendingName_[kMaxNameLength + 1U];
  Entry entries_[kEntriesPerPage];
  std::size_t entryCount_;
  std::size_t pageOffset_;
  bool hasMore_;
  int selectedIndex_;
  PendingOperation pendingOperation_;
  Result lastResult_;
};

}  // namespace fm

#endif
