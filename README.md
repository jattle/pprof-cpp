# Introduction 
Provide efficient parsing and convertion for gperftools CPU/MEM Profile.  
incldudes：  
- profile IO
- local generation of symbolized profile
  it can be submmited to remote server for later processing, such as flamegraph generation.
- others
  more utility comming soon.
# Dependencies 
 GNU binutils lib, if not exists on current os, install it using yum or other package tools according to your os:
```shell
yum install binutils-devel.x86_64
```
# examples
## online processing
```cpp
#include "profiling/cpu_profile.h"

int GenerateSymbolizedProfile(const std::string &file, std::string *output) {
    pprofcpp::CPUProfile profile{file};
    if (auto st = profile.Parse(); st != pprofcpp::ReaderRetCode::kOK) {
        return static_cast<int>(st);
    }
    pprofcpp::BfdSymbolLocator locator;
    RawProfileMeta meta;
    meta.program_path = "testbin";
    if (auto st = profile.GenerateRawProfile(meta, &locator, output); st != pprofcpp::CPUProfileRetCode::kOK) {
        return static_cast<int>(st);
    }
    return 0;
}
```
## offline processing
see tools/profile_printer and tools/addr2symbol.