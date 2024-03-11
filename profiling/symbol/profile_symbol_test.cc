/*
 * FileName profile_symbol_test.cc
 * Author jattle
 * Description:
 */
#include <mutex>
#include <string>
#include <vector>

#include "profiling/symbol/profile_symbol.h"

#include "gtest/gtest.h"

using namespace pprofcpp;

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

void* IntToPtrAddr(uintptr_t addr) { return reinterpret_cast<void*>(addr); }

TEST(BfdSymbolLocator, FindMatchedLib) {
  BfdSymbolLocator locator;
  locator.dyn_mappings_ = PackDynLibMappings();
  {
    BfdSymbolLocator::FileMatchMeta meta;
    meta.address = IntToPtrAddr(0x102);
    bool found = locator.FindMatchedLib(&meta);
    EXPECT_TRUE(found);
    EXPECT_EQ(meta.base, reinterpret_cast<void*>(0x100));
    EXPECT_EQ(meta.file, kLib1);
  }
  {
    BfdSymbolLocator::FileMatchMeta meta;
    meta.address = IntToPtrAddr(0x600);
    bool found = locator.FindMatchedLib(&meta);
    EXPECT_FALSE(found);
    EXPECT_EQ(meta.base, nullptr);
    EXPECT_EQ(meta.file, "");
  }
}

TEST(BfdSymbolLocator, SearchSymbol) {
  BfdSymbolLocator locator;
  locator.dyn_mappings_ = PackDynLibMappings();
  void* addr = reinterpret_cast<void*>(0x7ffffffffff10001);
  SymbolInfo sym_info;
  LocatorStatus st = locator.SearchSymbol(addr, &sym_info);
  fprintf(stderr, "addr: %p, symbol: %s\n", sym_info.address, sym_info.symbol_name.c_str());
  EXPECT_EQ(st.ret, LocatorRetCode::kSymbolNotFound);
}
