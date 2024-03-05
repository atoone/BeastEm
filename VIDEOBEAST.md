# Getting started with VideoBeast

## Enabling VideoBeast in BeastEm

To enable VideoBeast emulation, start BeastEm with either `-d` or `-d2` command line arguments followed by the filename of a video memory file. Either option will open a VideoBeast window once BeastEm starts, but the `-d2` option will magnify the display by a factor of two. VideoBeast initialises its 1Mb video RAM with the specified file - and the emulator includes a demonstration file, `videobeast.dat`

Once it is enabled, to access VideoBeast features from MicroBeast it needs to be paged into the Z80 memory map. BeastEm emulates a host interface with VideoBeast at page `0x40`. Depending on how you are using MicroBeast, there are two options for paging VideoBeast into memory.

## Option 1: Paging with MicroBeast BIOS

If you are using VideoBeast with CP/M or other MicroBeast BIOS features, such as the memory editor you must let the BIOS handle paging. The BIOS normally processes interrupts to read the keyboard and write to the display and can change page mapping unless you let it know which pages you wish to access. Therefore, to page VideoBeast into memory, you must call the BIOS routine at `0xFDE2: Map_Page`. An example call would be:

```
3E 01                       LD A, 01       ; Bank 1      :  0x4000 - 0x7FFF
1E 40                       LD E, 0x40    ; Page 0x40 : VideoBeast
C3 E2 FD                 JP  0xFDE2
```

This will map the 16Kb VideoBeast page into the Z80 memory map from address `0x4000` to `0x7FFF`. Note that the BIOS routine will RETurn at the end, so the snippet above should be CALLed to work.

You can try this out by starting the memory editor in MicroBeast (Cursor down to find it from the main clock display), and editing from address `0x8000` - enter the byte sequence for the snippet above, `3E 01 1E 40 C3 E2 FD`, then move the cursor to the start of the sequence and hit `X` to execute. 

Now, navigate to address `0x4000` (hit delete to stop editing values, then Enter to go to a new address). The default register configuration (specified in the `video_registers.mem` file) boots VideoBeast with the start of video RAM paged in and configured as a Text layer. Editing the first two bytes from address `0x4000` to `65 10` should display the character `e` on screen. Try different values to see how Text layers behave.

## Option 2: Standalone code

If you are writing bare code that does not rely on the MicroBeast BIOS, you can page memory directly. Paging is handled by writing the desired page to one of four IO ports (for the four 16K banks that make up the Z80 memory map). To test things out, disabling interrupts gives you control over the system without the BIOS changing pages. The following snippet illustrates the basic structure of a program.

```
            ORG 0x8000
main        DI                ; Disable interrupts 
            LD     A, 0x40    ; VideoBeast page 
            OUT    (0x71), A  ; Update the mapping for bank 1 
						
            ... 
            Write to VideoBeast in bank 1, addresses 0x4000-0x7FFF 
            ... 
            EI                ; Re-enable interrupts 
            RET 
```

# Intial State of VideoBeast (the videobeast.dat file)

VideoBeast displays screens based on a set of Registers, two Palettes and its video RAM. These are initialised with individual files that are read when BeastEm starts. The Registers and Palettes are loaded with the contents of `video_registers.mem`, `palette_1.mem` and `palette_2.mem` which are text files that specify the respective values to start with. 

## The Register file

The register file consists of values for the 128 registers (#80 to #FF) that control layer configuration and the overall state of VideoBeast. These are arranged in descending order, with the first to characters of each line giving the hexadecimal byte value for a register. Check the [online documentation](https://feertech.com/microbeast/videobeast_doc.html) for register definitions and explanation of the layer system.

## The Palette files

VideoBeast has two palettes of 256 colours each. The first palette is used for all 16 colour (4 bit per pixel) layer types (Text layers, Tiles, 4 bit Bitmaps and Sprites). The 256 colours are treated as 16 palettes each of 16 colours. The second palette is used for 256 colour (8bpp) layer types (8 bit Bitmaps).

Each of the palette files specifies a palette entry per line, with the four digit hexadecimal value representing a 5:5:5 RGB colour. VideoBeast uses the top three bits of each colour, and the 16th bit indicates that the palette entry is transparent (e.g. `8000` is a transparent value, `001C` is bright blue).

Palettes are loaded at start-up and can then be updated through the VideoBeast memory map.

## The Video RAM file

BeastEm includes a demonstration memory file (raw binary) that contains text, font, tile and bitmap graphics for display. VideoBeast layers define where to read graphic data from and how to display it. This means that the memory map is completely flexible according to the needs of the software.

The layout of the data in `videobeast.dat` is as follows:

| Address  | Offset  | Size |Data                 |
|-----------|---------|------|------|
| 0x00000 |  0Kb     |  16Kb | Text layer (128 x 64 characters+attributes) |
| 0x04000 | 16Kb    |  16Kb | Text layer  - Undrium 1bpp background     |
| 0x08000 | 32Kb    |   2Kb  | Font data - normal text                               |
| 0x08800 | 34Kb    |   2Kb  | Font data - Undrium 1bpp background     |
| 0x10000 | 64Kb    |  16Kb | Tile Map (128 x 64 tiles) Undrium layer 1   |
| 0x14000 | 80Kb    |  16Kb | Tile Map (128 x 64 tiles) Undrium layer 2   |
| 0x18000 | 96Kb    |  16Kb | Tile Map (128 x 64 tiles) Undrium layer 3   |
| 0x1C000 | 112Kb    |  16Kb | Tile Map (128 x 64 tiles) Undrium layer 4   |
| 0x20000 | 128Kb    |  32Kb | Tile Graphics (1024 tiles) Undrium tiles      |
| 0x40000 | 256Kb    |  256Kb | Beast bitmap - 4bpp                                 |
| 0x80000 | 512Kb   | 256Kb  | High-colour bitmap - 8bpp                       |

VideoBeast can be configured through its registers to show various combinations of these graphics, and MicroBeast can write new data by writing to the 16Kb bank it has mapped to VideoBeast. 

# Writing to VideoBeast Registers and Memory

VideoBeast is controlled purely through memory access, acting like a 16Kb bank of memory that the host (MicroBeast or another 8-bit computer) can read and write to.

Within that 16Kb bank, the top area of memory can be configured to access various registers, the palettes and sprite lists. The remaining addresses are mapped by VideoBeast to one or more offsets within its video RAM. Check the [online documentation](https://feertech.com/microbeast/videobeast_doc.html) for the most up to date documentation of register layout and behaviour.

Note that the address within the 16Kb area that VideoBeast provides can be thought of as an offset (`0x0000` to `0x3FFF`), but depending which bank MicroBeast has mapped to access VideoBeast, the Z80 will write to a bank address. So if VideoBeast is paged into Bank 1 (which the Z80 sees as addresses `0x4000` to `0x7FFF`), then offset `0x0000` appears at address `0x4000`, and offset `0x3FFF` appears at address `0x7FFF` from the perspective of the host CPU. VideoBeast offsets are always in terms of it's 16Kb host address space, ie. from `0x0000` to `0x3FFF` regardless of which MicroBeast bank it is mapped to.

The top two bytes are always accessible as registers, with offset `0x3FFF` controlling the screen mode and memory mapping, and offset `0x3FFE` controlling access to the full register set. Setting the byte at offset `0x3FFE` to `0xF3` unlocks the full register set which is then accessible through the top 256 bytes of memory.

The address space below the registers can itself be thought of as one or more banks (mapping within mapping!) that are mapped to locations within VideoBeast RAM. On startup it is configured as one continuous 16Kb bank that is mapped to address `0x00000` in VideoBeast memory. The base of this bank is adjustable in 4Kb increments by writing to the register at offset `0x3FF9`. So writing `0x10` to offset `0x3FF9` will set the base address in Video Ram to 16 * 4 = 64Kb. 

Various mappings are available, including two 8Kb banks (allowing us to copy between different locations in VideoBeast RAM) and a special Sinclair mapping that decodes host addresses in the Spectrum screen layout into VideoBeast Text layer addresses. 

# Trying it out

Coming soon - putting this into practise in the emulator.
