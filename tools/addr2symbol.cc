#include <cstring>

#include "gflags/gflags.h"

#include "profiling/symbol/profile_symbol.h"

DEFINE_string(exe, "", "executable file path");
DEFINE_string(proc_mapping, "", "proc mapping file path, maybe empty");
DEFINE_string(addr, "", "hex memory address, 0x00007fd4246d05b6 or 00007fd4246d05b6 etc");

int main(int argc, char* argv[]) {
  if (FLAGS_exe.empty() || FLAGS_addr.empty()) {
    google::ShowUsageWithFlags(argv[0]);
    return 1;
  }
  pprofcpp::BfdSymbolLocator locator{FLAGS_exe, FLAGS_proc_mapping};
  pprofcpp::SymbolInfo sym_info;
  constexpr size_t kBufferSize = 32;
  char buffer[kBufferSize] = {0};
  if (FLAGS_addr.length() > 2u && strncasecmp(FLAGS_addr.c_str(), "0x", 2u) == 0) {
    size_t min_size = FLAGS_addr.length() < kBufferSize ? FLAGS_addr.length() : kBufferSize;
    strncpy(buffer, FLAGS_addr.data(), min_size);
  } else {
    strncpy(buffer, "0x", 2);
    size_t min_size = FLAGS_addr.length() < kBufferSize - 2 ? FLAGS_addr.length() : kBufferSize - 2;
    strncpy(buffer + 2, FLAGS_addr.data(), min_size);
  }
  void* addr{nullptr};
  sscanf(buffer, "%p", &addr);
  std::vector<void*> addrs{addr};
  std::unordered_map<void*, pprofcpp::SymbolInfo> sym_mapping;
  auto st = locator.SearchSymbols(addrs, &sym_mapping);
  if (st.ret == pprofcpp::LocatorRetCode::kOK) {
    fprintf(stderr, "addr: %#018lx, symbol: %s\n", reinterpret_cast<uintptr_t>(addr),
            sym_mapping[addr].symbol_name.c_str());
  }
  return 0;
}
