# BeastEm - Development Guide

## Prerequisites

### All Platforms
- C++ compiler with C++17 support (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.7 or higher
- Git

### Linux (Debian/Ubuntu)
```bash
sudo apt-get install build-essential cmake git \
    libsdl2-dev libsdl2-net-dev libsdl2-image-dev \
    libsdl2-ttf-dev libsdl2-gfx-dev gtk3-dev
```

### Linux (Fedora/RHEL)
```bash
sudo dnf install gcc gcc-c++ cmake git \
    SDL2-devel SDL2_net-devel SDL2_image-devel \
    SDL2_ttf-devel SDL2_gfx-devel gtk3-devel
```

### macOS
```bash
brew install cmake sdl2 sdl2_gfx sdl2_image sdl2_net sdl2_ttf
```

### Windows
- Install MSYS2 with MinGW-w64
- Or use Visual Studio with CMake support
- SDL2 development libraries required

## Building

### Standard Build (Linux/macOS)
```bash
# Clone the repository
git clone <repository-url>
cd beastem

# Configure and build
cmake .
make clean all
```

### Manual Build (without CMake)
```bash
g++ -w -O2 -o beastem beastem.cpp src/*.cpp \
    -I/usr/include/SDL2 -D_REENTRANT \
    -lSDL2 -lSDL2_ttf -lSDL2_gfx -lSDL2_net
```

### macOS Apple Silicon
```bash
clang++ -w --std=c++20 -O2 -o beastem beastem.cpp src/*.cpp \
    -I /opt/homebrew/include/SDL2 -D_REENTRANT \
    -lSDL2 -lSDL2_ttf -lSDL2_gfx -lSDL2_net
```

### Debug Build
```bash
cmake -DCMAKE_BUILD_TYPE=Debug .
make
```

## Running

### Basic Usage
```bash
# Run with default firmware
./beastem

# Run with custom ROM
./beastem -f myprogram.bin

# Run with ROM at specific address
./beastem -f 8000 myprogram.bin
```

### Command Line Options

| Option | Description | Example |
|--------|-------------|---------|
| `-f [addr] file` | Load binary at address | `-f 8000 prog.bin` |
| `-fw [addr] file` | Load binary, watch for changes | `-fw prog.bin` |
| `-l [page] file` | Load listing file | `-l 0 firmware.lst` |
| `-lw [page] file` | Load listing, watch for changes | `-lw code.lst` |
| `-d file` | Enable VideoBeast (1x) | `-d videobeast.dat` |
| `-d2 file` | Enable VideoBeast (2x zoom) | `-d2 videobeast.dat` |
| `-a device` | Audio device ID | `-a 0` |
| `-s rate` | Audio sample rate (0=off) | `-s 22050` |
| `-v volume` | Volume (0-10) | `-v 5` |
| `-k speed` | CPU speed in KHz | `-k 8000` |
| `-b addr` | Initial breakpoint | `-b 8000` |
| `-z zoom` | UI zoom factor | `-z 1.5` |
| `-A path` | Asset directory path | `-A ./assets` |

### Environment Variables

| Variable | Description |
|----------|-------------|
| `BEASTEM_ASSETS` | Path to assets directory |

## Debug Controls

### Debug View (press ESC to enter)

| Key | Action |
|-----|--------|
| `R` | Run until ESC or breakpoint |
| `S` | Single step one instruction |
| `O` | Step over (run until next instruction) |
| `U` | Step out (run until return) |
| `T` | Run until branch taken |
| `L` | Toggle listing view |
| `E` | Soft reset CPU |
| `F` | File management menu |
| `B` | Toggle/edit breakpoint |
| `D` | Disconnect network terminal |
| `A` | Toggle audio file recording |
| `Q` | Quit |
| `Up/Down` | Select debug item |
| `Left/Right` | Modify selected item |
| `Enter` | Edit selected value |

### Memory Views
- `Z80` - Logical 64K memory map
- `PAGE` - Physical memory by page
- `Video RAM` - VideoBeast memory (when enabled)

## Project Structure

### Adding a New Peripheral

1. Create header file in `src/`:
```cpp
// src/mydevice.hpp
#pragma once
#include "i2c.hpp"

class MyDevice : public I2cDevice {
public:
    MyDevice(uint8_t address);
    bool atAddress(uint8_t addr) override;
    void start() override;
    uint8_t readNext() override;
    void write(uint8_t value) override;
    void stop() override;
};
```

2. Implement in `src/mydevice.cpp`

3. Add to `CMakeLists.txt`:
```cmake
add_executable(beastem
    ...
    src/mydevice.cpp
)
```

4. Register with I2C bus in `beast.cpp`:
```cpp
MyDevice *myDevice = new MyDevice(0x30);
i2c->addDevice(myDevice);
```

### Adding a New CLI Option

1. Add parsing in `beastem.cpp` main():
```cpp
else if (strcmp(argv[index], "-x") == 0) {
    if (index+1 >= argc) {
        std::cout << "Missing argument" << std::endl;
        printHelp();
        exit(1);
    }
    myOption = argv[++index];
}
```

2. Update `printHelp()`:
```cpp
std::cout << "   -x <value>  : My new option" << std::endl;
```

### Modifying Emulation Timing

CPU speed is controlled by `targetSpeedHz` in `Beast::init()`:
- Default: 8000 KHz (8 MHz)
- Timing uses picoseconds for precision
- `clock_cycle_ps` = 1e12 / targetSpeedHz

## Code Conventions

### Naming
- Classes: `PascalCase`
- Methods: `camelCase`
- Constants: `UPPER_SNAKE_CASE`
- Member variables: `camelCase` (no prefix)

### Headers
- Use `#pragma once` for include guards
- Include SDL headers as `<SDL.h>` not `"SDL.h"`

### Memory
- Use RAII where possible
- Raw pointers for SDL resources (managed by SDL)
- `std::vector` for dynamic arrays

## Testing

### Manual Testing Checklist

- [ ] Starts with default firmware
- [ ] Debug view displays correctly
- [ ] Single stepping works
- [ ] Breakpoints trigger
- [ ] Keyboard input works in run mode
- [ ] Display updates correctly
- [ ] Audio produces sound
- [ ] Network serial connects
- [ ] File loading works
- [ ] VideoBeast renders (if enabled)

### Test Files
- Default firmware: `assets/flash_v1.7.bin`
- Listing files: `assets/firmware.lst`, `assets/monitor.lst`
- VideoBeast demo: `assets/videobeast.dat`

## Troubleshooting

### Build Issues

**SDL2 not found:**
```
Could not find SDL2
```
- Install SDL2 development packages
- Check CMake module path includes `cmake/` directory

**Undefined reference to SDL functions:**
- Ensure all SDL2 libraries are installed (-dev packages)
- Check link order in manual builds

### Runtime Issues

**Window doesn't appear:**
- Check SDL2 is properly initialized
- Verify display drivers are working

**No audio:**
- List audio devices: check SDL_GetNumAudioDevices()
- Try different device with `-a` option
- Set sample rate to 0 to disable: `-s 0`

**Network serial not connecting:**
- Default port is 8456
- Check firewall settings
- Verify with `telnet localhost 8456`

## Resources

- [MicroBeast Wiki](https://github.com/atoone/MicroBeast/wiki)
- [Z80 Instruction Set](http://z80.info/z80-op.txt)
- [SDL2 Documentation](https://wiki.libsdl.org/)
- [Floooh Chips](https://github.com/floooh/chips)
- [VideoBeast Docs](https://feertech.com/microbeast/videobeast_doc.html)
