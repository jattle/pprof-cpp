/*
 * FileName profile_symbol.cc
 * Author jattle
 * Copyright (c) 2024 jattle 
 * Description:
 */
#include "profiling/symbol/profile_symbol.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <link.h>
#include <sstream>

#include "fmt/format.h"

#include "profiling/util/utils.h"

namespace pprofcpp {

constexpr char kSelfExePath[] = "/proc/self/exe";
constexpr char kSelfMapsPath[] = "/proc/self/maps";


BfdSymbolLocator::BfdSymbolLocator() {
  this->is_self_analysis_ = true;
  this->program_path_ = kSelfExePath;
  LoadFileContent(kSelfMapsPath, &this->proc_mapping_content_);
  bfd_init();
  LoadSelfSymbols();
}

BfdSymbolLocator::BfdSymbolLocator(const std::string& prog_path, const std::string& proc_map_data) {
  this->program_path_ = prog_path;
  this->proc_mapping_content_ = proc_map_data;
  bfd_init();
  LoadSelfSymbols();
}

LocatorStatus BfdSymbolLocator::LoadSelfSymbols() {
  // load self static symbols
  BfdAccessor bfd_info;
  if (auto ret = this->LoadMiniSymbols(this->program_path_, false, &bfd_info); ret.ret != LocatorRetCode::kOK) {
    return ret;
  }
  this->self_bfd_ = std::move(bfd_info);
  return LocatorStatus{LocatorRetCode::kOK, ""};
}

LocatorStatus BfdSymbolLocator::LoadMiniSymbols(const std::string& filename, bool only_dynamic, BfdAccessor* bfd_info) {
  bfd_info->bfd_ptr = bfd_openr(filename.c_str(), nullptr);
  if (bfd_info->bfd_ptr == nullptr) {
    return LocatorStatus{LocatorRetCode::kOpenFileFailed, fmt::format("open file {} failed", filename)};
  }
  if (bfd_check_format(bfd_info->bfd_ptr, bfd_object) == FALSE) {
    return LocatorStatus{LocatorRetCode::kCheckFormatErr, "Failed to process executable format"};
  }

  if ((bfd_get_file_flags(bfd_info->bfd_ptr) & HAS_SYMS) == 0) {
    return LocatorStatus{LocatorRetCode::kNoSymbols, "No symbols in executable"};
  }
  unsigned int psize{0};
  bfd_info->sym_count =
      bfd_read_minisymbols(bfd_info->bfd_ptr, only_dynamic, reinterpret_cast<void**>(&bfd_info->mini_syms), &psize);
  if (bfd_info->sym_count == 0) {
    return LocatorStatus{LocatorRetCode::kReadSymbolsFailed, "Failed to read symbols"};
  }
  asymbol** symbol_table = bfd_info->mini_syms;
  std::sort(symbol_table, symbol_table + bfd_info->sym_count, [](const asymbol* l, const asymbol* r) -> bool {
    return l->section->vma + l->value < r->section->vma + r->value;
  });
  return LocatorStatus{LocatorRetCode::kOK, ""};
}

int DynamicLibMappings::ParseProcMaps(const std::string& proc_mapping_content) {
  // parse content, extract dynamic libs loaded by program
  this->lib_mappings_.clear();
  std::istringstream iss{proc_mapping_content};
  // for lib mapping info aggregation
  std::unordered_map<int, ProcLibMapping*> ref_map;
  for (std::string line; std::getline(iss, line);) {
    int inode{0};
    char pathname[1024] = {0};
    ProcMapItem item;
    int ret = sscanf(line.c_str(), "%lx-%lx %4s %lx %x:%x %d %s", &item.start_addr, &item.end_addr, item.perms,
                     &item.offset, &item.dev_major, &item.dev_minor, &inode, pathname);

    if (ret == 8) {
      // only accept dynamic libs
      if (pathname[0] == '/' && strstr(pathname, ".so") != nullptr) {
        // update global addr bound
        if (this->lower_bound_ > item.start_addr) {
          this->lower_bound_ = item.start_addr;
        }
        if (this->upper_bound_ < item.end_addr) {
          this->upper_bound_ = item.end_addr;
        }
        auto iter = ref_map.find(inode);
        if (iter == ref_map.cend()) {
          // first found，create LibMapping，update load base addr & high addr
          ProcLibMapping lib_item;
          lib_item.base = item.start_addr;
          lib_item.upper_bound = item.end_addr;
          lib_item.inode = inode;
          lib_item.path = pathname;
          lib_item.items.emplace_back(std::move(item));
          this->lib_mappings_.emplace_back(std::move(lib_item));
          auto& back_item = this->lib_mappings_.back();
          ref_map.emplace(back_item.inode, &back_item);
        } else {
          // every LibMaping may has many ProcMapItems, add item and update its bound
          if (item.start_addr < iter->second->base) {
            iter->second->base = item.start_addr;
          }
          if (item.end_addr > iter->second->upper_bound) {
            iter->second->upper_bound = item.end_addr;
          }
          iter->second->items.emplace_back(std::move(item));
        }
      }
    }
  }
  return 0;
}

bool DynamicLibMappings::GetLibPaths(std::vector<std::string>* paths) {
  if (this->lib_mappings_.empty()) {
    return false;
  }
  for (const auto& item : this->lib_mappings_) {
    paths->push_back(item.path);
  }
  return true;
}

bool DynamicLibMappings::FindMatchedLib(const void* target_addr, ProcLibMapping* lib_mapping) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(target_addr);
  if (addr < this->lower_bound_ || addr >= this->upper_bound_) {
    return false;
  }
  for (auto iter = this->lib_mappings_.cbegin(); iter != this->lib_mappings_.cend(); iter++) {
    if (addr < iter->base || addr >= iter->upper_bound) {
      continue;
    }
    for (const auto& item : iter->items) {
      if (addr >= item.start_addr && addr < item.end_addr) {
        *lib_mapping = *iter;
        return true;
      }
    }
  }
  return false;
}

bool BfdSymbolLocator::FindMatchedLib(FileMatchMeta* meta) {
  std::shared_lock<std::shared_mutex> locker(this->rw_mutex_);
  ProcLibMapping lib_mapping;
  if (this->dyn_mappings_.FindMatchedLib(meta->address, &lib_mapping)) {
    meta->base = reinterpret_cast<void*>(lib_mapping.base);
    meta->file = lib_mapping.path;
    return true;
  }
  return false;
}

LocatorStatus BfdSymbolLocator::GetOrCreateDynBfd(const std::string& file, BfdAccessor** bfd_info_ptr) {
  {
    std::shared_lock<std::shared_mutex> locker(rw_mutex_);
    if (auto iter = this->dynamic_bfds_.find(file); iter != this->dynamic_bfds_.cend()) {
      *bfd_info_ptr = &iter->second;
      return LocatorStatus{LocatorRetCode::kOK, ""};
    }
  }
  std::unique_lock<std::shared_mutex> locker(rw_mutex_);
  if (auto iter = this->dynamic_bfds_.find(file); iter != this->dynamic_bfds_.cend()) {
    *bfd_info_ptr = &iter->second;
    return LocatorStatus{LocatorRetCode::kOK, ""};
  }
  // not loaded yet
  BfdAccessor bfd_info;
  // load normal symbols first
  auto ret = this->LoadMiniSymbols(file, false, &bfd_info);
  if (ret.ret != LocatorRetCode::kOK) {
    // no normal symbols, maybe stripped, load dynamic symbols
    ret = this->LoadMiniSymbols(file, true, &bfd_info);
  }
  if (ret.ret == LocatorRetCode::kOK) {
    *bfd_info_ptr = &(this->dynamic_bfds_.emplace(file, std::move(bfd_info)).first->second);
  }
  return ret;
}

LocatorStatus BfdSymbolLocator::SearchSymbol(const void* addr, SymbolInfo* sym_info) {
  if (this->self_bfd_.sym_count == 0) {
    return LocatorStatus{LocatorRetCode::kNoSymbols, "no symbols, maybe not inited yet"};
  }
  // search dynamic first
  FileMatchMeta match;
  match.address = addr;
  if (FindMatchedLib(&match) && !match.file.empty()) {
    return SearchDynamic(match, sym_info);
  }
  return SearchStatic(addr, sym_info);
}

LocatorStatus BfdSymbolLocator::SearchDynamic(const FileMatchMeta& match, SymbolInfo* sym_info) {
  BfdAccessor* bfd_info_ptr{nullptr};
  if (auto ret = this->GetOrCreateDynBfd(match.file, &bfd_info_ptr); ret.ret != LocatorRetCode::kOK) {
    return ret;
  }
  if (bfd_info_ptr->sym_count == 0) {
    return LocatorStatus{LocatorRetCode::kNoSymbols, ""};
  }
  void* raddr =
      reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(match.address) - reinterpret_cast<uintptr_t>(match.base));
  return this->SearchBfd(raddr, bfd_info_ptr, sym_info);
}

LocatorStatus BfdSymbolLocator::SearchStatic(const void* addr, SymbolInfo* sym_info) {
  return this->SearchBfd(addr, &self_bfd_, sym_info);
}

LocatorStatus BfdSymbolLocator::SearchBfd(const void* addr, const BfdAccessor* bfd_info_ptr, SymbolInfo* sym_info) {
  // nearest line: largest addr which <= target addr
  auto pc = reinterpret_cast<bfd_vma>(addr);
  asymbol fake_symbol;
  bfd_section fake_section;
  fake_section.vma = 0;
  fake_symbol.value = pc;
  fake_symbol.section = &fake_section;
  asymbol** iter = std::lower_bound(bfd_info_ptr->mini_syms, bfd_info_ptr->mini_syms + bfd_info_ptr->sym_count,
                                    &fake_symbol, [](const asymbol* l, const asymbol* r) -> bool {
                                      return l->section->vma + l->value < r->section->vma + r->value;
                                    });
  if (iter != bfd_info_ptr->mini_syms + bfd_info_ptr->sym_count) {
    if ((*iter)->value + (*iter)->section->vma > pc && iter != bfd_info_ptr->mini_syms) {
      // hit an symbol which address > pc, so decrease iter to get item which address < pc
      iter--;
    }
    if ((*iter)->value + (*iter)->section->vma <= pc) {
      sym_info->address = addr;
      sym_info->symbol_name = DemangleName((*iter)->name);
      return LocatorStatus{LocatorRetCode::kOK, ""};
    }
  }

  return LocatorStatus{LocatorRetCode::kSymbolNotFound, "no symbol"};
}

LocatorStatus BfdSymbolLocator::SearchSymbols(const std::vector<void*>& addrs,
                                              std::unordered_map<void*, SymbolInfo>* sym_mapping) {
  if (addrs.empty()) {
    return LocatorStatus{LocatorRetCode::kNoAddr, "no addrs provided"};
  }
  {
    std::unique_lock<std::shared_mutex> locker(rw_mutex_);
    if (this->is_self_analysis_) {
      // online analysis, load maps content again
      if (LoadFileContent(kSelfMapsPath, &this->proc_mapping_content_) != 0) {
        return LocatorStatus{LocatorRetCode::kOpenFileFailed, "load proc maps failed"};
      }
    }
    this->dyn_mappings_.ParseProcMaps(this->proc_mapping_content_);
  }
  for (const auto& addr : addrs) {
    SymbolInfo info;
    this->SearchSymbol(addr, &info);
    sym_mapping->emplace(addr, info);
  }
  return LocatorStatus{LocatorRetCode::kOK, ""};
}

}  // namespace pprofcpp
