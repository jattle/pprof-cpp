/*
 * FileName profile_printer.cc
 * Author jattle
 * Copyright (c) 2024 jattle 
 * Description: CPU Profile printer
 */
#include "profiling/cpu_profile.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s profile\n", argv[0]);
    return -1;
  }
  pprofcpp::CPUProfile profile{argv[1]};
  auto ret = profile.Parse();
  if (ret != pprofcpp::ReaderRetCode::kOK) {
    fprintf(stderr, "parse profile failed, ret: %d\n", static_cast<int>(ret));
  } else {
    fprintf(stdout, "Dump CPU profile:\n");
    fprintf(stdout, "%s\n", profile.ToString().c_str());
  }
  return 0;
}
