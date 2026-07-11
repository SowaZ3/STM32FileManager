#ifndef FATFS_FILE_SYSTEM_HPP
#define FATFS_FILE_SYSTEM_HPP

#include "i_file_system.hpp"

extern "C" {
#include "ff.h"
}

namespace fm {

class FatFsFileSystem final : public IFileSystem {
 public:
  FatFsFileSystem();

  Result mount() override;
  Result listDirectory(const char* path, std::size_t offset,
                       Entry* entries, std::size_t capacity,
                       std::size_t& count, bool& hasMore) override;
  Result copyFile(const char* source, const char* destination) override;
  Result move(const char* source, const char* destination) override;
  Result rename(const char* source, const char* destination) override;

 private:
  Result makeFatPath(const char* path, char* output,
                     std::size_t outputSize) const;
  static Result mapResult(FRESULT result);

  FATFS fatFs_;
  bool mounted_;
  std::uint8_t transferBuffer_[512U];
};

}  // namespace fm

#endif
