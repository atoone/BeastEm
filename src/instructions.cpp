#include "instructions.hpp"
#include <iostream>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>

// Opcode file from https://spectrumforeveryone.com/technical/z80-processor-instructions/
//
Instructions::Instructions() {
    std::ifstream myfile("z80_instr.txt");
    std::string line;
    int lineNum = 0;

    while (std::getline(myfile, line)) {
        if( lineNum++ == 0 ) continue;

        std::stringstream input(line);
        std::string part;
        std::vector<std::string> parts;

        while( std::getline(input, part, '\t')) {
            parts.push_back(part);
        }
        uint8_t opcode = 0;

        if( parts.size() > 3 ) {
            opcode = std::stoi(parts[0], nullptr, 16);
            parseOpcode(parts, 1, opcode, OPCODES);
        }
        if( parts.size() > 6 ) {
            parseOpcode(parts, 4, opcode, CB_OPCODES);
        }
        if( parts.size() > 12 ) {
            parseOpcode(parts, 10, opcode, ED_OPCODES);
        }
    }
    myfile.close();
}

void Instructions::parseOpcode(std::vector<std::string>parts, int column, uint8_t opcode, Opcode *opcodeArray) {
    if( parts[column+1].size() == 0 ) {
        return;
    }

    uint8_t length = std::stoi(parts[column+1]);

    if( length > 0 ) {
        uint8_t cycles = std::stoi(parts[column+2]);

        std::string mnemonic = parts[column];

        mnemonic.erase(std::remove(mnemonic.begin(), mnemonic.end(), '"'), mnemonic.end());

        std::size_t found = parts[1].find("(HL)");

        opcodeArray[opcode].opcode = opcode;
        opcodeArray[opcode].mnemonic = mnemonic;
        opcodeArray[opcode].length = length;
        opcodeArray[opcode].cycles = cycles;
        opcodeArray[opcode].isIXIY = found != std::string::npos;
    }
}

void Instructions::resetStack() {
    stack = 0;
}

bool Instructions::isOut(uint8_t op1, uint8_t op2) {
    for( auto flow: FLOW_OPCODES ) {
        if( (flow.prefix == 0x00 && flow.opcode == op1) ||
            (flow.prefix != 0 && flow.prefix == op1 && flow.opcode == op2) ) {
            stack += flow.dir;
            break;
        }
    }

    return stack < 0;
}

bool Instructions::isJumpOrReturn(uint8_t op1, uint8_t op2) {
    for( auto flow: FLOW_OPCODES ) {
        if( (flow.prefix == 0x00 && flow.opcode == op1) ||
            (flow.prefix != 0 && flow.prefix == op1 && flow.opcode == op2) ) {
            return flow.dir <= 0 && flow.mask == 0;
        }
    }

    return false;
}

bool Instructions::isConditional(uint8_t op1, uint8_t op2) {
    for( auto flow: FLOW_OPCODES ) {
        if( (flow.prefix == 0x00 && flow.opcode == op1) ||
            (flow.prefix != 0 && flow.prefix == op1 && flow.opcode == op2) ) {
            return flow.mask != 0;
        }
    }

    return false;
}

bool Instructions::isTaken(uint8_t op1, uint8_t op2, uint8_t flags) {
    for( auto flow: FLOW_OPCODES ) {
        if( (flow.prefix == 0x00 && flow.opcode == op1) ||
            (flow.prefix != 0 && flow.prefix == op1 && flow.opcode == op2) ) {
            return (flags & flow.mask) == flow.flags;
        }
    }

    return false;
}

std::string Instructions::decodeOpcode(Opcode *opcode, uint16_t address, std::function<uint8_t(uint16_t)> fetch) {
    std::string decoded = opcode->mnemonic;
    std::size_t pos = decoded.find('$');
    char buff[10];
    if( pos != std::string::npos) {
        uint8_t val = fetch(address);
        snprintf(buff, sizeof(buff), "0x%02X", val);
        decoded.replace(pos, 1, buff);
    }
    pos = decoded.find('^');
    if( pos != std::string::npos) {
        uint16_t val_l = fetch(address++);
        uint16_t val_h = fetch(address);
        snprintf(buff, sizeof(buff), "0x%04X", val_l+(val_h<<8));
        decoded.replace(pos, 1, buff);
    }
    pos = decoded.find('%');
    if( pos != std::string::npos) {
        int8_t offset = fetch(address++);
        uint16_t dest = address+offset;
        snprintf(buff, sizeof(buff), "0x%04X", dest);
        decoded.replace(pos, 1, buff);
    }
    return decoded;
}

std::string Instructions::decode(uint16_t address, std::function<uint8_t(uint16_t)> fetch, int *length) {
    uint8_t op1 = fetch(address++);
    uint8_t op2 = fetch(address);

    *length = 0;

    if( op1 == 0xCB ) {
        for(auto opcode: CB_OPCODES) {
            if( opcode.opcode == op2 ) {
                *length += opcode.length;
                return decodeOpcode(&opcode, address, fetch);
            }
        }
        return "Unknown";
    }
    if( op1 == 0xED ) {
        for(auto opcode: ED_OPCODES) {
            if( opcode.opcode == op2 ) {
                *length += opcode.length;
                return decodeOpcode(&opcode, address, fetch);
            }
        }
        return "Unknown";
    }
    for(auto opcode: OPCODES) {
        if( opcode.opcode == op1 ) {
            if( *length > 0 && opcode.isIXIY ) {
                *length++;
            }
            *length += opcode.length;
            return decodeOpcode(&opcode, address, fetch);
        }
    }

    return "unknown";
}

int Instructions::instructionLength(uint8_t op1, uint8_t op2) {
    if( op1 == 0xCB ) {
        return 2;
    }
    if( op1 == 0xED ) {
        for(auto opcode: ED_OPCODES) {
            if( opcode.opcode == op2 && opcode.length > 0 ) {
                return opcode.length;
            }
        }
        return -1;
    }

    int length = 0;

    if( op1 == 0xDD || op1 == 0xFD ) {
        if( op2 == 0xDD || op2 == 0xFD || op2 == 0xED ) {
            return -1;
        }
        if( op2 == 0xCB ) {
            return 4;
        }
        length = 1;
        op1 = op2;
    }

    for(auto opcode: OPCODES) {
        if( opcode.opcode == op1 ) {
            if( length > 0 && opcode.isIXIY ) {
                length++;
            }
            return length + opcode.length;
        }
    }
    return -1;
}
