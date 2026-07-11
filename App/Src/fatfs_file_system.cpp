#include "fatfs_file_system.hpp"

#include <cstring>

namespace fm {

FatFsFileSystem::FatFsFileSystem()
    : fatFs_{}, mounted_(false), transferBuffer_{} {}

Result FatFsFileSystem::mount() {
  const FRESULT result = f_mount(&fatFs_, "0:", 1U);
  mounted_ = (result == FR_OK);
  return mapResult(result);
}

Result FatFsFileSystem::makeFatPath(const char* path, char* output,
                                    std::size_t outputSize) const {
  if ((path == nullptr) || (output == nullptr)) {
    return Result::InvalidOperation;
  }
  const std::size_t pathLength = std::strlen(path);
  if ((pathLength + 3U) > outputSize) {
    return Result::PathTooLong;
  }
  std::strcpy(output, "0:");
  std::strcat(output, path);
  return Result::Ok;
}

Result FatFsFileSystem::listDirectory(const char* path, std::size_t offset,
                                      Entry* entries, std::size_t capacity,
                                      std::size_t& count, bool& hasMore) {
  count = 0U;
  hasMore = false;
  if (!mounted_) {
    return Result::NotMounted;
  }

  char fatPath[kMaxPathLength + 4U] = {};
  Result pathResult = makeFatPath(path, fatPath, sizeof(fatPath));
  if (pathResult != Result::Ok) {
    return pathResult;
  }

  DIR directory = {};
  FRESULT result = f_opendir(&directory, fatPath);
  if (result != FR_OK) {
    return mapResult(result);
  }

  FILINFO info = {};
  char longName[kMaxNameLength + 1U] = {};
  info.lfname = longName;
  info.lfsize = sizeof(longName);
  std::size_t visibleIndex = 0U;

  for (;;) {
    longName[0] = '\0';
    result = f_readdir(&directory, &info);
    if ((result != FR_OK) || (info.fname[0] == '\0')) {
      break;
    }

    const char* name = (longName[0] != '\0') ? longName : info.fname;
    if ((std::strcmp(name, ".") == 0) || (std::strcmp(name, "..") == 0)) {
      continue;
    }
    if (visibleIndex++ < offset) {
      continue;
    }
    if (count >= capacity) {
      hasMore = true;
      break;
    }

    std::strncpy(entries[count].name, name, kMaxNameLength);
    entries[count].name[kMaxNameLength] = '\0';
    entries[count].size = info.fsize;
    entries[count].isDirectory = (info.fattrib & AM_DIR) != 0U;
    ++count;
  }

  const FRESULT closeResult = f_closedir(&directory);
  if (result != FR_OK) {
    return mapResult(result);
  }
  return mapResult(closeResult);
}

Result FatFsFileSystem::copyFile(const char* source,
                                 const char* destination) {
  if (!mounted_) {
    return Result::NotMounted;
  }

  char sourcePath[kMaxPathLength + 4U] = {};
  char destinationPath[kMaxPathLength + 4U] = {};
  Result result = makeFatPath(source, sourcePath, sizeof(sourcePath));
  if (result == Result::Ok) {
    result = makeFatPath(destination, destinationPath, sizeof(destinationPath));
  }
  if (result != Result::Ok) {
    return result;
  }

  FIL input = {};
  FIL output = {};
  FRESULT fsResult = f_open(&input, sourcePath, FA_READ);
  if (fsResult != FR_OK) {
    return mapResult(fsResult);
  }

  fsResult = f_open(&output, destinationPath, FA_WRITE | FA_CREATE_NEW);
  if (fsResult != FR_OK) {
    f_close(&input);
    return mapResult(fsResult);
  }

  bool complete = true;
  for (;;) {
    UINT bytesRead = 0U;
    UINT bytesWritten = 0U;
    fsResult = f_read(&input, transferBuffer_, sizeof(transferBuffer_), &bytesRead);
    if (fsResult != FR_OK) {
      complete = false;
      break;
    }
    if (bytesRead == 0U) {
      break;
    }
    fsResult = f_write(&output, transferBuffer_, bytesRead, &bytesWritten);
    if ((fsResult != FR_OK) || (bytesWritten != bytesRead)) {
      complete = false;
      break;
    }
  }

  const FRESULT inputClose = f_close(&input);
  const FRESULT outputClose = f_close(&output);
  if (!complete || (inputClose != FR_OK) || (outputClose != FR_OK)) {
    f_unlink(destinationPath);
    return (fsResult == FR_OK) ? Result::IoError : mapResult(fsResult);
  }
  return Result::Ok;
}

Result FatFsFileSystem::move(const char* source, const char* destination) {
  return rename(source, destination);
}

Result FatFsFileSystem::rename(const char* source, const char* destination) {
  if (!mounted_) {
    return Result::NotMounted;
  }
  char sourcePath[kMaxPathLength + 4U] = {};
  char destinationPath[kMaxPathLength + 4U] = {};
  Result result = makeFatPath(source, sourcePath, sizeof(sourcePath));
  if (result == Result::Ok) {
    result = makeFatPath(destination, destinationPath, sizeof(destinationPath));
  }
  if (result != Result::Ok) {
    return result;
  }
  return mapResult(f_rename(sourcePath, destinationPath));
}

Result FatFsFileSystem::mapResult(FRESULT result) {
  switch (result) {
    case FR_OK:
      return Result::Ok;
    case FR_NO_FILE:
    case FR_NO_PATH:
      return Result::NotFound;
    case FR_EXIST:
      return Result::AlreadyExists;
    case FR_INVALID_NAME:
      return Result::InvalidName;
    case FR_NOT_READY:
    case FR_NOT_ENABLED:
    case FR_NO_FILESYSTEM:
      return Result::NotMounted;
    default:
      return Result::IoError;
  }
}

}  // namespace fm
