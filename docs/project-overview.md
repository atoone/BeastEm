# BeastEm - Project Overview

## Executive Summary

**BeastEm** is a cycle-accurate emulator for the [MicroBeast Z80 computer kit](https://feertech.com/microbeast/), developed by Feersum Technology. It provides a complete software simulation of the MicroBeast hardware, enabling software development and testing without physical hardware.

## Project Information

| Property | Value |
|----------|-------|
| **Project Name** | BeastEm (Feersum MicroBeast Emulator) |
| **Version** | 1.2 |
| **Author** | Andrew Toone / Feersum Technology |
| **License** | zlib/libpng |
| **Repository Type** | Monolith |
| **Primary Language** | C++ (C++17/20) |
| **Platforms** | Windows, Linux, macOS |

## Purpose

BeastEm serves as:
1. **Development Tool** - Develop and test Z80 assembly programs for MicroBeast
2. **Learning Platform** - Understand Z80 architecture and retro computing
3. **Hardware Prototyping** - Test VideoBeast graphics chip features before hardware availability

## Key Features

### Emulated Hardware
- **CPU**: Z80 processor with cycle-accurate timing
- **Memory**: 512K RAM, 512K ROM with 4x16K memory paging
- **I/O**: Z80 PIO with software I2C bus
- **Display**: 24-character, 14-segment LED display
- **Peripherals**: RTC (Real-Time Clock), 16C550 UART
- **Input**: I/O mapped keyboard (48 keys)
- **Audio**: 1-bit audio output

### Development Features
- Built-in debugger with single-step execution
- Z80 disassembler with listing file integration
- Memory inspection and editing
- Register viewing and modification
- Breakpoint support
- Serial console over network socket (TCP)

### VideoBeast Support (Optional)
- Prototype video graphics chip emulation
- Multiple video modes (640x480, 848x480)
- Layer-based rendering (text, tiles, bitmaps, sprites)
- Two 256-color palettes
- 1MB video RAM

## Technology Stack

| Category | Technology | Purpose |
|----------|------------|---------|
| Language | C++ | Core implementation |
| Build | CMake | Cross-platform builds |
| Graphics | SDL2 | Windowing, rendering, input |
| Fonts | SDL2_ttf | Text rendering |
| Network | SDL2_net | UART serial over TCP |
| CPU Core | Floooh Chips | Z80 emulation |

## Quick Start

```bash
# Build (Linux/macOS)
cmake .
make clean all

# Run with default firmware
./beastem

# Run with custom firmware
./beastem -f myprogram.bin

# Run with VideoBeast enabled
./beastem -d2 videobeast.dat
```

## Documentation Index

- [Architecture Documentation](./architecture.md)
- [Source Tree Analysis](./source-tree-analysis.md)
- [Development Guide](./development-guide.md)

## Related Resources

- [MicroBeast Wiki](https://github.com/atoone/MicroBeast/wiki)
- [Feersum Technology Website](https://feertech.com/microbeast/)
- [VideoBeast Documentation](https://feertech.com/microbeast/videobeast_doc.html)
