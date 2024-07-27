/*
 * FileName profie_io_test.cc
 * Author jattle
 * Description:
 */
#include <memory>

#include "profiling/io/profile_io.h"

#include "gtest/gtest.h"

constexpr char kCPUProfileSample[] = "./profiling/io/cpu_profile_sample";

using namespace fustsdk;

TEST(CPUProfileReader, InvalidFile) {
  CPUProfileReader reader("file_not_exists");
  size_t val;
  EXPECT_EQ(reader.GetSlot(0, &val), ReaderRetCode::kInvalidStream);
}

TEST(CPUProfileReader, BinaryHeader) {
  CPUProfileReader reader(kCPUProfileSample);
  EXPECT_EQ(reader.init_status_, ReaderRetCode::kOK);
  EXPECT_EQ(reader.unpack_type_, UnpackType::kLittleEndian);
  EXPECT_EQ(reader.address_len_, ProfileAddressLen::k64Bit);
  // 0: header count, 1: hdr_words, 2: format version, 3: sampling period(in microseconds), 4: padding
  size_t val{0};
  EXPECT_EQ(reader.GetSlot(0, &val), ReaderRetCode::kOK);
  EXPECT_EQ(val, 0);
  EXPECT_EQ(reader.GetSlot(1, &val), ReaderRetCode::kOK);
  EXPECT_GE(val, 3);
  EXPECT_EQ(reader.GetSlot(2, &val), ReaderRetCode::kOK);
  EXPECT_EQ(val, 0);
  EXPECT_EQ(reader.GetSlot(3, &val), ReaderRetCode::kOK);
  EXPECT_EQ(val, 10000);
  EXPECT_EQ(reader.GetSlot(4, &val), ReaderRetCode::kOK);
  EXPECT_EQ(val, 0);
}

TEST(CPUProfileReader, ProfileRecord) {
  CPUProfileReader reader(kCPUProfileSample);
  EXPECT_EQ(reader.init_status_, ReaderRetCode::kOK);
  // skip to slot 5
  size_t val{0};
  // sample count
  EXPECT_EQ(reader.GetSlot(5, &val), ReaderRetCode::kOK);
  EXPECT_GE(val, 1);
  // number of call chain PCs (num_pcs), must be >= 1
  EXPECT_EQ(reader.GetSlot(6, &val), ReaderRetCode::kOK);
  EXPECT_GE(val, 1);
  EXPECT_EQ(reader.GetSlot(7, &val), ReaderRetCode::kOK);
  // address
  EXPECT_GE(val, 0);
}

TEST(CPUProfileReader, ReadLeftContent) {
  CPUProfileReader reader(kCPUProfileSample);
  EXPECT_EQ(reader.init_status_, ReaderRetCode::kOK);
  std::string content;
  auto st = reader.ReadLeftContent(&content);
  EXPECT_EQ(st, ReaderRetCode::kEndOfFile);
  EXPECT_TRUE(!content.empty());
}

void WriteThenRead(const CPUProfileMetaData& meta) {
  // serialize
  std::shared_ptr<std::ostream> os = std::make_shared<std::stringstream>();
  CPUProfileBinaryHeader header;
  header.sampling_period = 1000;
  CPUProfileWriter writer{os, header, meta};
  EXPECT_EQ(writer.init_status_, WriterRetCode::kOK);
  // sample_count, num_pc, pc, call_ptr...
  std::vector<size_t> stack1{10, 4, 0x1, 0x20, 0x30, 0x40};
  for (const auto& item : stack1) {
    EXPECT_EQ(writer.AppendSlot(item), WriterRetCode::kOK);
  }
  // end flag
  std::vector<size_t> stack2{1, 1, 0};
  for (const auto& item : stack2) {
    EXPECT_EQ(writer.AppendSlot(item), WriterRetCode::kOK);
  }
  std::string text{"build=/path/to/binary\n40000000-40015000 r-xp 00000000 03:01 12845071   /lib/ld-2.3.2.so\n"};
  EXPECT_EQ(writer.AppendMapsText(text), WriterRetCode::kOK);
  // deserialize
  std::string profile = static_cast<std::stringstream*>(os.get())->str();
  std::unique_ptr<std::istream> is = std::make_unique<std::istringstream>(profile);
  CPUProfileReader reader{std::move(is)};
  EXPECT_EQ(reader.init_status_, ReaderRetCode::kOK);
  EXPECT_EQ(reader.unpack_type_, meta.unpack_type);
  EXPECT_EQ(reader.address_len_, meta.address_len);
  CPUProfileBinaryHeader header1;
  size_t index{0};
  reader.GetSlot(index++, &header1.hdr_count);
  reader.GetSlot(index++, &header1.hdr_words);
  reader.GetSlot(index++, &header1.version);
  reader.GetSlot(index++, &header1.sampling_period);
  reader.GetSlot(index++, &header1.padding);
  EXPECT_EQ(header, header1);
  std::vector<size_t> stack;
  size_t pc{0};
  while (true) {
    size_t sampling_cout{0}, num_pc{0}, val{0};
    reader.GetSlot(index++, &sampling_cout);
    reader.GetSlot(index++, &num_pc);
    reader.GetSlot(index++, &pc);
    stack = {sampling_cout, num_pc, pc};
    if (pc == 0) {
      EXPECT_EQ(stack, stack2);
      break;
    }
    for (size_t i = 1; i < num_pc; i++) {
      reader.GetSlot(index++, &val);
      stack.emplace_back(val);
    }
    EXPECT_EQ(stack, stack1);
  }
  std::string left_content;
  EXPECT_EQ(reader.ReadLeftContent(&left_content), ReaderRetCode::kEndOfFile);
  EXPECT_EQ(left_content, text);
}

TEST(CPUProfileWriter, DefaultMeta) {
  CPUProfileMetaData meta;
  WriteThenRead(meta);
}

TEST(CPUProfileWriter, Meta1) {
  CPUProfileMetaData meta;
  meta.address_len = ProfileAddressLen::k32Bit;
  meta.unpack_type = UnpackType::kBigEndian;
  WriteThenRead(meta);
}

TEST(CPUProfileWriter, Meta2) {
  CPUProfileMetaData meta;
  meta.address_len = ProfileAddressLen::k32Bit;
  meta.unpack_type = UnpackType::kLittleEndian;
  WriteThenRead(meta);
}

TEST(CPUProfileWriter, Meta3) {
  CPUProfileMetaData meta;
  meta.address_len = ProfileAddressLen::k64Bit;
  meta.unpack_type = UnpackType::kBigEndian;
  WriteThenRead(meta);
}
