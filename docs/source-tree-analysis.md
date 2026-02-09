# BeastEm - Source Tree Analysis

## Directory Structure

```
beastem/
├── beastem.cpp              # Main entry point - CLI parsing, SDL init, main loop
├── CMakeLists.txt           # CMake build configuration
├── Makefile                 # Generated Makefile (from CMake)
├── README.md                # Primary documentation
├── VIDEOBEAST.md            # VideoBeast feature documentation
├── LICENSE                  # zlib/libpng license
├── .gitignore               # Git ignore patterns
├── firmware_p24.bin         # Legacy firmware binary
│
├── assets/                  # Runtime assets (fonts, images, data files)
│   ├── firmware.lst         # Default firmware listing file
│   ├── monitor.lst          # Monitor listing file
│   ├── flash_v1.7.bin       # Default MicroBeast firmware ROM
│   ├── layout_2d.png        # PCB layout image for debug view
│   ├── Roboto-Medium.ttf    # UI font
│   ├── RobotoMono-VariableFont_wght.ttf  # Monospace font for code
│   ├── palette_1.mem        # VideoBeast palette 1 (4bpp layers)
│   ├── palette_2.mem        # VideoBeast palette 2 (8bpp layers)
│   ├── video_registers.mem  # VideoBeast default register values
│   ├── videobeast.dat       # Demo VideoBeast memory image
│   └── z80_instr.txt        # Z80 instruction set reference
│
├── src/                     # Source code
│   ├── assets.cpp           # Asset path resolution
│   ├── assets.hpp
│   ├── beast.cpp            # Core emulator class (main loop, coordination)
│   ├── beast.hpp
│   ├── binaryFile.cpp       # ROM/RAM binary file loading
│   ├── binaryFile.hpp
│   ├── debug.cpp            # Debug output utilities
│   ├── debug.hpp
│   ├── digit.cpp            # 14-segment LED digit rendering
│   ├── digit.hpp
│   ├── display.cpp          # I2C LED display controller emulation
│   ├── display.hpp
│   ├── gui.cpp              # GUI components (prompts, editing)
│   ├── gui.hpp
│   ├── i2c.cpp              # I2C bus protocol emulation
│   ├── i2c.hpp
│   ├── instructions.cpp     # Z80 instruction decoding/disassembly
│   ├── instructions.hpp
│   ├── listing.cpp          # Assembly listing file parser
│   ├── listing.hpp
│   ├── rtc.cpp              # Real-time clock I2C device
│   ├── rtc.hpp
│   ├── uart16c550.h         # 16C550 UART emulation (header-only)
│   ├── videobeast.cpp       # VideoBeast video card emulation
│   ├── videobeast.hpp
│   ├── z80.h                # Floooh Chips Z80 CPU emulation
│   ├── z80pio.h             # Floooh Chips Z80 PIO emulation
│   └── include/             # SDL2 headers (vendored for Windows)
│       ├── SDL.h
│       ├── SDL_*.h          # Various SDL2 headers
│       └── ...
│
├── cmake/                   # CMake find modules
│   ├── FindSDL2_gfx.cmake
│   ├── FindSDL2_image.cmake
│   ├── FindSDL2_net.cmake
│   └── FindSDL2_ttf.cmake
│
├── deps/                    # External dependencies
│   └── nativefiledialog-extended-1.1.1/
│       ├── CMakeLists.txt
│       ├── src/
│       │   ├── nfd_gtk.cpp      # Linux GTK implementation
│       │   ├── nfd_win.cpp      # Windows implementation
│       │   ├── nfd_portal.cpp   # Linux portal implementation
│       │   └── include/
│       │       ├── nfd.h
│       │       └── nfd.hpp
│       └── test/
│
├── build/                   # Build output directory
│
├── docs/                    # Generated documentation
│   ├── index.md
│   ├── project-overview.md
│   ├── architecture.md
│   ├── source-tree-analysis.md
│   └── development-guide.md
│
├── .vscode/                 # VS Code configuration
│
└── .claude/                 # Claude Code configuration
```

## Source Files by Component

### Entry Point
| File | Lines | Description |
|------|-------|-------------|
| `beastem.cpp` | ~246 | Main entry, CLI parsing, initialization |

### Core Emulation
| File | Lines | Description |
|------|-------|-------------|
| `src/beast.cpp` | ~1800 | Main emulator class, event loop |
| `src/beast.hpp` | ~360 | Beast class definition |
| `src/z80.h` | ~4000 | Z80 CPU emulation (Floooh) |
| `src/z80pio.h` | ~800 | Z80 PIO emulation (Floooh) |

### Peripherals
| File | Lines | Description |
|------|-------|-------------|
| `src/i2c.cpp` | ~226 | I2C bus protocol |
| `src/i2c.hpp` | ~58 | I2C interfaces |
| `src/display.cpp` | ~200 | LED display controller |
| `src/display.hpp` | ~46 | Display interface |
| `src/digit.cpp` | ~200 | LED segment rendering |
| `src/digit.hpp` | ~41 | Digit class |
| `src/rtc.cpp` | ~200 | Real-time clock |
| `src/rtc.hpp` | ~74 | RTC interface |
| `src/uart16c550.h` | ~400 | UART emulation |

### VideoBeast
| File | Lines | Description |
|------|-------|-------------|
| `src/videobeast.cpp` | ~870 | Video card emulation |
| `src/videobeast.hpp` | ~193 | VideoBeast interface |

### Debug/Development
| File | Lines | Description |
|------|-------|-------------|
| `src/instructions.cpp` | ~300 | Z80 disassembly |
| `src/instructions.hpp` | ~100 | Instruction decoder |
| `src/listing.cpp` | ~250 | Listing file parser |
| `src/listing.hpp` | ~63 | Listing structures |
| `src/gui.cpp` | ~350 | GUI components |
| `src/gui.hpp` | ~166 | GUI interface |
| `src/debug.cpp` | ~0 | Debug utilities |
| `src/debug.hpp` | ~29 | Debug stream class |

### File Handling
| File | Lines | Description |
|------|-------|-------------|
| `src/binaryFile.cpp` | ~100 | Binary file loading |
| `src/binaryFile.hpp` | ~35 | BinaryFile class |
| `src/assets.cpp` | ~37 | Asset path resolution |
| `src/assets.hpp` | ~13 | Asset functions |

## Key Entry Points

### Application Start
- **File**: `beastem.cpp:65`
- **Function**: `main()`
- **Flow**: CLI parsing → SDL init → Beast construction → main loop

### Emulation Loop
- **File**: `src/beast.cpp`
- **Function**: `Beast::mainLoop()`
- **Flow**: Event handling → `run()` → Draw → Repeat

### CPU Execution
- **File**: `src/beast.cpp`
- **Function**: `Beast::run()`
- **Flow**: `z80_tick()` → Memory/IO handling → Peripheral updates

## Asset Files

### Fonts
| File | Usage |
|------|-------|
| `Roboto-Medium.ttf` | Main UI text |
| `RobotoMono-VariableFont_wght.ttf` | Debug view, hex values |

### Firmware
| File | Description |
|------|-------------|
| `flash_v1.7.bin` | Default MicroBeast ROM image |
| `firmware.lst` | Firmware assembly listing |
| `monitor.lst` | Monitor program listing |

### VideoBeast
| File | Description |
|------|-------------|
| `palette_1.mem` | 16-color palettes (4bpp modes) |
| `palette_2.mem` | 256-color palette (8bpp modes) |
| `video_registers.mem` | Default register configuration |
| `videobeast.dat` | Demo graphics data (1MB) |

### Images
| File | Description |
|------|-------------|
| `layout_2d.png` | PCB layout for debug display |

## Build Artifacts

### Generated Files (gitignored)
- `beastem` - Main executable
- `Makefile` - Generated by CMake
- `CMakeCache.txt` - CMake cache
- `CMakeFiles/` - CMake build files
- `build/` - Object files
- `deps/*/libnfd.a` - NFD static library

## Configuration Files

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Main build configuration |
| `.gitignore` | Git ignore patterns |
| `.vscode/` | VS Code settings |
| `cmake/*.cmake` | SDL2 find modules |
