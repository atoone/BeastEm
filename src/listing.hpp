/**
 * listing.hpp - Z80 Assembly Listing File Parser
 *
 * Provides source-level debugging by parsing assembler listing files.
 * Maps memory addresses back to source code lines, enabling the debugger
 * to display the original assembly source at the current program counter.
 *
 * Supports banked/paged memory: each listing file is associated with a
 * memory page, and addresses are stored as physical addresses
 * (page << 14 | offset) where offset is the 14-bit position within the 16KB bank.
 *
 * Usage:
 *   Listing listing;
 *   int idx = listing.addFile("program.lst", 0, true);  // page 0, auto-watch
 *   listing.loadFile(listing.getFiles()[idx]);          // parse the file
 *
 *   // Later, in debugger:
 *   uint32_t physAddr = (currentPage << 14) | (cpu.pc & 0x3FFF);
 *   auto loc = listing.getLocation(physAddr);
 *   if (loc.valid) {
 *       auto [line, ok] = listing.getLine(loc);
 *       if (ok) displaySourceLine(line.text);
 *   }
 */

#pragma once
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <string>
#include <map>
#include <vector>

class Listing {

    public:
        /**
         * Represents a position in a source file.
         * Used as the value type in the address-to-source mapping.
         */
        struct Location {
            unsigned int fileNum;   // Index into sources vector
            unsigned int lineNum;   // 1-based line number in the file
            bool valid;             // false if address not found in any listing
        };

        /**
         * A single parsed line from a listing file.
         *
         * Listing files typically look like:
         *   "  42   0100 3E 42       LD A, 42h"
         *
         * This struct captures both the display text and the extracted data.
         */
        struct Line {
            std::string text;       // Original line text (for display)
            std::string head;       // Text before the address (line number, etc.)
            uint32_t    address;    // 16-bit logical address from this line
            uint8_t     bytes[4];   // Up to 4 machine code bytes
            int         byteCount;  // Number of bytes extracted (0-4)
            bool        isData = false;  // true if line has bytes but no source text
                                         // (e.g., continuation of DB/DW directive)
        };

        /**
         * A loaded source/listing file.
         *
         * Each source file is associated with a memory page for physical
         * address calculation. The 'watch' feature enables auto-reload
         * when the file is modified (useful during development).
         */
        struct Source {
            std::string shortname;  // Filename without path (for display)
            std::string filename;   // Full path to the listing file
            unsigned int fileNum;   // Index in sources vector
            int page;               // Memory page/bank (0-31) for physical addressing
            std::vector<Line> lines;  // All parsed lines from the file
            bool watch;             // If true, check for file modifications
            std::filesystem::file_time_type lastRead;  // For change detection
        };

        // --- File Management ---

        /**
         * Checks if a file exists and appears to be a valid listing.
         * Does not load the file, just validates format.
         */
        bool    isValidFile(std::string filename);

        /**
         * Registers a listing file for tracking.
         * Call loadFile() afterward to actually parse it.
         *
         * @param filename  Full path to the listing file
         * @param page      Memory page (0-31) - determines physical address range
         * @param watch     Enable auto-reload on file change
         * @return          File index, or -1 if invalid listing format
         */
        int     addFile(std::string filename, int page, bool watch);

        /**
         * Parses a listing file and builds the address mapping.
         * Can be called again to reload after file changes.
         */
        void    loadFile(Source &source);

        /**
         * Removes a file from tracking. Cleans up lineMap entries.
         */
        void    removeFile(unsigned int fileNum);

        /**
         * Returns the number of loaded source files.
         */
        size_t  fileCount();

        /**
         * Returns mutable reference to all source files.
         * Used for iteration, file selection UI, etc.
         */
        std::vector<Source>& getFiles();

        // --- Address Lookup ---

        /**
         * Finds the source location for a physical address.
         *
         * @param address  Physical address: (page << 14) | (logical_address & 0x3FFF)
         * @return         Location with valid=false if not found
         */
        Location getLocation(uint32_t address);

        /**
         * Retrieves the Line data for a source location.
         *
         * @param location  From getLocation()
         * @return          Pair of (Line, success). Check second before using first.
         */
        std::pair<Line, bool> getLine(Location location);

        // --- File Watching ---

        /**
         * Returns true if the file has watch enabled.
         */
        bool    isWatched(Source &file);

        /**
         * Returns true if the file has been modified since last loadFile().
         * Only meaningful if isWatched() is true.
         */
        bool    isUpdated(Source &file);

        /**
         * Toggles the watch flag for a source file.
         */
        void    toggleWatch(Source &file);

    private:
        /** All loaded source files */
        std::vector<Source> sources;

        /**
         * Maps physical addresses to source locations.
         *
         * Key: physical address = (page << 14) | (logical_address & 0x3FFF)
         * Value: Location (fileNum, lineNum)
         *
         * This enables O(1) lookup of source code for any address.
         */
        std::map<uint32_t, Location> lineMap;

        /**
         * Regex pattern to extract address from listing lines.
         *
         * Pattern: ^[0-9]+(\\++\\s*|\\s+)([0-9a-f]{4})
         *
         * Breakdown:
         *   ^[0-9]+       - Line starts with line number
         *   (\\++\\s*|\\s+) - Followed by '+' signs (macro) or whitespace
         *   ([0-9a-f]{4}) - Capture group 2: 4-digit hex address
         *
         * Examples that match:
         *   "1    0000 3E"     -> address = 0000
         *   "42+  1234 CD"     -> address = 1234 (macro expansion)
         *   "100  ABCD"        -> address = ABCD
         */
        const char* addressRegex = "^[0-9]+(\\++\\s*|\\s+)([0-9a-f]{4})";

        /**
         * Validates that a file appears to be a text listing (not binary).
         */
        bool isValidListing(std::ifstream& stream);

        /**
         * Converts a hex character ('0'-'9', 'a'-'f', 'A'-'F') to int (0-15).
         */
        int fromHex(char c);

};
