#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <functional>

class Instructions {

    struct FlowOpcode {
        uint8_t prefix;
        uint8_t opcode;
        uint8_t mask;
        uint8_t flags;
        int     dir;   // 1 -> in (ie. call)  0 -> neither (ie jump) -1 -> out (ie return)
    };

    

    public:
        Instructions();
        void resetStack();
        bool isOut(uint8_t op1, uint8_t op2);
        bool isTaken(uint8_t op1, uint8_t op2, uint8_t flags);
        
        bool isJumpOrReturn(uint8_t op1, uint8_t op2);
        bool isConditional(uint8_t op1, uint8_t op2);

        struct Opcode {
                uint8_t opcode;
                std::string mnemonic;
                uint8_t length;
                uint8_t cycles;
                bool isIXIY;
            };

        int instructionLength(uint8_t op1, uint8_t op2);
        std::string decodeOpcode(std::string mnemonic, uint16_t address, std::function<uint8_t(uint16_t)> fetch);
        std::string decode(uint16_t address, std::function<uint8_t(uint16_t)> fetch, int *length);

 
    private:
        void parseOpcode(std::vector<std::string>parts, int column, uint8_t opcode, Opcode *opcodeArray);
        int stack = 0;

        const FlowOpcode FLOW_OPCODES[37] = {
        FlowOpcode {0x00, 0xCD, 0, 0, 1},   // Call
        
        FlowOpcode {0x00, 0xDC, 0x01, 0x01, 1},   // Call - Carry
        FlowOpcode {0x00, 0xD4, 0x01, 0x00, 1},   // Call - No Carry
        FlowOpcode {0x00, 0xCC, 0x40, 0x40, 1},   // Call - Zero
        FlowOpcode {0x00, 0xC4, 0x40, 0x00, 1},   // Call - Non Zero
        FlowOpcode {0x00, 0xEC, 0x04, 0x04, 1},   // Call - Parity Even
        FlowOpcode {0x00, 0xE4, 0x04, 0x00, 1},   // Call - Parity Odd
        FlowOpcode {0x00, 0xFC, 0x80, 0x80, 1},   // Call - Sign neg
        FlowOpcode {0x00, 0xF4, 0x80, 0x00, 1},   // Call - Sign pos

        FlowOpcode {0x00, 0xC9, 0, 0, -1},  // Return

        FlowOpcode {0x00, 0xD8, 0x01, 0x01, -1},   // Return - Carry
        FlowOpcode {0x00, 0xD0, 0x01, 0x00, -1},   // Return - No Carry
        FlowOpcode {0x00, 0xC8, 0x40, 0x40, -1},   // Return - Zero
        FlowOpcode {0x00, 0xC0, 0x40, 0x00, -1},   // Return - Non Zero
        FlowOpcode {0x00, 0xE8, 0x04, 0x04, -1},   // Return - Parity Even
        FlowOpcode {0x00, 0xE0, 0x04, 0x00, -1},   // Return - Parity Odd
        FlowOpcode {0x00, 0xF8, 0x80, 0x80, -1},   // Return - Sign neg
        FlowOpcode {0x00, 0xF0, 0x80, 0x00, -1},   // Return - Sign pos
        
        FlowOpcode {0xED, 0x4D, 0, 0, -1},  // RETI
        FlowOpcode {0xED, 0x45, 0, 0, -1},  // RETN

        FlowOpcode {0x00, 0xC3, 0, 0, 0},  // JP

        FlowOpcode {0x00, 0xDA, 0x01, 0x01, 0},   // JP - Carry
        FlowOpcode {0x00, 0xD2, 0x01, 0x00, 0},   // JP - No Carry
        FlowOpcode {0x00, 0xCA, 0x40, 0x40, 0},   // JP - Zero
        FlowOpcode {0x00, 0xC2, 0x40, 0x00, 0},   // JP - Non Zero
        FlowOpcode {0x00, 0xEA, 0x04, 0x04, 0},   // JP - Parity Even
        FlowOpcode {0x00, 0xE2, 0x04, 0x00, 0},   // JP - Parity Odd
        FlowOpcode {0x00, 0xFA, 0x80, 0x80, 0},   // JP - Sign neg
        FlowOpcode {0x00, 0xF2, 0x80, 0x00, 0},   // JP - Sign pos

        FlowOpcode {0x00, 0xE9, 0, 0, 0},   // JP (HL)   
        FlowOpcode {0xDD, 0xE9, 0, 0, -1},  // JP (IX)
        FlowOpcode {0xFD, 0xE9, 0, 0, -1},  // JP (IY)

        FlowOpcode {0x00, 0x18, 0, 0, 0},  // JR

        FlowOpcode {0x00, 0x38, 0x01, 0x01, 0},   // JR - Carry
        FlowOpcode {0x00, 0x30, 0x01, 0x00, 0},   // JR - No Carry
        FlowOpcode {0x00, 0x28, 0x40, 0x40, 0},   // JR - Zero
        FlowOpcode {0x00, 0x20, 0x40, 0x00, 0},   // JR - Non Zero
        };

        Opcode OPCODES[256] = {0};

        Opcode CB_OPCODES[256] = {0};
        Opcode ED_OPCODES[256] = {0};
        Opcode IXYCB_OPCODES[256] = {0};
};
