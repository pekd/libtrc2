libtrc2
=======

A C library to easily create "generic" (architecture independent) trace files
for [trcview](https://github.com/pekd/vmx86).
Unlike [libtrc](https://github.com/pekd/libtrc), this library preservess the CPU
state's structure, so it's possible to use registers in expressions and save a
lot of disk space.

Features
--------

- Memory management (`mmap`/`munmap`)
- Memory access (read/write)
- Custom CPU state structure
- Custom formatting of CPU state

Usage
-----

Write your code, include `trace.h`, and link it with `trace.c`. That's it.

Refer to the `demo.c` for more details.

Demo
----

The demo program `demo.c` creates a simple trace file with a few steps, memory
accesses and an mmap event.

Compile the demo with:
```c
gcc -o demo demo.c trace.c
```
