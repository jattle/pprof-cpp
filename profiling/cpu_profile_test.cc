/*
 * FileName: cpu_profile_test.cc
 * Author: jattle
 * Descrption:
 */
#include "profiling/cpu_profile.h"
#include "profiling/symbol/profile_symbol.h"

#include "gtest/gtest.h"

using namespace pprofcpp;

constexpr char kCPUProfileSample[] = "./profiling/io/cpu_profile_sample";

static constexpr char kLib1[] = {"/usr/lib64/lib1.so"};
static constexpr char kLib2[] = {"/usr/lib64/lib2.so"};

DynamicLibMappings PackDynLibMappings() {
  ProcMapItem item1{
      .start_addr = 0x100,
      .end_addr = 0x200,
  };
  ProcMapItem item2{
      .start_addr = 0x200,
      .end_addr = 0x300,
  };
  ProcMapItem item3{
      .start_addr = 0x300,
      .end_addr = 0x400,
  };
  ProcMapItem item4{
      .start_addr = 0x400,
      .end_addr = 0x500,
  };
  ProcLibMapping lib1{
      .inode = 100,
      .path = kLib1,
      .base = 0x100,
      .upper_bound = 0x300,
      .items = {item1, item2},
  };
  ProcLibMapping lib2{
      .inode = 200,
      .path = kLib2,
      .base = 0x300,
      .upper_bound = 0x500,
      .items = {item3, item4},
  };
  DynamicLibMappings dyn_libs;
  dyn_libs.lower_bound_ = 0x100;
  dyn_libs.upper_bound_ = 0x500;
  dyn_libs.lib_mappings_ = {lib1, lib2};
  return dyn_libs;
}

TEST(CPUProfile, Parse) {
  CPUProfile profile{kCPUProfileSample};
  auto st = profile.Parse();
  EXPECT_EQ(st, ReaderRetCode::kOK);
  EXPECT_EQ(profile.binary_header_.hdr_count, 0);
  EXPECT_GE(profile.binary_header_.hdr_words, 3);
  EXPECT_EQ(profile.binary_header_.padding, 0);
  EXPECT_GT(profile.record_num_, 0);
  EXPECT_GT(profile.ptr_num_, 0);
  EXPECT_FALSE(profile.stacks_.empty());
  EXPECT_FALSE(profile.proc_maps_items_.empty());
}

TEST(CPUProfile, GenerateRawProfile) {
  CPUProfile profile{kCPUProfileSample};
  auto st = profile.Parse();
  EXPECT_EQ(st, ReaderRetCode::kOK);
  BfdSymbolLocator locator;
  locator.dyn_mappings_ = PackDynLibMappings();
  std::string profile_content;
  RawProfileMeta meta;
  meta.profile_type = RawProfileType::kPProfCompatible;
  meta.program_path = "./fustcpp";
  auto ret = profile.GenerateRawProfile(meta, &locator, &profile_content);
  EXPECT_EQ(ret, CPUProfileRetCode::kOK);
  EXPECT_TRUE(profile_content.find("--- symbol\n") != std::string::npos);
  EXPECT_TRUE(profile_content.find("binary=./fustcpp\n") != std::string::npos);
  meta.profile_type = RawProfileType::kFixedRaw;
  ret = profile.GenerateRawProfile(meta, &locator, &profile_content);
  EXPECT_EQ(ret, CPUProfileRetCode::kOK);
  EXPECT_TRUE(profile_content.find("--- symbol_fixed\n") != std::string::npos);
  EXPECT_TRUE(profile_content.find("binary=./fustcpp\n") != std::string::npos);
}

TEST(CPUProfile, ParseMapsText) {
  CPUProfile profile{kCPUProfileSample};
  std::string text{"build=/path/to/binary\n40000000-40015000 r-xp 00000000 03:01 12845071   /lib/ld-2.3.2.so\n"};
  EXPECT_EQ(0, profile.ParseMapsText(text));
  EXPECT_EQ(profile.proc_maps_items_.size(), 1u);
  profile.proc_maps_items_.clear();
  text = {"build=/path/to/binary\n40000000-40015000 r-xp 00000000 03:01 12845071   /$build/lib/ld-2.3.2.so\n"};
  EXPECT_EQ(0, profile.ParseMapsText(text));
  EXPECT_EQ(profile.proc_maps_items_.size(), 1u);
  EXPECT_TRUE(profile.proc_maps_items_.at(0).find("/path/to/binary") != std::string::npos);
}

TEST(CPUProfile, ReplaceBuildSpecifier) {
  std::string pat = "$build";
  std::string target = "/data/binary";
  std::string line = "$buildA/assss/ddded";
  CPUProfile::ReplaceBuildSpecifier(pat, target, line);
  // no change
  EXPECT_EQ(line, "$buildA/assss/ddded");
  line = "$build|ss/assss/ddded";
  CPUProfile::ReplaceBuildSpecifier(pat, target, line);
  EXPECT_EQ(line, "/data/binary|ss/assss/ddded");
}

TEST(CPUProfile, ReParse) {
  CPUProfile profile{kCPUProfileSample};
  auto st = profile.Parse();
  EXPECT_EQ(st, ReaderRetCode::kOK);
  BfdSymbolLocator locator;
  locator.dyn_mappings_ = PackDynLibMappings();
  std::string profile_content;
  RawProfileMeta meta;
  meta.profile_type = RawProfileType::kFixedRaw;
  meta.program_path = "./fustcpp";
  auto ret = profile.GenerateRawProfile(meta, &locator, &profile_content);
  EXPECT_EQ(ret, CPUProfileRetCode::kOK);
  const std::string kProfileMarker{"--- profile\n"};
  std::string raw_profile = profile_content.substr(profile_content.find(kProfileMarker) + kProfileMarker.length());
  {
    std::unique_ptr<std::istream> is = std::make_unique<std::istringstream>(raw_profile);
    CPUProfile tmp_profile{std::move(is)};
    auto st = tmp_profile.Parse();
    EXPECT_EQ(st, ReaderRetCode::kEmptyMapsText);
    EXPECT_EQ(tmp_profile.binary_header_, profile.binary_header_);
    EXPECT_EQ(tmp_profile.record_num_, profile.record_num_);
    EXPECT_EQ(tmp_profile.ptr_num_, profile.ptr_num_);
    EXPECT_EQ(tmp_profile.stacks_, profile.stacks_);
    // no proc map items
    EXPECT_TRUE(tmp_profile.proc_maps_items_.empty());
  }
}
