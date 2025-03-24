// CS204 - Project Phase 2
// Group - 8
// Members:
// 1. Adarsh Chaudhary (2023CSB1321)
// 2. Deepanshu (2023CSB1117)
// 3. Lavudya Sai Mani Chaitanya (2023CSB1133)

//---------------------------------------------------------------------------------------

// Including the required header files
#include <iostream>
#include <bits/stdc++.h>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cstdint>

using namespace std;

// Function Prototypes
class RISCVSimulator {
public:
    RISCVSimulator();
    void loadMachineCodeFile(const string &filename);
    void simulate();

private:
    // Helper Functions
    string convertIntToHexString(long long num, int numBits);
    long long convertHexStringToInt(const string &hexStr, int numBits);
    unsigned int convertBinaryStringToInt(const string &binStr);
    string convertPCToHex(unsigned int num);
    string incrementAndGenerateNextPC();
    int selectOperandA(bool usePCForA);
    int selectOperandB(bool useImmediateForB);

    // Pipeline Stages
    void fetchInstruction();
    void decodeInstruction();
    void executeInstruction();
    void memoryAccess();
    void writeBack();

    // Instruction Set
    struct InstructionRegister {
        string operation;
        int opcode;
        int rd;
        int funct3;
        int funct7;
        int rs1;
        int rs2;
        long long imm;
    };

    // Data Members
    map<string, string> instructionMemory;
    map<string, string> dataMemory;
    string currentInstruction;
    string currentPC = "0x0";
    unsigned int returnAddress = 0;
    long long int memoryDataRegister;
    long long int aluResult;
    long long int registerY;
    long long int memoryData;
    bool isPCUpdatedDuringExecution = false;
    int registerFile[32] = {0}; // Register file

    long long int clockCycle = 0;

    InstructionRegister ir;
};

// Constructor
RISCVSimulator::RISCVSimulator() {
    // Initialize the register file
    registerFile[2] = 2147483612; // Stack pointer
    registerFile[3] = 268435456;  // Frame pointer
    registerFile[10] = 1;
    registerFile[11] = 2147483612;
}

// Helper Functions
string RISCVSimulator::convertIntToHexString(long long num, int numBits) {
    if (numBits <= 0 || numBits > 64) {
        throw invalid_argument("numBits must be between 1 and 64.");
    }
    uint64_t mask = (numBits == 64) ? ~0ULL : ((1ULL << numBits) - 1);
    uint64_t val = static_cast<uint64_t>(num) & mask;

    int hexDigits = (numBits + 3) / 4;

    stringstream ss;
    ss << "0x" << uppercase << setw(hexDigits) << setfill('0') << hex << val;
    return ss.str();
}

// Convert hexadecimal string to integer
long long RISCVSimulator::convertHexStringToInt(const string &hexStr, int numBits) {
    if (numBits <= 0 || numBits > 64) {
        throw invalid_argument("numBits must be between 1 and 64.");
    }

    uint64_t result = 0;
    int start = 0;

    // Check if the string starts with "0x" or "0X"
    if (hexStr.size() >= 2 && hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X')) {
        start = 2;
    }

    // Convert the hexadecimal string to integer
    for (size_t i = start; i < hexStr.size(); ++i) {
        char c = hexStr[i];
        result *= 16;
        if (c >= '0' && c <= '9') {
            result += c - '0';
        } else if (c >= 'a' && c <= 'f') {
            result += c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            result += c - 'A' + 10;
        } else {
            throw invalid_argument("Invalid character in hexadecimal string");
        }
    }

    uint64_t signBit = (numBits == 64) ? (1ULL << 63) : (1ULL << (numBits - 1));

    // Check if the number is negative
    if (result & signBit) {
        if (numBits == 64) {
            return static_cast<int64_t>(result);
        } else {
            uint64_t subtractVal = (1ULL << numBits);
            return static_cast<long long>(result - subtractVal);
        }
    } else {
        return static_cast<long long>(result);
    }
}

// Convert binary string to integer
unsigned int RISCVSimulator::convertBinaryStringToInt(const string &binStr) {
    unsigned int result = 0;
    for (char c : binStr) {
        result <<= 1;
        if (c == '1') {
            result |= 1;
        } else if (c != '0') {
            throw invalid_argument("Binary string contains invalid characters.");
        }
    }
    return result;
}

// Convert PC to hexadecimal string
string RISCVSimulator::convertPCToHex(unsigned int num) {
    stringstream ss;
    ss << "0x" << hex << num;
    return ss.str();
}

// Increment PC and generate the next PC
string RISCVSimulator::incrementAndGenerateNextPC() {
    unsigned int currentPCInt = convertHexStringToInt(currentPC, 32);
    unsigned int nextPC = currentPCInt + 4;

    if (ir.opcode == 0x63) { // Branch instructions
        bool branchTaken = (aluResult == 1);
        if (branchTaken) {
            nextPC = currentPCInt + ir.imm;
        }
    } else if (ir.opcode == 0x6F) { // JAL
        returnAddress = currentPCInt + 4;
        nextPC = currentPCInt + ir.imm;
    } else if (ir.opcode == 0x67) { // JALR
        nextPC = (registerFile[ir.rs1] + ir.imm) & ~1;
    }

    // Update the PC
    return convertPCToHex(nextPC);
}

// Select operand A
int RISCVSimulator::selectOperandA(bool usePCForA) {
    return usePCForA ? convertHexStringToInt(currentPC, 32) : registerFile[ir.rs1];
}

// Select operand B
int RISCVSimulator::selectOperandB(bool useImmediateForB) {
    return useImmediateForB ? ir.imm : registerFile[ir.rs2];
}

// Pipeline Stages

// Fetch instruction
void RISCVSimulator::fetchInstruction() {
    currentInstruction = instructionMemory[currentPC];
    cout << "FETCH: Instruction-> " << currentInstruction << " , PC-> " << currentPC << endl;
    clockCycle++;
}

// Decode instruction
void RISCVSimulator::decodeInstruction() {
    int instruction = convertBinaryStringToInt(currentInstruction); // Convert the instruction to integer
    ir.opcode = instruction & 0x7F; // Extract the opcode
    ir.rd = (instruction >> 7) & 0x1F; // Extract the destination register
    ir.funct3 = (instruction >> 12) & 0x7; // Extract funct3
    ir.rs1 = (instruction >> 15) & 0x1F; // Extract source register 1
    ir.rs2 = (instruction >> 20) & 0x1F; // Extract source register 2
    ir.funct7 = (instruction >> 25) & 0x7F; // Extract funct7
    ir.imm = 0;

    switch (ir.opcode) {
        // R-type
        case 0x33:
            if (ir.funct3 == 0 && ir.funct7 == 0) ir.operation = "ADD";
            else if (ir.funct3 == 0 && ir.funct7 == 0x20) ir.operation = "SUB";
            else if (ir.funct3 == 0 && ir.funct7 == 0x01) ir.operation = "MUL";
            else if (ir.funct3 == 7) ir.operation = "AND";
            else if (ir.funct3 == 6 && ir.funct7 == 0) ir.operation = "OR";
            else if (ir.funct3 == 4 && ir.funct7 == 0) ir.operation = "XOR";
            else if (ir.funct3 == 4 && ir.funct7 == 0x01) ir.operation = "DIV";
            else if (ir.funct3 == 6 && ir.funct7 == 0x01) ir.operation = "REM";
            else if (ir.funct3 == 1) ir.operation = "SLL";
            else if (ir.funct3 == 2) ir.operation = "SLT";
            else if (ir.funct3 == 5 && ir.funct7 == 0) ir.operation = "SRL";
            else if (ir.funct3 == 5 && ir.funct7 == 0x20) ir.operation = "SRA";
            else ir.operation = "UNKNOWN R-TYPE";
            cout << "DECODE: Operation-> " << ir.operation << ", rs1-> " << registerFile[ir.rs1] << ", rs2-> " << registerFile[ir.rs2] << ", rd-> " << ir.rd << endl;
            break;

        // I-type
        case 0x13:
            ir.imm = (instruction >> 20) & 0xFFF;
            if (ir.imm & 0x800) ir.imm |= 0xFFFFF000;
            if (ir.funct3 == 0) ir.operation = "ADDI";
            else if (ir.funct3 == 7) ir.operation = "ANDI";
            else if (ir.funct3 == 6) ir.operation = "ORI";
            else if (ir.funct3 == 4) ir.operation = "XORI";
            else if (ir.funct3 == 5) ir.operation = "SRLI";
            else if (ir.funct3 == 2) ir.operation = "SLTI";
            else if (ir.funct3 == 1) ir.operation = "SLLI";
            else ir.operation = "UNKNOWN I-TYPE";
            cout << "DECODE: Operation-> " << ir.operation << ", rs1-> " << registerFile[ir.rs1] << ", imm-> " << ir.imm << ", rd-> " << ir.rd << endl;
            break;

        // I-type Load instructions
        case 0x03:
            ir.imm = (instruction >> 20) & 0xFFF;
            if (ir.imm & 0x800) ir.imm |= 0xFFFFF000;
            if (ir.funct3 == 0) ir.operation = "LB";
            else if (ir.funct3 == 1) ir.operation = "LH";
            else if (ir.funct3 == 2) ir.operation = "LW";
            else if (ir.funct3 == 3) ir.operation = "LD";
            else ir.operation = "UNKNOWN LOAD";
            cout << "DECODE: Operation-> " << ir.operation << ", rs1-> " << registerFile[ir.rs1] << ", offset-> " << ir.imm << ", rd-> " << ir.rd << endl;
            break;

        // JALR instruction (I-type)
        case 0x67:
            ir.imm = (instruction >> 20) & 0xFFF;
            if (ir.imm & 0x800) ir.imm |= 0xFFFFF000;
            ir.operation = "JALR";
            cout << "DECODE: Operation-> " << ir.operation << ", rs1-> " << registerFile[ir.rs1] << ", imm-> " << ir.imm << ", rd-> " << ir.rd << endl;
            break;

        // S-type
        case 0x23:
            ir.imm = (((instruction >> 7) & 0x1F) | (((instruction >> 25) & 0x7F) << 5));
            if (ir.imm & 0x800) ir.imm |= 0xFFFFF000;
            if (ir.funct3 == 0) ir.operation = "SB";
            else if (ir.funct3 == 1) ir.operation = "SH";
            else if (ir.funct3 == 2) ir.operation = "SW";
            else if (ir.funct3 == 3) ir.operation = "SD";
            else ir.operation = "UNKNOWN S-TYPE";
            cout << "DECODE: Operation-> " << ir.operation << ", r1-> " << registerFile[ir.rs1] << ", rs2-> " << registerFile[ir.rs2] << ", offset-> " << ir.imm << endl;
            break;

        // SB-type
        case 0x63:
            ir.imm = (((instruction >> 7) & 0x1) << 11) |
                     (((instruction >> 8) & 0xF) << 1) |
                     (((instruction >> 25) & 0x3F) << 5) |
                     (((instruction >> 31) & 0x1) << 12);
            if (ir.imm & 0x1000) ir.imm |= 0xFFFFE000;
            if (ir.funct3 == 0) ir.operation = "BEQ";
            else if (ir.funct3 == 1) ir.operation = "BNE";
            else if (ir.funct3 == 5) ir.operation = "BGE";
            else if (ir.funct3 == 4) ir.operation = "BLT";
            else ir.operation = "UNKNOWN BRANCH";
            cout << "DECODE: Operation-> " << ir.operation << ", rs1-> " << registerFile[ir.rs1] << ", rs2-> " << registerFile[ir.rs2] << ", branch offset-> " << ir.imm << endl;
            break;

        // U-type lui
        case 0x37:
            ir.imm = instruction & 0xFFFFF000;
            ir.operation = "LUI";
            cout << "DECODE: Operation-> " << ir.operation << ", rd-> " << ir.rd << ", imm-> " << ir.imm << endl;
            break;

        // U-type auipc
        case 0x17:
            ir.imm = instruction & 0xFFFFF000;
            ir.operation = "AUIPC";
            cout << "DECODE: Operation-> " << ir.operation << ", rd-> " << ir.rd << ", imm-> " << ir.imm << endl;
            break;

        // UJ-type
        case 0x6F:
            ir.imm = (((instruction >> 21) & 0x3FF) << 1) |
                     (((instruction >> 12) & 0xFF) << 12) |
                     (((instruction >> 20) & 0x1) << 11) |
                     (((instruction >> 31) & 0x1) << 20);
            if (ir.imm & 0x100000) ir.imm |= 0xFFE00000;
            ir.operation = "JAL";
            cout << "DECODE: Operation-> " << ir.operation << ", rd-> " << ir.rd << ", offset-> " << ir.imm << endl;
            break;

        // Unknown opcode
        default:
            cout << "DECODE: Unsupported ir.opcode 0x" << hex << ir.opcode << dec << endl;
            break;
    }
    clockCycle++; // Increment the clock cycle
}

// Execute instruction
void RISCVSimulator::executeInstruction() {
    isPCUpdatedDuringExecution = false; // Reset the flag
    bool usePCReg = false;
    bool useImmVal = false;
    int valB = selectOperandB(useImmVal);
    int valA = selectOperandA(usePCReg);

    if (ir.opcode == 0x33) { // R-type
        if (ir.operation == "ADD") aluResult = valA + valB;
        else if (ir.operation == "SUB") aluResult = valA - valB;
        else if (ir.operation == "AND") aluResult = valA & valB;
        else if (ir.operation == "MUL") aluResult = valA * valB;
        else if (ir.operation == "DIV") aluResult = valA / valB;
        else if (ir.operation == "REM") aluResult = valA % valB;
        else if (ir.operation == "OR") aluResult = valA | valB;
        else if (ir.operation == "XOR") aluResult = valA ^ valB;
        else if (ir.operation == "SLL") aluResult = valA << (valB & 0x1F);
        else if (ir.operation == "SRL") aluResult = (unsigned)valA >> (valB & 0x1F);
        else if (ir.operation == "SRA") aluResult = valA >> (valB & 0x1F);
        else if (ir.operation == "SLT") aluResult = (valA < valB) ? 1 : 0;
        else cout << "EXECUTE: Unsupported R-type instruction encountered." << endl;
        cout << "EXECUTE: Result of R-type execution: " << aluResult << endl;
    } else if (ir.opcode == 0x13) { // I-type
        useImmVal = true;
        valB = selectOperandB(useImmVal);
        if (ir.funct3 == 0) aluResult = valA + valB;
        else if (ir.funct3 == 7) aluResult = valA & valB;
        else if (ir.funct3 == 6) aluResult = valA | valB;
        else if (ir.funct3 == 4) aluResult = valA ^ valB;
        else if (ir.funct3 == 2) aluResult = (valA < valB) ? 1 : 0;
        else if (ir.funct3 == 1) aluResult = valA << (valB & 0x1F);
        else if (ir.funct3 == 5) aluResult = (unsigned)valA >> (valB & 0x1F);
        else cout << "EXECUTE: Unrecognized I-type ALU instruction." << endl;
        cout << "EXECUTE: Computed result for I-type instruction-> " << aluResult << endl;
    } else if (ir.opcode == 0x03) { // Load instruction
        useImmVal = true;
        valB = selectOperandB(useImmVal);
        aluResult = valA + valB;
        cout << "EXECUTE: Memory address calculated for load-> " << aluResult << endl;
    } else if (ir.opcode == 0x23) { // Store instructions (S-type)
        memoryDataRegister = valB;
        useImmVal = true;
        valB = selectOperandB(useImmVal);
        aluResult = valA + valB;
        cout << "EXECUTE: Memory address computed for store operation-> " << aluResult << endl;
    } else if (ir.opcode == 0x63) { // Branch instructions (SB-type)
        aluResult = 0;
        if (ir.funct3 == 0 && (registerFile[ir.rs1] == registerFile[ir.rs2])) aluResult = 1;
        else if (ir.funct3 == 1 && (registerFile[ir.rs1] != registerFile[ir.rs2])) aluResult = 1;
        else if (ir.funct3 == 4 && (registerFile[ir.rs1] < registerFile[ir.rs2])) aluResult = 1;
        else if (ir.funct3 == 5 && (registerFile[ir.rs1] >= registerFile[ir.rs2])) aluResult = 1;
        else aluResult = 0;
        currentPC = incrementAndGenerateNextPC();
        isPCUpdatedDuringExecution = true;
        cout << "EXECUTE: Branch evaluation result-> " << aluResult << endl;
    } else if (ir.opcode == 0x37) { // LUI
        aluResult = ir.imm;
        cout << "EXECUTE: Loaded upper immediate value-> " << aluResult << endl;
    } else if (ir.opcode == 0x17) { // AUIPC
        aluResult = ir.imm + convertHexStringToInt(currentPC, 32);
        cout << "EXECUTE: Computed address for AUIPC instruction-> " << aluResult << endl;
    } else if (ir.opcode == 0x6F) { // JAL
        currentPC = incrementAndGenerateNextPC();
        isPCUpdatedDuringExecution = true;
        cout << "EXECUTE: JAL instruction executed, ALU not involved." << endl;
    } else if (ir.opcode == 0x67) { // JALR
        currentPC = incrementAndGenerateNextPC();
        isPCUpdatedDuringExecution = true;
        cout << "EXECUTE: JALR instruction executed, no ALU operation required." << endl;
    } else { // Unsupported opcode
        cout << "EXECUTE: Unsupported opcode encountered during ALU processing." << endl;
    }
    clockCycle++;
}

// Memory access
void RISCVSimulator::memoryAccess() {
    if (ir.opcode == 0x03) { // Load instruction
        string addressStr;
        string res = "";
        if (ir.funct3 == 0) { // LB
            addressStr = convertIntToHexString(aluResult, 32);
            res += dataMemory[addressStr];
            memoryData = convertHexStringToInt(res, 8);
        } else if (ir.funct3 == 1) { // LH
            long long int x = aluResult + 1;
            while (x >= aluResult) {
                addressStr = convertIntToHexString(x, 32);
                res += dataMemory[addressStr];
                x--;
            }
            memoryData = convertHexStringToInt(res, 16);
        } else if (ir.funct3 == 2) { // LW
            long long int x = aluResult + 3;
            while (x >= aluResult) {
                addressStr = convertIntToHexString(x, 32);
                res += dataMemory[addressStr];
                x--;
            }
            memoryData = convertHexStringToInt(res, 32);
        } else if (ir.funct3 == 3) { // LD
            long long int x = aluResult + 7;
            while (x >= aluResult) {
                addressStr = convertIntToHexString(x, 32);
                res += dataMemory[addressStr];
                x--;
            }
            memoryData = convertHexStringToInt(res, 64);
        }
        cout << "MEMORY: Loaded data-> " << memoryData << " from address-> " << aluResult << endl;
    } else if (ir.opcode == 0x23) { // Store instruction
        string addressStr;
        string data;
        long long int int_address = aluResult;
        if (ir.funct3 == 0) { // SB
            data = convertIntToHexString(memoryDataRegister, 8);
            addressStr = convertIntToHexString(int_address, 32);
            dataMemory[addressStr] = data.substr(2, 2);
        } else if (ir.funct3 == 1) { // SH
            data = convertIntToHexString(memoryDataRegister, 16);
            for (int i = 1; i >= 0; --i) {
                addressStr = convertIntToHexString(int_address, 32);
                int_address += 1;
                dataMemory[addressStr] = data.substr(2 * i + 2, 2);
            }
        } else if (ir.funct3 == 2) { // SW
            data = convertIntToHexString(memoryDataRegister, 32);
            for (int i = 3; i >= 0; --i) {
                addressStr = convertIntToHexString(int_address, 32);
                int_address += 1;
                dataMemory[addressStr] = data.substr(2 * i + 2, 2);
            }
        } else if (ir.funct3 == 3) { // SD
            data = convertIntToHexString(memoryDataRegister, 64);
            for (int i = 7; i >= 0; --i) {
                addressStr = convertIntToHexString(int_address, 32);
                int_address += 1;
                dataMemory[addressStr] = data.substr(2 * i + 2, 2);
            }
        }
        cout << "MEMORY: Stored data-> " << memoryDataRegister << " at address-> " << aluResult << endl;
    } else {
        cout << "MEMORY: No memory access required\n";
    }

    if (ir.opcode == 0x03) { // Load instruction
        registerY = memoryData;
    } else if (ir.opcode == 0x6F) { // JAL
        registerY = returnAddress;
    } else if (ir.opcode == 0x67) { // JALR
        registerY = returnAddress;
    } else if (ir.opcode == 0x23) { // Store instruction
        registerY = 0;
    } else if (ir.opcode == 0x63) { // Branch instruction
        registerY = 0;
    } else {
        registerY = aluResult;
    }
    clockCycle++;
}

// Write back
void RISCVSimulator::writeBack() {
    if (ir.opcode == 0x23 || ir.opcode == 0x63) {
        cout << "WRITEBACK: No write back required\n";
    } else {
        registerFile[ir.rd] = registerY;
        cout << "WRITEBACK: Wrote " << registerY << " to R[" << ir.rd << "]\n";
    }
    registerFile[0] = 0; // x0 is always 0
    clockCycle++;
}

// Load machine code file
void RISCVSimulator::loadMachineCodeFile(const string &filename) {
    ifstream file(filename);
    if (!file) {
        cerr << "Error: Unable to open file " << filename << "\n";
        return;
    }

    string line;
    bool foundTextSegment = true;

    // Skip lines until the "Memory Contents:" line is found
    while (getline(file, line)) {
        if (line.find("Memory Contents:") != string::npos) { // Check if the line contains "Memory Contents:"
            foundTextSegment = false;
            break;
        }
    }

    // Process Data Segment lines (address-value pairs)
    // Skip lines until the "Text Segment:" line is found
    while (getline(file, line)) {
        if (line.find("Text Segment:") != string::npos) { // Check if the line contains "Text Segment:"
            foundTextSegment = true;
            break;
        }

        // Check if the line contains an address-value pair
        if (line.find(":") != string::npos) {
            istringstream iss(line);
            string address, arrow, value;
            if (iss >> address >> arrow >> value && arrow == ":") {
                dataMemory[address] = value;
            }
        }
    }

    // Process the Text Segment lines (PC and machine code)
    if (foundTextSegment) {
        while (getline(file, line)) {
            if (line.empty()) continue;
            istringstream iss(line);
            string pc, code;

            // Extract the PC and machine code
            if (iss >> pc >> code) {
                if (code == "Terminate") {
                    instructionMemory[pc] = code;
                    break;
                }
                instructionMemory[pc] = code;
            }
        }
    }

    file.close();
}

// Simulate the RISC-V processor
void RISCVSimulator::simulate() {
    loadMachineCodeFile("output.mc");

    cout << "Simulation started...\n";
    cout << "----------------------------------------------------------------------------------------------------\n";
    while (instructionMemory[currentPC] != "Terminate") {
        fetchInstruction();
        decodeInstruction();
        executeInstruction();
        memoryAccess();
        writeBack();
        if (!isPCUpdatedDuringExecution) {
            currentPC = incrementAndGenerateNextPC();
        }
        cout << "Clock: " << clockCycle << endl;
        for (auto reg : registerFile) {
            cout << reg << " ";
        }
        cout << endl;
        cout << "----------------------------------------------------------------------------------------------------\n";
    }

    cout << "\nSimulation completed after " << clockCycle << " clock cycles.\n";

    // Write the register file contents to a file named "data_memory.txt"
    ofstream dataMemoryFile("data_memory.txt");
    if (!dataMemoryFile) {
        cerr << "Error: Unable to create data_memory.txt" << endl;
        return;
    }
    // Write the data memory contents to the file
    dataMemoryFile << "Data Memory:\n";
    for (const auto &entry : dataMemory) {
        dataMemoryFile << "Address: " << entry.first << " -> Value: " << entry.second << "\n";
    }
}

int main() {
    RISCVSimulator simulator;
    simulator.simulate();
    return 0;
}