/*
 * FileName profile_io.cc
 * Author jattle
 * Description:
 */
#include "profiling/io/profile_io.h"

#include <cassert>
#include <cstring>

#include "profiling/util/endian.h"

namespace pprofcpp {

CPUProfileReader::CPUProfileReader(const std::string& file) : file_name_(file) {
  this->is_ = std::make_unique<std::ifstream>(file.c_str(), std::ios_base::binary);
  Init();
}

CPUProfileReader::CPUProfileReader(std::unique_ptr<std::istream> is) {
  this->is_ = std::move(is);
  Init();
}

ReaderRetCode CPUProfileReader::GetSlot(size_t index, size_t* val) {
  if (index >= slots_.size()) {
    // try next slot
    if (status_ != ReaderRetCode::kOK) {
      return status_;
    }
    do {
      if (auto ret = NextSlot(); ret != 0) {
        return status_;
      }
    } while (slots_.size() <= index);
  }
  *val = slots_.at(index);
  return ReaderRetCode::kOK;
}

void CPUProfileReader::Init() {
  // reference: https://github.com/gperftools/gperftools/blob/master/docs/cpuprofile-fileformat.html
  /// Binary Header Format
  /// slot    data
  ///  0  header count (0; must be 0)
  ///  1  header slots after this one (3; must be >= 3)
  ///  2  format version (0; must be 0)
  ///  3  sampling period, in microseconds
  ///  4  padding (0)

  // 以下只解析header的前两个slot，对于32位address len，每个Slot占用4字节，对于64位address len，每个Slot占用8字节
  // 首先根据第一个Slot确定address len，再根据第二个Slot判断pack类型是big endian还是little endian
  if (!this->is_->good()) {
    error_msg_ = "open file " + file_name_ + " failed: " + strerror(errno);
    status_ = ReaderRetCode::kInvalidStream;
    return;
  }
  char buffer[8] = {0};
  if (ReadNextNChar<sizeof(buffer)>(buffer) != sizeof(buffer)) {
    return;
  }
  if (*reinterpret_cast<uint64_t*>(buffer) == 0) {
    address_len_ = ProfileAddressLen::k64Bit;
  } else {
    address_len_ = ProfileAddressLen::k32Bit;
  }
  if (address_len_ == ProfileAddressLen::k64Bit) {
    // 读取第二个slot(hdr_words)判断
    if (ReadNextNChar<sizeof(buffer)>(buffer) != sizeof(buffer)) {
      return;
    }
    if (*reinterpret_cast<uint32_t*>(&buffer[0]) == 0) {
      unpack_type_ = UnpackType::kBigEndian;
      hdr_words_ = be64toh(*reinterpret_cast<uint64_t*>(buffer));
    } else if (*reinterpret_cast<uint32_t*>(&buffer[4]) == 0) {
      unpack_type_ = UnpackType::kLittleEndian;
      hdr_words_ = le64toh(*reinterpret_cast<uint64_t*>(buffer));
    } else {
      status_ = ReaderRetCode::kInvalidUnpackType;
      return;
    }
  } else if (address_len_ == ProfileAddressLen::k32Bit) {
    // 第二个4字节是hdr_words
    // 4,5,6,7
    if (*reinterpret_cast<uint16_t*>(&buffer[4]) == 0) {
      unpack_type_ = UnpackType::kBigEndian;
      hdr_words_ = be32toh(*reinterpret_cast<uint32_t*>(&buffer[4]));
    } else if (*reinterpret_cast<uint16_t*>(&buffer[6]) == 0) {
      // 4,5存储了实际值
      unpack_type_ = UnpackType::kLittleEndian;
      hdr_words_ = le32toh(*reinterpret_cast<uint32_t*>(&buffer[4]));
    } else {
      status_ = ReaderRetCode::kInvalidUnpackType;
      return;
    }
  }
  slots_.emplace_back(0);
  slots_.emplace_back(hdr_words_);
  status_ = ReaderRetCode::kOK;
  return;
}

bool CPUProfileReader::Bit32Convert(char buffer[4], size_t* val) {
  switch (unpack_type_) {
    case UnpackType::kLittleEndian:
      *val = le32toh(*reinterpret_cast<uint32_t*>(buffer));
      return true;
    case UnpackType::kBigEndian:
      *val = be32toh(*reinterpret_cast<uint32_t*>(buffer));
      return true;
    default:
      return false;
  }
}

bool CPUProfileReader::Bit64Convert(char buffer[8], size_t* val) {
  switch (unpack_type_) {
    case UnpackType::kLittleEndian:
      *val = le64toh(*reinterpret_cast<uint64_t*>(buffer));
      return true;
    case UnpackType::kBigEndian:
      *val = be64toh(*reinterpret_cast<uint64_t*>(buffer));
      return true;
    default:
      return false;
  }
}

int CPUProfileReader::NextSlot() {
  if (address_len_ == ProfileAddressLen::k32Bit) {
    char buffer[4];
    if (auto ret = ReadNextNChar<sizeof(buffer)>(buffer); ret != sizeof(buffer)) {
      return -1;
    }
    size_t val{0};
    if (Bit32Convert(buffer, &val)) {
      slots_.emplace_back(val);
      return 0;
    }
    status_ = ReaderRetCode::kConvertErr;
    return -1;
  }
  if (address_len_ == ProfileAddressLen::k64Bit) {
    char buffer[8];
    if (auto ret = ReadNextNChar<sizeof(buffer)>(buffer); ret != sizeof(buffer)) {
      return -1;
    }
    size_t val{0};
    if (Bit64Convert(buffer, &val)) {
      slots_.emplace_back(val);
      return 0;
    }
    status_ = ReaderRetCode::kConvertErr;
    return -1;
  }
  status_ = ReaderRetCode::kInvalidAddressLen;
  // unexpected address len
  return -1;
}

/// @brief read content left in file
ReaderRetCode CPUProfileReader::ReadLeftContent(std::string* content) {
  char buffer[1024] = {0};
  size_t bytes{0};
  while (bytes = ReadNextNChar<sizeof(buffer)>(buffer), bytes >= 0) {
    if (bytes > 0) {
      content->append(buffer, buffer + bytes);
    }
    if (bytes != sizeof(buffer)) {
      break;
    }
  }
  return status_;
}

void CPUProfileWriter::Init() {
  if (os_->fail()) {
    error_msg_ = std::string("invalid ostream: ") + strerror(errno);
    return;
  }
  // writer binary header
  AppendSlot(header_.hdr_count);
  AppendSlot(header_.hdr_words);
  AppendSlot(header_.version);
  AppendSlot(header_.sampling_period);
  AppendSlot(header_.padding);
}

bool CPUProfileWriter::Bit32Convert(size_t val, char buffer[4]) {
  uint32_t v{0};
  switch (meta_.unpack_type) {
    case UnpackType::kLittleEndian:
      v = htole32(val);
      break;
    case UnpackType::kBigEndian:
      v = htobe32(val);
      break;
    default:
      return false;
  }
  memcpy(buffer, &v, 4);
  return true;
}

bool CPUProfileWriter::Bit64Convert(size_t val, char buffer[8]) {
  uint64_t v{0};
  switch (meta_.unpack_type) {
    case UnpackType::kLittleEndian:
      v = htole64(val);
      break;
    case UnpackType::kBigEndian:
      v = htobe64(val);
      break;
    default:
      return false;
  }
  memcpy(buffer, &v, 8);
  return true;
}

WriterRetCode CPUProfileWriter::AppendSlot(size_t val) {
  if (meta_.address_len == ProfileAddressLen::k32Bit) {
    char buffer[4];
    if (!Bit32Convert(val, buffer)) {
      return WriterRetCode::kWriteError;
    }
    WriteNextNChar<4>(buffer);
  } else if (meta_.address_len == ProfileAddressLen::k64Bit) {
    char buffer[8];
    if (!Bit64Convert(val, buffer)) {
      return WriterRetCode::kWriteError;
    }
    WriteNextNChar<8>(buffer);
  }
  return status_;
}

WriterRetCode CPUProfileWriter::AppendMapsText(const std::string& text) {
  os_->write(text.data(), text.size());
  if (os_->good()) {
    return WriterRetCode::kOK;
  }
  status_ = WriterRetCode::kWriteError;
  error_msg_ = strerror(errno);
  return status_;
}

}  // namespace pprofcpp
