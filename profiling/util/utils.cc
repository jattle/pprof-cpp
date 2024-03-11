#include "profiling/util/utils.h"

#include <cxxabi.h>

#include <memory>
#include <cstdio>

namespace pprofcpp {

std::string DemangleName(const char* mangled_name) {
  int status;
  char* demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);

  std::string ret;
  if (status == 0) {
    ret = demangled;
    free(demangled);
  } else {
    ret = mangled_name;
  }
  return ret;
}

int LoadFileContent(const std::string& filename, std::string* content) {
  content->clear();
  std::unique_ptr<FILE, decltype(&std::fclose)> fp{std::fopen(filename.c_str(), "rb"), std::fclose};
  if (fp) {
    char buffer[4 * 1024];
    while (true) {
      size_t bytes = fread(buffer, sizeof(char), sizeof(buffer), fp.get());
      if (bytes > 0) {
        content->append(buffer, buffer + bytes);
      }
      if (bytes != sizeof(buffer)) {
        if (feof(fp.get())) {
          break;
        }
        // read error
        return 1;
      }
    }
    return 0;
  }
  return 1;
}

}