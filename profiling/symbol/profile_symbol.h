/*
 * FileName profile_symbol.h
 * Author jattle
 * Description: provide following utils for addr symbol  mapping:
 * gperftools CPU Profile parsing and raw profile generating,
 * Bfd based runtime addr to symbol locating
 */
#pragma once

#include "bfd.h"

#include <link.h>
#include <functional>
#include <shared_mutex>
#include <string>
#include <vector>

namespace pprofcpp {


/// @brief simple symbol info consists of address and symbol_name
struct SymbolInfo {
  const void* address{nullptr};
  std::string symbol_name;  // equivalent to demangled function name now
};

enum class LocatorRetCode {
  kOK = 0,
  kOpenFileFailed = 1,
  kCheckFormatErr = 2,
  kNoSymbols = 3,
  kReadSymbolsFailed = 4,
  kNoMatchedFile = 5,
  kSymbolNotFound = 6,
  kNoAddr = 7,
};

struct LocatorStatus {
  explicit LocatorStatus(LocatorRetCode code, std::string&& msg) {
    this->ret = code;
    this->err = std::move(msg);
  }
  LocatorRetCode ret{LocatorRetCode::kOK};
  std::string err;
};

/// @brief symobl locator interface
class SymbolLocator {
 public:
  SymbolLocator() = default;
  virtual ~SymbolLocator() = default;
  virtual LocatorStatus SearchSymbols(const std::vector<void*>& addrs,
                                      std::unordered_map<void*, SymbolInfo>* sym_mapping) = 0;
};

/// @brief lib mapping item
struct ProcMapItem {
  uintptr_t start_addr{0};
  uintptr_t end_addr{0};
  char perms[5];
  uint64_t offset{0};
  int dev_major{0};
  int dev_minor{0};
};

/// @brief single lib mapping, may contains serveral mapping item
struct ProcLibMapping {
  int inode{0};
  std::string path;          // file path
  uintptr_t base{0};         // load base addr(lowest addr of all items)
  uintptr_t upper_bound{0};  // highest addr of all items
  std::vector<ProcMapItem> items;
};

/// @brief dynamic lib mappings for current running process
class DynamicLibMappings {
 public:
  DynamicLibMappings() = default;
  ~DynamicLibMappings() = default;
  // @brief find matched lib specified addr belongs to
  bool FindMatchedLib(const void* addr, ProcLibMapping* lib_mapping);
  // @brief parse proc lib mapping from mapping content(dump content of /proc/xxx/maps)
  int ParseProcMaps(const std::string& proc_mapping_content);
  // @brief get distinct lib paths loaded by the program
  bool GetLibPaths(std::vector<std::string>* paths);

 private:
  uintptr_t lower_bound_{UINTPTR_MAX};        // lowest addr
  uintptr_t upper_bound_{0};                  // highest addr
  std::vector<ProcLibMapping> lib_mappings_;  // dependent dynamic libs
};

/// @brief bfd object file info accessor wrapper
struct BfdAccessor {
  BfdAccessor() = default;
  BfdAccessor(BfdAccessor&& rhs) { MoveData(std::move(rhs)); }
  BfdAccessor& operator=(BfdAccessor&& rhs) {
    MoveData(std::move(rhs));
    return *this;
  }
  ~BfdAccessor() {
    if (bfd_ptr != nullptr) {
      bfd_close(bfd_ptr);
      free(mini_syms);
    }
    bfd_ptr = nullptr;
    mini_syms = nullptr;
    sym_count = 0;
  }
  bfd* bfd_ptr{nullptr};         // object file accessor pointer
  asymbol** mini_syms{nullptr};  // bfd symbol table pointer
  int sym_count{0};              // loaded symbol count

 private:
  void MoveData(BfdAccessor&& rhs) {
    this->bfd_ptr = rhs.bfd_ptr;
    this->mini_syms = rhs.mini_syms;
    this->sym_count = rhs.sym_count;
    rhs.bfd_ptr = nullptr;
    rhs.mini_syms = nullptr;
    rhs.sym_count = 0;
  }
  BfdAccessor(const BfdAccessor&) = delete;
  BfdAccessor& operator=(const BfdAccessor&) = delete;
};

/// @brief bfd symbol locator which locate symbol for given address
class BfdSymbolLocator : public SymbolLocator {
 public:
  /// @brief for current program analysis
  BfdSymbolLocator();
  /// @brief given program file path and proc mapping content for offline analysis
  BfdSymbolLocator(const std::string& prog_path, const std::string& proc_map_data);
  ~BfdSymbolLocator() override = default;
  LocatorStatus SearchSymbols(const std::vector<void*>& addrs,
                              std::unordered_map<void*, SymbolInfo>* sym_mapping) override;

 private:
  struct SearchSymbolMeta {
    bool found{false};
    bfd_symbol** syms{nullptr};
    bfd_vma pc;
    const char* file_name{nullptr};
    const char* function_name{nullptr};
    unsigned int line{0};
  };

  struct FileMatchMeta {
    std::string file;
    const void* address{nullptr};
    const void* base{nullptr};
  };

  LocatorStatus LoadSelfSymbols();
  LocatorStatus PreLoadDynSymbols();
  LocatorStatus LoadMiniSymbols(const std::string& filename, bool only_dynamic, BfdAccessor* bfd_info);
  LocatorStatus SearchDynamic(const void* addr, SymbolInfo* sym_info);
  LocatorStatus SearchStatic(const void* addr, SymbolInfo* sym_info);
  LocatorStatus SearchBfd(const void* addr, const BfdAccessor* bfd_info_ptr, SymbolInfo* sym_info);
  LocatorStatus SearchSymbol(const void* addr, SymbolInfo* sym_info);
  bool FindMatchedLib(FileMatchMeta* meta);
  LocatorStatus GetOrCreateDynBfd(const std::string& file, BfdAccessor** bfd_info_ptr);
  LocatorStatus SearchDynamic(const FileMatchMeta& match, SymbolInfo* sym_info);

 private:
  BfdAccessor self_bfd_;
  std::shared_mutex rw_mutex_;
  std::unordered_map<std::string, BfdAccessor> dynamic_bfds_;
  DynamicLibMappings dyn_mappings_;
  std::string program_path_;
  std::string proc_mapping_content_;
  bool is_self_analysis_{false};  // is analyzing current process(online analysis)?
};

}  // namespace pprofcpp
