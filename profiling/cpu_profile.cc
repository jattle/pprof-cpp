/*
 * FileName: cpu_profile.cc
 * Author: jattle
 * Descrption:
 */

#include "profiling/cpu_profile.h"

#include <sstream>
#include <unordered_set>

#include "fmt/format.h"

#include "fust/fustsdk/profiling/profile_symbol.h"

namespace fustsdk {

ReaderRetCode CPUProfile::Parse() {
  CPUProfileReader reader(std::move(this->is_));
#define RETURN_IF_NOT_EXPECTED(expr, expected) \
  do {                                         \
    auto ret = (expr);                         \
    if (ret != (expected)) return ret;         \
  } while (0);
  // read header
  size_t index = 0;
  RETURN_IF_NOT_EXPECTED(reader.GetSlot(index++, &this->binary_header_.hdr_count), ReaderRetCode::kOK);
  RETURN_IF_NOT_EXPECTED(reader.GetSlot(index++, &this->binary_header_.hdr_words), ReaderRetCode::kOK);
  RETURN_IF_NOT_EXPECTED(reader.GetSlot(index++, &this->binary_header_.version), ReaderRetCode::kOK);
  RETURN_IF_NOT_EXPECTED(reader.GetSlot(index++, &this->binary_header_.sampling_period), ReaderRetCode::kOK);
  if (auto st = reader.GetSlot(index++, &this->binary_header_.padding); st != ReaderRetCode::kOK) {
    return st;
  }
  // read record
  bool end_of_slots{false};
  while (true) {
    size_t sample_count, num_pcs, pc{0};
    RETURN_IF_NOT_EXPECTED(reader.GetSlot(index++, &sample_count), ReaderRetCode::kOK);
    RETURN_IF_NOT_EXPECTED(reader.GetSlot(index++, &num_pcs), ReaderRetCode::kOK);
    RETURN_IF_NOT_EXPECTED(reader.GetSlot(index++, &pc), ReaderRetCode::kOK);
    if (pc == 0) {
      // Binary Trailer found: gperftools/docs/cpuprofile-fileformat.html
      // end of slots
      end_of_slots = true;
      break;
    }
    CallStack stack;
    stack.sample_count = sample_count;
    stack.ptrs.emplace_back(reinterpret_cast<void*>(pc));
    for (size_t i = 1; i < num_pcs; i++) {
      size_t val;
      if (auto st = reader.GetSlot(index++, &val); st != ReaderRetCode::kOK) {
        return st;
      }
      stack.ptrs.emplace_back(reinterpret_cast<void*>(val));
    }
    this->total_sample_cnt_ += sample_count;
    this->record_num_++;
    this->ptr_num_ += stack.ptrs.size();
    this->stacks_.emplace_back(std::move(stack));
  }
  // Parse Text List of Mapped Objects
  if (end_of_slots) {
    std::string maps_text;
    if (auto ret = reader.ReadLeftContent(&maps_text); ret != ReaderRetCode::kEndOfFile) {
      return ReaderRetCode::kReadError;
    }
    // parse text lines
    if (ParseMapsText(maps_text) != 0) {
      return ReaderRetCode::kEmptyMapsText;
    }
    this->maps_text_ = std::move(maps_text);
  }
#undef RETURN_IF_NOT_EXPECTED
  return ReaderRetCode::kOK;
}

void CPUProfile::ReplaceBuildSpecifier(const std::string& pat, const std::string& target, std::string& line) {
  auto is_word_char = [](char c) -> bool { return isalnum(c) || c == '_'; };
  for (auto pos = line.find(pat); pos != std::string::npos;) {
    size_t next_start = pos + pat.length();
    if (next_start == line.length() || !is_word_char(line.at(next_start))) {
      // replace pattern
      line.replace(pos, pat.length(), target);
      pos = line.find(pat);
    } else {
      pos = line.find(pat, next_start);
    }
  }
}

int CPUProfile::ParseMapsText(const std::string& maps_text) {
  if (maps_text.empty()) {
    return -1;
  }
  std::istringstream iss{maps_text};
  std::string binary;
  const std::string kBuildSpecifier{"build="};
  const std::string kBuildPlaceHolder{"$build"};

  for (std::string line; std::getline(iss, line);) {
    if (line.empty()) {
      continue;
    }
    if (line.length() >= kBuildSpecifier.length() &&
        strncmp(line.c_str(), kBuildSpecifier.c_str(), kBuildSpecifier.length()) == 0) {
      // Build specifier found, for example, build=/path/to/binary
      binary = line.substr(kBuildSpecifier.length());
    } else {
      // Mapping line, for example, 40000000-40015000 r-xp 00000000 03:01 12845071   /lib/ld-2.3.2.so
      // The first address must start at the beginning of the line.
      // When processing the paths see in mapping lines, occurrences of $build followed by a non-word character
      // (i.e., characters other than underscore or alphanumeric characters),
      // should be replaced by the path given on the last build specifier line.
      ReplaceBuildSpecifier(kBuildPlaceHolder, binary, line);
      this->proc_maps_items_.emplace_back(std::move(line));
    }
  }
  return this->proc_maps_items_.empty() ? -1 : 0;
}

CPUProfileRetCode CPUProfile::GenerateSymbolMapping(SymbolLocator* locator) {
  if (this->stacks_.empty()) {
    return CPUProfileRetCode::kEmptyStack;
  }
  std::unordered_set<void*> addrs_set;
  for (const auto& s : this->stacks_) {
    if (s.ptrs.empty()) {
      continue;
    }
    addrs_set.insert(s.ptrs.at(0));
    for (size_t i = 1; i < s.ptrs.size(); i++) {
      // subtract by 1 to get call ptr
      addrs_set.insert(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(s.ptrs[i]) - 1));
    }
  }
  std::unordered_map<void*, SymbolInfo> sym_mapping;
  std::vector<void*> addrs{addrs_set.cbegin(), addrs_set.cend()};
  if (auto ret = locator->SearchSymbols(addrs, &sym_mapping); ret.ret != LocatorRetCode::kOK) {
    return CPUProfileRetCode::kSearchSymbolFailed;
  }
  for (const auto& item : sym_mapping) {
    this->symbol_mapping_[item.first] = item.second.symbol_name;
  }
  return CPUProfileRetCode::kOK;
}

/// @brief generate raw profile(similar to file genreated by pprof --raw)
CPUProfileRetCode CPUProfile::GenerateRawProfile(const RawProfileMeta& meta, SymbolLocator* locator,
                                                 std::string* profile) {
  if (meta.program_path.empty()) {
    return CPUProfileRetCode::kNoProgramPath;
  }
  // assume 2M
  profile->reserve(2 * 1024 * 1024);
  if (meta.profile_type == RawProfileType::kFixedRaw) {
    profile->append("--- symbol_fixed\n");
  } else if (meta.profile_type == RawProfileType::kPProfCompatible) {
    profile->append("--- symbol\n");
  }
  profile->append(fmt::format("binary={}\n", meta.program_path));
  std::string symbols;
  if (auto ret = GenerateRawSymbols(locator, &symbols); ret != CPUProfileRetCode::kOK) {
    return ret;
  }
  profile->append(symbols);
  profile->append("---\n");
  profile->append("--- profile\n");
  std::string content;
  if (auto st = GenerateBinaryProfile(meta, &content); st != CPUProfileRetCode::kOK) {
    return st;
  }
  profile->append(content);
  return CPUProfileRetCode::kOK;
}

CPUProfileRetCode CPUProfile::GenerateBinaryProfile(const RawProfileMeta& meta, std::string* content) {
  std::shared_ptr<std::ostream> os = std::make_shared<std::ostringstream>();
  CPUProfileWriter writer{os, this->binary_header_};
#define RETURN_IF_NOT_EXPECTED(expr, expected)                          \
  do {                                                                  \
    auto ret = (expr);                                                  \
    if (ret != (expected)) return CPUProfileRetCode::kGenProfileFailed; \
  } while (0);
  // dump stack
  for (const auto& s : this->stacks_) {
    // sample count, num_pc, pc
    RETURN_IF_NOT_EXPECTED(writer.AppendSlot(s.sample_count), WriterRetCode::kOK);
    RETURN_IF_NOT_EXPECTED(writer.AppendSlot(s.ptrs.size()), WriterRetCode::kOK);
    RETURN_IF_NOT_EXPECTED(writer.AppendSlot(reinterpret_cast<uintptr_t>(s.ptrs.at(0))), WriterRetCode::kOK);
    for (size_t i = 1; i < s.ptrs.size(); i++) {
      if (meta.profile_type == RawProfileType::kPProfCompatible) {
        // call ptr is subtracted by 1
        RETURN_IF_NOT_EXPECTED(writer.AppendSlot(reinterpret_cast<uintptr_t>(s.ptrs.at(i)) - 1), WriterRetCode::kOK);
      } else if (meta.profile_type == RawProfileType::kFixedRaw) {
        RETURN_IF_NOT_EXPECTED(writer.AppendSlot(reinterpret_cast<uintptr_t>(s.ptrs.at(i))), WriterRetCode::kOK);
      }
    }
  }
  // dump trailer
  RETURN_IF_NOT_EXPECTED(writer.AppendSlot(0), WriterRetCode::kOK);
  RETURN_IF_NOT_EXPECTED(writer.AppendSlot(1), WriterRetCode::kOK);
  RETURN_IF_NOT_EXPECTED(writer.AppendSlot(0), WriterRetCode::kOK);
  // we dont need maps text here
  *content = static_cast<std::ostringstream*>(os.get())->str();
#undef RETURN_IF_NOT_EXPECTED
  return CPUProfileRetCode::kOK;
}

CPUProfileRetCode CPUProfile::GenerateRawSymbols(SymbolLocator* locator, std::string* symbols) {
  if (!stacks_.empty() && symbol_mapping_.empty()) {
    if (auto ret = GenerateSymbolMapping(locator); ret != CPUProfileRetCode::kOK) {
      return ret;
    }
  }
  char buf[20] = {0};
  for (const auto& [addr, sym] : symbol_mapping_) {
    size_t n = snprintf(buf, sizeof(buf), "%#018lx", reinterpret_cast<uintptr_t>(addr));
    symbols->append(buf, buf + n);
    symbols->append(" ");
    symbols->append(sym.empty() ? std::string(buf, buf + n) : sym);
    symbols->append("\n");
  }
  return CPUProfileRetCode::kOK;
}

std::string CPUProfile::ToString() {
  std::string report;
  report.append(fmt::format("---------------Header:\n"));
  report.append(fmt::format("hdr_count: {}\n", this->binary_header_.hdr_count));
  report.append(fmt::format("hdr_words: {}\n", this->binary_header_.hdr_words));
  report.append(fmt::format("version: {}\n", this->binary_header_.version));
  report.append(fmt::format("sampling_period: {}\n", this->binary_header_.sampling_period));
  report.append(fmt::format("padding: {}\n", this->binary_header_.padding));
  report.append(fmt::format("profile num: {}, total sample num: {}, call stack num: {}, ptr nums: {}\n",
                            this->record_num_, this->total_sample_cnt_, this->stacks_.size(), this->ptr_num_));
  report.append(fmt::format("---------------Stacks:\n"));
  std::unordered_set<void*> dedupped_ptrs;
  char buf[20] = {0};
  for (const auto& s : this->stacks_) {
    for (const auto& ptr : s.ptrs) {
      size_t n = snprintf(buf, sizeof(buf), "%#018lx ", reinterpret_cast<uintptr_t>(ptr));
      report.append(buf, buf + n);
      dedupped_ptrs.insert(ptr);
    }
    report.append("\n");
  }
  report.append(fmt::format("distinct ptr num: {}\n", dedupped_ptrs.size()));
  return report;
}

}  // namespace fustsdk
