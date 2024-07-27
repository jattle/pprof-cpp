/*
 * FileName profile_io.cc
 * Author jattle
 * Description:
 */
#include "profiling/io/profile_io.h"

#include "profiling/util/endian.h"

#include <cassert>
#include <cstring>

namespace fustsdk {

CPUProfileReader::CPUProfileReader(const std::string& file) : file_name_(file) {
  this->is_ = std::make_unique<std::ifstream>(file.c_str(), std::ios_base::binary);
  this->init_status_ = Init();
}

CPUProfileReader::CPUProfileReader(std::unique_ptr<std::istream> is) {
  this->is_ = std::move(is);
  this->init_status_ = Init();
}

ReaderRetCode CPUProfileReader::GetSlot(size_t index, size_t* val) {
  if (init_status_ != ReaderRetCode::kOK) {
    return init_status_;
  }
  if (index >= slots_.size()) {
    do {
      if (auto ret = NextSlot(); ret != ReaderRetCode::kOK) {
        return ret;
      }
    } while (slots_.size() <= index);
  }
  *val = slots_.at(index);
  return ReaderRetCode::kOK;
}

ReaderRetCode CPUProfileReader::Init() {
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
    this->init_status_ = ReaderRetCode::kInvalidStream;
    return ReaderRetCode::kInvalidStream;
  }
  char buffer[8] = {0};
  if (ReadNextNChar<sizeof(buffer)>(buffer) != sizeof(buffer)) {
    return ReaderRetCode::kReadError;
  }
  if (*reinterpret_cast<uint64_t*>(buffer) == 0) {
    address_len_ = ProfileAddressLen::k64Bit;
  } else {
    address_len_ = ProfileAddressLen::k32Bit;
  }
  if (address_len_ == ProfileAddressLen::k64Bit) {
    // 读取第二个slot(hdr_words)判断
    if (ReadNextNChar<sizeof(buffer)>(buffer) != sizeof(buffer)) {
      return ReaderRetCode::kReadError;
    }
    if (*reinterpret_cast<uint32_t*>(&buffer[0]) == 0) {
      unpack_type_ = UnpackType::kBigEndian;
      hdr_words_ = be64toh(*reinterpret_cast<uint64_t*>(buffer));
    } else if (*reinterpret_cast<uint32_t*>(&buffer[4]) == 0) {
      unpack_type_ = UnpackType::kLittleEndian;
      hdr_words_ = le64toh(*reinterpret_cast<uint64_t*>(buffer));
    } else {
      return ReaderRetCode::kInvalidUnpackType;
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
      return ReaderRetCode::kInvalidUnpackType;
    }
  }
  slots_.emplace_back(0);
  slots_.emplace_back(hdr_words_);
  this->init_status_ = ReaderRetCode::kOK;
  return ReaderRetCode::kOK;
}

bool CPUProfileReader::Bit32Convert(char (&buffer)[k32BitSize], size_t* val) {
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

bool CPUProfileReader::Bit64Convert(char (&buffer)[k64BitSize], size_t* val) {
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

ReaderRetCode CPUProfileReader::NextSlot() {
  if (address_len_ == ProfileAddressLen::k32Bit) {
    char buffer[k32BitSize];
    if (auto ret = ReadNextNChar<sizeof(buffer)>(buffer); ret != sizeof(buffer)) {
      return ReaderRetCode::kReadError;
    }
    size_t val{0};
    if (Bit32Convert(buffer, &val)) {
      slots_.emplace_back(val);
      return ReaderRetCode::kOK;
    }
    return ReaderRetCode::kConvertErr;
  }
  if (address_len_ == ProfileAddressLen::k64Bit) {
    char buffer[k64BitSize];
    if (auto ret = ReadNextNChar<sizeof(buffer)>(buffer); ret != sizeof(buffer)) {
      return ReaderRetCode::kReadError;
    }
    size_t val{0};
    if (Bit64Convert(buffer, &val)) {
      slots_.emplace_back(val);
      return ReaderRetCode::kOK;
    }
    return ReaderRetCode::kConvertErr;
  }
  // unexpected address len
  return ReaderRetCode::kInvalidAddressLen;
}

/// @brief read content left in file
ReaderRetCode CPUProfileReader::ReadLeftContent(std::string* content) {
  char buffer[1024] = {0};
  size_t bytes{0};
  while (bytes = ReadNextNChar<sizeof(buffer)>(buffer), bytes >= 0) {
    if (bytes > 0) {
      content->append(buffer, buffer + bytes);
    }
    if (bytes == 0) {
      return ReaderRetCode::kEndOfFile;
    }
  }
  return ReaderRetCode::kReadError;
}

WriterRetCode CPUProfileWriter::Init() {
  if (os_->fail()) {
    return WriterRetCode::kInvalidStream;
  }
  // writer binary header
  AppendSlot(header_.hdr_count);
  AppendSlot(header_.hdr_words);
  AppendSlot(header_.version);
  AppendSlot(header_.sampling_period);
  AppendSlot(header_.padding);
  this->init_status_ = WriterRetCode::kOK;
  return WriterRetCode::kOK;
}

bool CPUProfileWriter::Bit32Convert(size_t val, char (&buffer)[k32BitSize]) {
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
  memcpy(buffer, &v, sizeof(buffer));
  return true;
}

bool CPUProfileWriter::Bit64Convert(size_t val, char (&buffer)[k64BitSize]) {
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
  memcpy(buffer, &v, sizeof(buffer));
  return true;
}

WriterRetCode CPUProfileWriter::AppendSlot(size_t val) {
  if (meta_.address_len == ProfileAddressLen::k32Bit) {
    char buffer[k32BitSize];
    if (!Bit32Convert(val, buffer)) {
      return WriterRetCode::kConvertErr;
    }
    return WriteNextNChar<k32BitSize>(buffer) == k32BitSize ? WriterRetCode::kOK : WriterRetCode::kWriteError;
  } else if (meta_.address_len == ProfileAddressLen::k64Bit) {
    char buffer[k64BitSize];
    if (!Bit64Convert(val, buffer)) {
      return WriterRetCode::kConvertErr;
    }
    return WriteNextNChar<k64BitSize>(buffer) == k64BitSize ? WriterRetCode::kOK : WriterRetCode::kWriteError;
  }
  return WriterRetCode::kInvalidAddrLen;
}

WriterRetCode CPUProfileWriter::AppendMapsText(const std::string& text) {
  os_->write(text.data(), text.size());
  return os_->good() ? WriterRetCode::kOK : WriterRetCode::kWriteError;
}

}  // namespace fustsdk
