/**
 * listing.cpp - Z80 Assembly Listing File Parser
 *
 * This module parses assembler listing files (e.g., from z80asm, pasmo, etc.)
 * to enable source-level debugging in the emulator. Listing files contain
 * both the original source code and the generated machine code with addresses.
 *
 * Example listing file format (varies by assembler):
 *   1    0000 3E 42       LD A, 42h
 *   2    0002 C9          RET
 *   3+   0003             ; comment
 *
 * The parser extracts:
 *   - Memory addresses (e.g., 0000, 0002)
 *   - Machine code bytes (e.g., 3E 42, C9)
 *   - Source text for display
 *
 * This allows the debugger to show source code at the current PC address.
 */

#include "listing.hpp"
#include <cctype>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

/**
 * Trims leading whitespace from a string in-place.
 */
static inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
}

// Platform-specific path separator
const char filePathSeparator =
#ifdef _WIN32
    '\\';
#else
    '/';
#endif

/**
 * Returns reference to all loaded source files.
 */
std::vector<Listing::Source> &Listing::getFiles() { return sources; }

/**
 * Removes a source file from the listing manager.
 *
 * After removal, renumbers all remaining files and their lineMap entries
 * to maintain contiguous file numbering.
 *
 * @param fileNum  Index of the file to remove
 */
void Listing::removeFile(unsigned int fileNum) {
  sources.erase(sources.begin() + fileNum);

  // Renumber remaining source files
  for (auto &source : sources) {
    if (source.fileNum > fileNum) {
      unsigned int newFileNum = source.fileNum -1;
      source.fileNum = newFileNum;
      for (auto& symbol:source.symbols) {
        symbol.fileNum = newFileNum;
      }
    }
  }

  // Update lineMap: remove entries for deleted file, renumber others
  for (auto it = lineMap.begin(); it != lineMap.end();) {
    if (it->second.fileNum == fileNum)
      it = lineMap.erase(it);
    else {
      if (it->second.fileNum > fileNum) {
        it->second.fileNum = it->second.fileNum - 1;
      }
      ++it;
    }
  }

  updateSymbolMap();
}

/**
 * Adds a new listing file to be tracked.
 *
 * Does not immediately parse the file - call loadFile() to parse.
 *
 * @param filename  Full path to the listing file
 * @param page      Memory page/bank number (0-31) - used to create
 *                  physical addresses: (page << 14) | (logical_addr & 0x3FFF)
 * @param watch     If true, auto-reload when file changes on disk
 * @return          File index, or -1 if file is not a valid listing
 */
int Listing::addFile(std::string filename, int page, bool watch) {
  std::ifstream myfile(filename);
  if (!myfile) {
    std::cout << "Listing file does not exist: " << filename << std::endl;
    exit(1);
  }
  bool isValid = isValidListing(myfile);
  myfile.close();

  if (!isValid)
    return -1;

  unsigned int fileNum = sources.size();

  // Extract short filename for display (strip directory path)
  std::size_t pos = filename.find_last_of(filePathSeparator);
  std::string shortname = filename;

  if (pos != std::string::npos && ((pos + 2) < filename.length())) {
    shortname = filename.substr(pos + 1);
  }
  Source source = {shortname, filename, fileNum, page, {}, {}, watch};

  sources.push_back(source);
  return sources.size() - 1;
}

/**
 * Checks if a file exists and appears to be a valid listing file.
 */
bool Listing::isValidFile(std::string filename) {
  std::ifstream myfile(filename);
  if (!myfile) {
    return false;
  }
  bool isValid = isValidListing(myfile);
  myfile.close();
  return isValid;
}

/**
 * Heuristic validation of a listing file.
 *
 * Checks the first 4KB of the file to ensure it's likely a text listing:
 *   - No null bytes (would indicate binary file)
 *   - No lines longer than 300 chars (would indicate minified/binary)
 *   - At least 5 bytes of content
 *
 * @param stream  Input file stream (will be rewound after check)
 * @return        true if file appears to be a valid listing
 */
bool Listing::isValidListing(std::ifstream &stream) {
  char buffer[4096];
  stream.read(buffer, sizeof(buffer));
  int length = stream.gcount();

  stream.seekg(0, std::ios::beg); // Rewind for subsequent reading

  if (length < 5) {
    return false;
  }
  int lineLength = 0;

  for (int i = 0; i < length; i++) {
    if (buffer[i] == '\0')
      return false; // Binary file detected

    if (buffer[i] == '\n' || buffer[i] == '\r') {
      if (lineLength > 300) {
        return false; // Line too long - probably not a listing
      }
      lineLength = 0;
    } else {
      lineLength++;
    }
  }

  if (lineLength > 300) {
    return false;
  }

  return true;
}

/**
 * Parses a listing file and builds the address-to-line mapping.
 *
 * This is the core parsing function. It reads each line and uses a regex
 * to extract the address and machine code bytes.
 *
 * Regex pattern: ^[0-9]+(\\++\\s*|\\s+)([0-9a-f]{4})
 *   - Matches lines starting with a line number
 *   - Followed by optional '+' signs (for macro expansions) or whitespace
 *   - Followed by a 4-digit hex address
 *
 * Example matches:
 *   "1    0000 3E 42  LD A,42h"  -> address 0x0000, bytes [3E, 42]
 *   "2+   0002 C9     RET"       -> address 0x0002, bytes [C9]
 *
 * The lineMap stores: physical_address -> (fileNum, lineNum)
 * where physical_address = (page << 14) | (logical_address & 0x3FFF)
 *
 * @param source  Source file struct to populate with parsed lines
 */
void Listing::loadFile(Source &source) {

  // Clear any previously parsed data for this file
  source.lines.clear();
  source.symbols.clear();

  //std::cout << "Clearing old lines " << source.fileNum << std::endl;
  for (auto it = lineMap.begin(); it != lineMap.end();) {
    // TODO: Peformance fix here
    if (it->second.fileNum == source.fileNum) {
      it = lineMap.erase(it);
    } else {
      ++it;
    }
  }
  //std::cout << "Lines cleared" << std::endl;

  uint32_t address = 0;
  bool foundAddress = false;
  unsigned int lineNum = 0;
  unsigned int addressLine = 0; // Line number where current address was first seen

  std::string text;

  source.lastRead = std::filesystem::last_write_time(source.filename);
  std::ifstream myfile(source.filename);

  if (!isValidListing(myfile)) {
    std::cout << "File '" << source.filename
              << "' does not appear to be a valid listing. Did you mean to "
                 "load a binary file with '-f'?"
              << std::endl;
    return;
  }

  bool isLabelMap = false;

  while (std::getline(myfile, text)) {
    Line line = {};
    line.text = text; // Store original text for display

    if (text.length() == 0) {
      continue;
    }

    ltrim(text);

    if (!isLabelMap ) {
      // Try to match the address pattern in this line
      std::regex matcher = std::regex(addressRegex, std::regex::icase);
      std::smatch match;
      if (std::regex_search(text, match, matcher)) {
        uint32_t nextAddress = std::stoi(match.str(2), nullptr, 16);

        // When address changes, record the mapping for the previous address
        // This handles multi-line instructions and data blocks
        if (foundAddress && (nextAddress != address)) {
          Location loc = {source.fileNum, addressLine, true, source.page};
          // Create physical address: (page << 14) | 14-bit offset
          // Use assignment so the most recently loaded listing takes precedence
          // when multiple listings map to the same address
          lineMap[(source.page << 14) | (address & 0x3FFF)] = loc;
        }

        foundAddress = true;
        address = nextAddress;
        addressLine = lineNum;  // 0-based index (lineNum before push_back)

        line.address = address;
        line.head = text.substr(
            0, match.position(2)); // Text before address (line num, etc.)

        // Parse up to 4 machine code bytes following the address
        // Format: "ADDR XX XX XX XX" where XX are hex byte pairs
        int byteCount = 0;
        unsigned int bytePos = match.position(2) + match.length(2);

        while (byteCount < 4 && ((bytePos + 2) < text.length()) &&
              (text[bytePos] == ' ') && isxdigit(tolower(text[bytePos + 1])) &&
              isxdigit(tolower(text[bytePos + 2]))) {
          line.bytes[byteCount++] =
              (fromHex(text[bytePos + 1]) << 4) | fromHex(text[bytePos + 2]);
          bytePos += 3;
        }

        if (bytePos + 2 < text.length()) {
          if (text[bytePos] == '.' && text[bytePos + 1] == '.' && text[bytePos + 2] == '.') {
            // DEFS can result in a truncated byte sequence indicated by a '...' string - skip if so.
            bytePos += 3;
          }
        }

        addSymbol(text, line, source);

        line.byteCount = byteCount;

        // Check if this line contains only data (no source code after bytes)
        // Used to distinguish data directives (DB, DW) from instructions
        if (byteCount > 0) {
          line.isData = true;

          while (bytePos < text.length()) {
            if (!isspace(text[bytePos++])) {
              line.isData = false; // Found non-whitespace => has source code
              break;
            }
          }
        }
      } else {
        if( text.rfind("tasm:", 0) == 0 ) {
          // We ignore tasm output
        }
        else if( text.rfind("# ", 0) == 0) {
          // SJAsmPlus telling us things.
        }
        else if( text.rfind("Value", 0) == 0) {
          // SJAsmPlus symbol map follows
          source.symbols.clear(); // Forget the labels we've scraped and just use the symbol map
          isLabelMap = true;
        }
        else {
          std::cout << "No match on line " << lineNum << ": " << text << std::endl;
        }
      }

      source.lines.push_back(line);
      lineNum++;  // Increment after push_back so addressLine is 0-based index
    }
    else {
      // SJAsmPlus label format:
      // 0x0010 X module.labelName
      //
      if (text.length() > 9 && text.rfind("0x",0) == 0) {
        if (text[7] != 'X') { // Skip unused labels
          uint32_t value = std::stoi(text.substr(2,4), nullptr, 16);
          std::string label = text.substr(9);
          Symbol symbol = {label, value, source.page};
          source.symbols.push_back(symbol);
        }
      }
    }
  }
  myfile.close();

  // Don't forget to record the final address
  if (foundAddress) {
    Location loc = {source.fileNum, addressLine, true, source.page};
    lineMap[(source.page << 14) | (address & 0x3FFF)] = loc;
  }

  updateSymbolMap();
  std::cout << "Parsed listing, file " << source.filename << " for page "
            << source.page << " has " << lineNum << " lines, " << source.symbols.size() << " label(s)." << std::endl;
}

void Listing::addSymbol( std::string &text, Listing::Line &line, Listing::Source &source) {
  // Now see if we have a label..
  size_t index = symbolColumn;

  if (index + 2 < text.length()) {
    if (NON_LABEL_CHARS.find(text[index]) == std::string::npos) {
      // Not whitespace or local label indicator..
      std::string label = "";
      while (index < text.length()) {
        if (text[index] == ':' || text[index] == ' ' || text[index] == '\t')
          break;
        label += text[index++];
      }

      bool isEquate = false;
      while (++index < text.length() && (text[index] == ' ' || text[index] == '\t') ) {}

      if( index < text.length() && text[index] == '.') index++;

      if (text.rfind("equ", index) == index || text.rfind("EQU", index) == index) {
        isEquate = true;
      }

      if (!isEquate) {
        Symbol symbol = {label, line.address, source.page, source.fileNum};
        source.symbols.push_back(symbol);
        // std::cout << "Label '" << label << " = " << line.address << std::endl;
      }
    }
  }
}

void Listing::updateSymbolMap() {
  symbolMap.clear();
  for(auto &source: sources) {
    for (auto &symbol: source.symbols) {
      symbolMap.insert(symbol);
    }
  }
}

/**
 * Converts a hex character to its numeric value.
 * @return 0-15 for valid hex chars, -1 for invalid
 */
int Listing::fromHex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  return -1; // unexpected char
}

/**
 * Returns the number of loaded source files.
 */
size_t Listing::fileCount() { return sources.size(); }

/**
 * Looks up the source location for a given physical address.
 *
 * @param address  Physical address: (page << 14) | (logical_addr & 0x3FFF)
 * @return         Location with fileNum, lineNum, and valid flag
 */
Listing::Location Listing::getLocation(uint32_t address) {
  auto search = lineMap.find(address);
  if (search != lineMap.end()) {
    return search->second;
  }

  return Location() = {0, 0, false};
}

/**
 * Retrieves the parsed Line data for a given source location.
 *
 * @param location  Source location from getLocation()
 * @return          Pair of (Line data, success flag)
 */
std::pair<Listing::Line, bool> Listing::getLine(Location location) {
  if (location.valid && location.fileNum < sources.size() &&
      location.lineNum < sources[location.fileNum].lines.size()) {
    return std::pair<Listing::Line, bool>(
        sources[location.fileNum].lines[location.lineNum], true);
  }
  return std::pair<Listing::Line, bool>(Listing::Line{}, false);
}

/**
 * Returns whether a source file is being watched for changes.
 */
bool Listing::isWatched(Source &file) { return file.watch; }

/**
 * Checks if a watched file has been modified since last load.
 * Used to trigger auto-reload when source is recompiled.
 */
bool Listing::isUpdated(Source &file) {
  if (file.watch) {
    return file.lastRead < std::filesystem::last_write_time(file.filename);
  }
  return false;
}

/**
 * Toggles the watch status for a source file.
 */
void Listing::toggleWatch(Source &file) { file.watch = !file.watch; }

void Listing::lookup(std::string match) {
  symbolLookup.clear();

  if (match.length()<2) {
    return;
  }

  bool filterFile = false;
  unsigned int  fileNum = 0;

  if (match[1] == ':' && std::isdigit(match[0])) {
    fileNum = match[0]-'0';
    if (fileNum-- > 0) {
      filterFile = true;
      match = match.substr(2);

      if (match.length()<2) {
        return;
      }
    }
  }

  for (auto & symbol: symbolMap) {
    bool found = true;
    size_t i = match.length();

    if (filterFile && symbol.fileNum != fileNum) continue;
    if (i>symbol.label.length()) continue;

    while(i-->0) {
      if(tolower(match[i]) != tolower(symbol.label[i])) {
        found = false;
        break;
      }
    }
    if (found) {
      symbolLookup.push_back(symbol);
    }
    
    if (symbolLookup.size()>MAX_LOOKUP) break;
  }

 
  for(auto & symbol: symbolMap) {
    if (filterFile && symbol.fileNum != fileNum) continue;
    if (symbolLookup.size()>MAX_LOOKUP) break;

    size_t k = 0;
    for (size_t i = 0; i < symbol.label.size(); ++i)
      if (tolower(match[k]) == tolower(symbol.label[i])) {
        k++;
        if (k == match.size()) {
            symbolLookup.push_back(symbol);
            break;
        }
      }
  }

}

size_t Listing::matches() {
  return symbolLookup.size();
}

std::string Listing::getLabel(size_t index) {
  if (index>symbolLookup.size()) return "";
  return symbolLookup[index].label;
}

int Listing::getValue(size_t index) {
  if (index>symbolLookup.size()) return 0;

  return symbolLookup[index].value;
}

std::string Listing::getDescription1(size_t index) {
    if (index>symbolLookup.size()) return "";

    Symbol& symbol = symbolLookup[index];
    return GUI::string_format("0x%04X  Physical: 0x%05X", symbol.value, (symbol.value & 0x3FFF) | (symbol.page << 14));
}

std::string Listing::getDescription2(size_t index) {
    if (index>symbolLookup.size()) return "";

    Symbol& symbol = symbolLookup[index];
    return GUI::string_format("File %d: %-14s", symbol.fileNum+1, sources[symbol.fileNum].filename.c_str());
}