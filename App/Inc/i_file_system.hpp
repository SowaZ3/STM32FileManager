#ifndef I_FILE_SYSTEM_HPP
#define I_FILE_SYSTEM_HPP

#include "file_manager_types.hpp"

namespace fm {

class IFileSystem {
 public:
  virtual ~IFileSystem() = default;

  virtual Result mount() = 0;
  virtual Result listDirectory(const char* path, std::size_t offset,
                               Entry* entries, std::size_t capacity,
                               std::size_t& count, bool& hasMore) = 0;
  virtual Result copyFile(const char* source, const char* destination) = 0;
  virtual Result move(const char* source, const char* destination) = 0;
  virtual Result rename(const char* source, const char* destination) = 0;
};

}  // namespace fm

#endif
