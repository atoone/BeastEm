# BeastEm - Architecture Documentation

## Overview

BeastEm follows a **modular monolith** architecture with clear separation between emulation components, peripherals, and user interface. The design prioritizes cycle-accurate emulation over performance optimization.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         beastem.cpp (Main)                          │
│                    Entry point, CLI parsing, SDL init               │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Beast Class (beast.cpp)                        │
│              Main emulation loop, memory management                 │
│                    Coordinates all components                       │
└─────────────────────────────────────────────────────────────────────┘
          │              │              │              │
          ▼              ▼              ▼              ▼
┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│   Z80 CPU   │  │   Z80 PIO   │  │    UART     │  │  VideoBeast │
│   (z80.h)   │  │ (z80pio.h)  │  │(uart16c550) │  │(videobeast) │
└─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘
                       │
                       ▼
              ┌─────────────────┐
              │    I2C Bus      │
              │   (i2c.cpp)     │
              └─────────────────┘
                 │         │
                 ▼         ▼
         ┌───────────┐  ┌───────────┐
         │  Display  │  │    RTC    │
         │(display)  │  │  (rtc)    │
         └───────────┘  └───────────┘
```

## Core Components

### 1. Main Entry Point (`beastem.cpp`)

**Purpose**: Application initialization and command-line processing

**Responsibilities**:
- Parse command-line arguments (-f, -l, -d, -v, -k, -b, -z, -A)
- Initialize SDL2 subsystems
- Initialize SDLNet for network serial
- Create main window
- Instantiate Beast emulator
- Run main loop

**Key Functions**:
- `main()` - Entry point at line 65
- `printHelp()` - CLI help display at line 50
- `isHexNum()`, `isNum()`, `isFloatNum()` - Input validation

### 2. Beast Emulator Core (`src/beast.cpp`, `src/beast.hpp`)

**Purpose**: Central emulation coordinator

**Responsibilities**:
- Memory management (512K ROM, 512K RAM)
- CPU tick coordination
- Peripheral orchestration
- Debug interface
- Keyboard input handling
- Audio generation

**Key Classes/Structures**:
```cpp
class Beast {
    // Memory
    uint8_t rom[ROM_SIZE];  // 512K ROM
    uint8_t ram[RAM_SIZE];  // 512K RAM
    uint8_t memoryPage[4];  // 4x16K page mapping

    // Emulated Hardware
    z80_t cpu;              // Z80 CPU state
    z80pio_t pio;           // Parallel I/O
    uart_t uart;            // Serial UART
    I2c *i2c;               // I2C bus
    VideoBeast *videoBeast; // Video card (optional)

    // Peripherals
    I2cDisplay *display1, *display2;
    I2cRTC *rtc;
};
```

**Emulation Modes**:
- `RUN` - Full speed execution
- `STEP` - Single instruction step
- `OVER` - Step over calls
- `OUT` - Run until return
- `TAKE` - Run until branch taken
- `DEBUG` - Debug view active

### 3. Z80 CPU Emulation (`src/z80.h`)

**Purpose**: Cycle-accurate Z80 processor emulation

**Source**: [Floooh Chips Library](https://github.com/floooh/chips)

**Features**:
- Full Z80 instruction set
- Cycle-accurate timing
- All addressing modes
- Interrupt handling (INT, NMI)
- Pin-level simulation

**Key Functions**:
```cpp
uint64_t z80_init(z80_t* cpu);      // Initialize CPU
uint64_t z80_reset(z80_t* cpu);     // Reset CPU
uint64_t z80_tick(z80_t* cpu, uint64_t pins);  // Execute one cycle
bool z80_opdone(z80_t* cpu);        // Check instruction complete
```

### 4. Z80 PIO (`src/z80pio.h`)

**Purpose**: Parallel I/O controller emulation

**Features**:
- Two 8-bit ports (A and B)
- Multiple operating modes
- Interrupt generation
- I2C bus interface via Port B

### 5. UART (`src/uart16c550.h`)

**Purpose**: Serial communication emulation

**Features**:
- 16C550 compatible
- TX/RX FIFO buffers
- Configurable baud rate
- Network socket bridge (TCP port 8456)

### 6. I2C Bus System (`src/i2c.cpp`, `src/i2c.hpp`)

**Purpose**: Software I2C bus for peripheral communication

**Architecture**:
```cpp
class I2cDevice {
    virtual bool atAddress(uint8_t address) = 0;
    virtual void start() = 0;
    virtual uint8_t readNext() = 0;
    virtual void write(uint8_t value) = 0;
    virtual void stop() = 0;
};

class I2c {
    std::vector<I2cDevice*> devices;
    uint64_t tick(uint64_t* busState, uint64_t time_ps);
};
```

**Connected Devices**:
- I2cDisplay (LED display controller)
- I2cRTC (Real-time clock)

### 7. LED Display (`src/display.cpp`, `src/display.hpp`, `src/digit.cpp`, `src/digit.hpp`)

**Purpose**: 14-segment LED display emulation

**Features**:
- 24 character display (2x IS31FL3730 controllers)
- Individual segment control
- PWM brightness per segment
- SDL2 rendering

### 8. Real-Time Clock (`src/rtc.cpp`, `src/rtc.hpp`)

**Purpose**: RTC chip emulation

**Features**:
- Time/date registers
- Square wave output
- BCD encoding
- Host time synchronization

### 9. VideoBeast (`src/videobeast.cpp`, `src/videobeast.hpp`)

**Purpose**: Advanced video graphics chip emulation

**Features**:
- 1MB video RAM
- Multiple video modes (640x480, 848x480)
- 6 rendering layers
- Layer types: Text, Tile, 4bpp Bitmap, 8bpp Bitmap, Sprite
- Two 256-color palettes
- Hardware multiply unit
- Sinclair screen compatibility mode

**Memory Map**:
| Offset | Purpose |
|--------|---------|
| 0x0000-0x3EFF | Video RAM (paged) |
| 0x3F00-0x3F7F | Palettes/Sprites |
| 0x3F80-0x3FFD | Registers |
| 0x3FFE | Lock register |
| 0x3FFF | Mode register |

### 10. Debug/Development Tools

#### Instructions (`src/instructions.cpp`, `src/instructions.hpp`)
- Z80 instruction decoding
- Disassembly generation
- Control flow analysis

#### Listing (`src/listing.cpp`, `src/listing.hpp`)
- Assembly listing file parsing (TASM, sjsmplus formats)
- Source-level debugging
- Page-aware address mapping

#### GUI (`src/gui.cpp`, `src/gui.hpp`)
- Debug interface rendering
- Value editing
- Prompt dialogs

### 11. File Handling

#### BinaryFile (`src/binaryFile.cpp`, `src/binaryFile.hpp`)
- ROM/RAM file loading
- Multiple destination types (Physical, Paged, Logical, Video RAM)
- File watching for hot-reload

#### Assets (`src/assets.cpp`, `src/assets.hpp`)
- Asset path resolution
- Environment variable support (BEASTEM_ASSETS)
- CLI override (-A flag)

## Memory Architecture

### Z80 Memory Map (64K Logical)
```
0x0000-0x3FFF  Bank 0 (16K) - Pageable
0x4000-0x7FFF  Bank 1 (16K) - Pageable
0x8000-0xBFFF  Bank 2 (16K) - Pageable
0xC000-0xFFFF  Bank 3 (16K) - Pageable
```

### Physical Memory (1MB Total)
```
0x00000-0x7FFFF  ROM (512K) - 32 pages
0x80000-0xFFFFF  RAM (512K) - 32 pages
```

### VideoBeast Memory (1MB)
```
0x00000-0xFFFFF  Video RAM (1MB)
+ 256 bytes registers
+ 512 bytes palettes (2x256)
+ Sprite tables
```

## Data Flow

### Emulation Cycle
1. `Beast::mainLoop()` handles SDL events
2. `Beast::run()` executes CPU cycles
3. `z80_tick()` returns pin state
4. Memory/IO operations based on pins
5. Peripherals updated on relevant cycles
6. Display refreshed at frame rate (50Hz)

### I2C Communication
1. PIO Port B changes detected
2. I2C clock/data lines decoded
3. Address phase identifies target device
4. Read/write operations forwarded to device
5. ACK/NAK handled automatically

## External Dependencies

| Dependency | Purpose | Integration |
|------------|---------|-------------|
| SDL2 | Core graphics/input | CMake find_package |
| SDL2_gfx | 2D primitives | CMake find_package |
| SDL2_ttf | Font rendering | CMake find_package |
| SDL2_net | Network sockets | CMake find_package |
| SDL2_image | Image loading | CMake find_package |
| nativefiledialog | File dialogs | Git submodule |
| Floooh Chips | Z80 emulation | Header-only (vendored) |

## Build Configuration

### CMake Structure
```
CMakeLists.txt           # Main build config
cmake/                   # CMake modules
  FindSDL2_gfx.cmake
  FindSDL2_image.cmake
  FindSDL2_net.cmake
  FindSDL2_ttf.cmake
deps/                    # Dependencies
  nativefiledialog-extended-1.1.1/
```

### Platform Detection
- `PLATFORM_WIN32` - Windows
- `PLATFORM_MACOS` - macOS
- `PLATFORM_LINUX` - Linux

### Compiler Support
- GCC (GNU)
- Clang
- AppleClang
- MSVC
- ClangCL
