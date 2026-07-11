#ifndef FILE_MANAGER_TYPES_HPP
#define FILE_MANAGER_TYPES_HPP

#include <cstddef>
#include <cstdint>

namespace fm {

constexpr std::size_t kMaxNameLength = 63U;
constexpr std::size_t kMaxPathLength = 127U;
constexpr std::size_t kEntriesPerPage = 5U;

enum class Result : std::uint8_t {
  Ok,
  NotMounted,
  NotFound,
  AlreadyExists,
  InvalidName,
  InvalidOperation,
  IoError,
  PathTooLong
};

struct Entry {
  char name[kMaxNameLength + 1U];
  std::uint32_t size;
  bool isDirectory;
};

enum class PendingOperation : std::uint8_t {
  None,
  Copy,
  Move
};

}  // namespace fm

#endif
