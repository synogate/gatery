riscv-tests
================

This repository hosts unit tests for RISC-V processors.

Source: https://github.com/riscv-software-src/riscv-tests

Building these test binaries from source is straight forward. Make sure to include riscv64-unknown-elf-* in PATH.

```
./configure --with-xlen=32
make -j
```

Output in isa folder.

NOTE: We had to replace the mret instruction in env/p/riscv_test.h with jr t0 since our riscv thinks its a normal ecall.
