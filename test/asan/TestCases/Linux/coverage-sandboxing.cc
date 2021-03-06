// RUN: %clangxx_asan -mllvm -asan-coverage=2 -DSHARED %s -shared -o %T/libcoverage_sandboxing_test.so -fPIC
// RUN: %clangxx_asan -mllvm -asan-coverage=1 %s   -o %t -Wl,-R,\$ORIGIN -L%T -lcoverage_sandboxing_test
// RUN: export ASAN_OPTIONS=coverage=1:verbosity=1
// RUN: rm -rf %T/coverage_sandboxing_test
// RUN: mkdir %T/coverage_sandboxing_test && cd %T/coverage_sandboxing_test
// RUN: mkdir vanilla && cd vanilla
// RUN: %run %t 2>&1         | FileCheck %s --check-prefix=CHECK-vanilla
// RUN: mkdir ../sandbox1 && cd ../sandbox1
// RUN: %run %t a 2>&1       | FileCheck %s --check-prefix=CHECK-sandbox
// RUN: %sancov unpack coverage_sandboxing_test.sancov.packed 
// RUN: mkdir ../sandbox2 && cd ../sandbox2
// RUN: %run %t a b 2>&1     | FileCheck %s --check-prefix=CHECK-sandbox
// RUN: %sancov unpack coverage_sandboxing_test.sancov.packed 
// RUN: cd ..
// RUN: %sancov print vanilla/libcoverage_sandboxing_test.so.*.sancov > vanilla.txt
// RUN: %sancov print sandbox1/libcoverage_sandboxing_test.so.*.sancov > sandbox1.txt
// RUN: %sancov print sandbox2/libcoverage_sandboxing_test.so.*.sancov > sandbox2.txt
// RUN: diff vanilla.txt sandbox1.txt
// RUN: diff vanilla.txt sandbox2.txt
// RUN: cd ../ && rm coverage_sandboxing_test -r
// https://code.google.com/p/address-sanitizer/issues/detail?id=263
// XFAIL: android

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sanitizer/common_interface_defs.h>

#define bb0(n)                        \
  case n:                             \
    fprintf(stderr, "foo: %d\n", n);  \
    break;

#define bb1(n) bb0(n) bb0(n + 1)
#define bb2(n) bb1(n) bb1(n + 2)
#define bb3(n) bb2(n) bb2(n + 4)
#define bb4(n) bb3(n) bb3(n + 8)
#define bb5(n) bb4(n) bb4(n + 16)
#define bb6(n) bb5(n) bb5(n + 32)
#define bb7(n) bb6(n) bb6(n + 64)
#define bb8(n) bb7(n) bb7(n + 128)

#ifdef SHARED
void foo(int i) {
  switch(i) {
    // 256 basic blocks
    bb8(0)
  }
}
#else
extern void foo(int i);

int main(int argc, char **argv) {
  assert(argc <= 3);
  for (int i = 0; i < 256; i++) foo(i);
  fprintf(stderr, "PID: %d\n", getpid());
  if (argc == 1) {
    // Vanilla mode, dump to individual files.
    return 0;
  }
  // Dump to packed file.
  int fd = creat("coverage_sandboxing_test.sancov.packed", 0660);
  __sanitizer_sandbox_arguments args = {0};
  args.coverage_sandboxed = 1;
  args.coverage_fd = fd;
  if (argc == 2)
    // Write to packed file, do not split into blocks.
    args.coverage_max_block_size = 0;
  else if (argc == 3)
    // Write to packed file, split into blocks (as if writing to a socket).
    args.coverage_max_block_size = 100;
  __sanitizer_sandbox_on_notify(&args);
  return 0;
}
#endif

// CHECK-vanilla: PID: [[PID:[0-9]+]]
// CHECK-vanilla: [[PID]].sancov: 1 PCs written
// CHECK-vanilla: .so.[[PID]].sancov: 258 PCs written

// CHECK-sandbox: PID: [[PID:[0-9]+]]
// CHECK-sandbox: 258 PCs written to packed file
