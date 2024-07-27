/*
 * FileName profile_io.h
 * Author jattle
 * Description: gperftools CPU Profile low level file reader,
 * will parse file and return raw data
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace fustsdk {

/// @brief binary header of gperftool generated cpu profile
struct CPUProfileBinaryHeader {
  size_t hdr_count{0};        //   0  header count (0; must be 0)
  size_t hdr_words{3};        //   1  header slots after this one (3; must be >= 3)
  size_t version{0};          //   2  format version (0; must be 0)
  size_t sampling_period{0};  //   3  sampling period, in microseconds
  size_t padding{0};          //   4  padding (0)
};

inline bool operator==(const CPUProfileBinaryHeader& l, const CPUProfileBinaryHeader& r) {
  return memcmp(&l, &r, sizeof(CPUProfileBinaryHeader)) == 0;
}

enum class UnpackType {
  kNone = 0,
  kLittleEndian = 1,
  kBigEndian = 2,
};

enum class ProfileAddressLen {
  kNone = 0,
  k64Bit = 1,
  k32Bit = 2,
};

constexpr size_t k32BitSize = 4;
constexpr size_t k64BitSize = 8;

enum class ReaderRetCode {
  kOK = 0,
  kInvalidStream = 1,
  kNotInited = 10,
  kEndOfFile = 11,
  kReadError = 12,
  kInvalidAddressLen = 13,
  kInvalidUnpackType = 14,
  kConvertErr = 15,
  kEmptyMapsText = 16,
};

class CPUProfileReader {
 public:
  explicit CPUProfileReader(const std::string& file);
  explicit CPUProfileReader(std::unique_ptr<std::istream> is);
  ~CPUProfileReader() = default;
  ReaderRetCode GetSlot(size_t index, size_t* val);
  /// @brief read content left in file
  ReaderRetCode ReadLeftContent(std::string* content);

 private:
  ReaderRetCode Init();
  ReaderRetCode NextSlot();
  template <size_t N>
  int ReadNextNChar(char (&buffer)[N]) {
    if (is_->read(buffer, N); !is_->good()) {
      if (is_->eof()) {
        return is_->gcount();
      } else {
        // error
        return -1;
      }
    }
    return is_->gcount();
  }
  bool Bit32Convert(char (&buffer)[k32BitSize], size_t* val);
  bool Bit64Convert(char (&buffer)[k64BitSize], size_t* val);

  std::string file_name_;
  std::unique_ptr<std::istream> is_;
  ReaderRetCode init_status_{ReaderRetCode::kNotInited};
  UnpackType unpack_type_{UnpackType::kNone};
  ProfileAddressLen address_len_{ProfileAddressLen::kNone};
  size_t hdr_count_{0};
  size_t hdr_words_{0};
  std::vector<size_t> slots_;
};

enum class WriterRetCode {
  kOK = 0,
  kNotInited = 20,
  kWriteError = 21,
  kConvertErr = 22,
  kInvalidStream = 23,
  kInvalidAddrLen = 24,
};

struct CPUProfileMetaData {
  UnpackType unpack_type{UnpackType::kLittleEndian};
  ProfileAddressLen address_len{ProfileAddressLen::k64Bit};
};

class CPUProfileWriter {
 public:
  CPUProfileWriter(std::shared_ptr<std::ostream> os, const CPUProfileBinaryHeader& header,
                   const CPUProfileMetaData& meta = CPUProfileMetaData{})
      : os_(std::move(os)), header_(header), meta_(meta) {
    Init();
  }
  CPUProfileWriter(const std::string& file, const CPUProfileBinaryHeader& header,
                   const CPUProfileMetaData& meta = CPUProfileMetaData{}) {
    os_ = std::make_unique<std::ofstream>(file, std::ofstream::binary);
    header_ = header;
    meta_ = meta;
    Init();
  }
  ~CPUProfileWriter() = default;
  WriterRetCode AppendSlot(size_t val);
  WriterRetCode AppendMapsText(const std::string& text);

 private:
  WriterRetCode Init();
  bool Bit32Convert(size_t val, char (&buffer)[k32BitSize]);
  bool Bit64Convert(size_t val, char (&buffer)[k64BitSize]);
  template <size_t N>
  int WriteNextNChar(char (&buffer)[N]) {
    if (os_->write(buffer, N); !os_->good()) {
      return -1;
    }
    return N;
  }

  std::shared_ptr<std::ostream> os_;
  CPUProfileBinaryHeader header_;
  CPUProfileMetaData meta_;
  WriterRetCode init_status_{WriterRetCode::kNotInited};
  std::string error_msg_;
};

}  // namespace fustsdk
