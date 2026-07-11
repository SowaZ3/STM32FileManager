#include "file_manager_controller.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

namespace {

class FakeFileSystem final : public fm::IFileSystem {
 public:
  fm::Result mount() override {
    mounted = true;
    return fm::Result::Ok;
  }

  fm::Result listDirectory(const char* path, std::size_t offset,
                           fm::Entry* entries, std::size_t,
                           std::size_t& count, bool& hasMore) override {
    count = 0U;
    hasMore = false;
    if (std::strcmp(path, "/") == 0) {
      if (offset == 0U) {
        setEntry(entries[0], "DOCS", true);
        setEntry(entries[1], "A.TXT", false);
        count = 2U;
        hasMore = true;
      } else {
        setEntry(entries[0], "LAST.TXT", false);
        count = 1U;
      }
      return fm::Result::Ok;
    }
    if (std::strcmp(path, "/DOCS") == 0) {
      setEntry(entries[0], "NOTE.TXT", false);
      count = 1U;
      return fm::Result::Ok;
    }
    return fm::Result::NotFound;
  }

  fm::Result copyFile(const char* source, const char* destination) override {
    copiedFrom = source;
    copiedTo = destination;
    return fm::Result::Ok;
  }

  fm::Result move(const char* source, const char* destination) override {
    movedFrom = source;
    movedTo = destination;
    return fm::Result::Ok;
  }

  fm::Result rename(const char* source, const char* destination) override {
    renamedFrom = source;
    renamedTo = destination;
    return fm::Result::Ok;
  }

  static void setEntry(fm::Entry& entry, const char* name, bool directory) {
    std::strcpy(entry.name, name);
    entry.size = 42U;
    entry.isDirectory = directory;
  }

  bool mounted = false;
  std::string copiedFrom;
  std::string copiedTo;
  std::string movedFrom;
  std::string movedTo;
  std::string renamedFrom;
  std::string renamedTo;
};

void navigationAndPaging() {
  FakeFileSystem fileSystem;
  fm::FileManagerController controller(fileSystem);

  assert(controller.start() == fm::Result::Ok);
  assert(fileSystem.mounted);
  assert(std::strcmp(controller.currentPath(), "/") == 0);
  assert(controller.entryCount() == 2U);

  assert(controller.activate(0U) == fm::Result::Ok);
  assert(std::strcmp(controller.currentPath(), "/DOCS") == 0);
  assert(controller.goBack() == fm::Result::Ok);
  assert(std::strcmp(controller.currentPath(), "/") == 0);

  assert(controller.nextPage() == fm::Result::Ok);
  assert(controller.pageOffset() == fm::kEntriesPerPage);
  assert(std::strcmp(controller.entries()[0].name, "LAST.TXT") == 0);
  assert(controller.previousPage() == fm::Result::Ok);
  assert(controller.pageOffset() == 0U);
}

void copyMoveAndRename() {
  FakeFileSystem fileSystem;
  fm::FileManagerController controller(fileSystem);
  assert(controller.start() == fm::Result::Ok);

  assert(controller.select(1U) == fm::Result::Ok);
  assert(controller.beginCopy() == fm::Result::Ok);
  assert(controller.activate(0U) == fm::Result::Ok);
  assert(controller.paste() == fm::Result::Ok);
  assert(fileSystem.copiedFrom == "/A.TXT");
  assert(fileSystem.copiedTo == "/DOCS/A.TXT");

  assert(controller.goBack() == fm::Result::Ok);
  assert(controller.select(1U) == fm::Result::Ok);
  assert(controller.beginMove() == fm::Result::Ok);
  assert(controller.activate(0U) == fm::Result::Ok);
  assert(controller.paste() == fm::Result::Ok);
  assert(fileSystem.movedFrom == "/A.TXT");
  assert(fileSystem.movedTo == "/DOCS/A.TXT");

  assert(controller.select(0U) == fm::Result::Ok);
  assert(controller.renameSelected("NEW.TXT") == fm::Result::Ok);
  assert(fileSystem.renamedFrom == "/DOCS/NOTE.TXT");
  assert(fileSystem.renamedTo == "/DOCS/NEW.TXT");
}

void rejectsInvalidOperations() {
  FakeFileSystem fileSystem;
  fm::FileManagerController controller(fileSystem);
  assert(controller.start() == fm::Result::Ok);
  assert(controller.beginCopy() == fm::Result::InvalidOperation);
  assert(controller.select(0U) == fm::Result::InvalidOperation);
  assert(controller.select(1U) == fm::Result::Ok);
  assert(controller.renameSelected("BAD/NAME") == fm::Result::InvalidName);
}

}  // namespace

int main() {
  navigationAndPaging();
  copyMoveAndRename();
  rejectsInvalidOperations();
  std::cout << "All file manager controller tests passed.\n";
  return 0;
}
