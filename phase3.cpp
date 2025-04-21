// CS204 - Project Phase 3
// Group - 8
// Members:
// 1. Adarsh Chaudhary (2023CSB1321)
// 2. Deepanshu (2023CSB1117)
// 3. Lavudya Sai Mani Chaitanya (2023CSB1133)

//---------------------------------------------------------------------------------------


#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <bitset>

class PipelinedCPU {
private:
    // Configuration Knobs
    bool pipeliningEnabled = true;
    bool dataForwardingEnabled = true;
    bool printRegistersEnabled = false;
    bool printPipelineRegsEnabled = true;
    int traceInstructionNum = -1;
    bool printBPUEnabled = false;

    // Core Components
    std::map<std::string, std::string> registerFile;
    std::map<std::string, std::string> textSegment;
    std::map<std::string, std::string> dataMemory;

    // Branch Predictor
    class BranchPredictor {
    public:
        std::map<uint64_t, int> historyTable;
        std::map<uint64_t, uint64_t> targetBuffer;
        
        uint64_t predictions = 0;
        uint64_t mispredictions = 0;
        uint64_t btbHits = 0;
        uint64_t btbMisses = 0;

        bool predictTaken(uint64_t pc) {
            predictions++;
            return historyTable.count(pc) ? historyTable[pc] == 1 : false;
        }
        
        bool isInBTB(uint64_t pc) {
            return targetBuffer.count(pc);
        }

        uint64_t getPredictedTarget(uint64_t pc) {
            if (targetBuffer.count(pc)) {
                btbHits++;
                return targetBuffer[pc];
            } else {
                btbMisses++;
                return pc + 4;
            }
        }

        void update(uint64_t pc, bool actuallyTaken, uint64_t actualTarget) {
            int currentState = historyTable.count(pc) ? historyTable[pc] : 0;
            bool predictedTaken = (currentState == 1);

            if (predictedTaken != actuallyTaken) {
                mispredictions++;
            }

            historyTable[pc] = actuallyTaken ? 1 : 0;

            if (actuallyTaken) {
                targetBuffer[pc] = actualTarget;
            }
        }

        std::string toString() {
            std::ostringstream oss;
            oss << "BPU State: Predictions=" << predictions 
                << ", Mispredictions=" << mispredictions 
                << ", BTB Hits=" << btbHits 
                << ", BTB Misses=" << btbMisses << "\n";
            oss << " History Table (PC -> State[0=NT,1=T]):\n";
            
            if (historyTable.empty()) {
                oss << "  <Empty>\n";
            } else {
                for (const auto& entry : historyTable) {
                    oss << "  0x" << std::hex << std::setw(8) << std::setfill('0') << entry.first 
                        << " -> " << std::dec << entry.second << "\n";
                }
            }
            
            oss << " Branch Target Buffer (PC -> Target):\n";
            if (targetBuffer.empty()) {
                oss << "  <Empty>\n";
            } else {
                for (const auto& entry : targetBuffer) {
                    oss << "  0x" << std::hex << std::setw(8) << std::setfill('0') << entry.first 
                        << " -> 0x" << std::setw(8) << std::setfill('0') << entry.second << "\n";
                }
            }
            return oss.str();
        }
    } bpu;

    // Pipeline Registers
    struct IFIDRegister {
        uint64_t instructionPC = 0;
        std::string instruction = "0x00000000";
        uint64_t nextPC = 0;
        uint64_t instructionNumber = 0;
        bool valid = false;
        bool predictedTaken = false;
        uint64_t predictedTarget = 0;

        void clear() {
            instruction = "0x00000000";
            valid = false;
            instructionNumber = 0;
            predictedTaken = false;
            predictedTarget = 0;
        }

        std::string toString() {
            char buf[256];
            snprintf(buf, sizeof(buf), 
                "IF/ID [Valid:%s]: PC=0x%08lX, IR=%s, NextPC=0x%08lX, Inst#:%lu, PredTaken:%s, PredTarget=0x%08lX",
                valid ? "true" : "false", instructionPC, instruction.c_str(), nextPC, instructionNumber,
                predictedTaken ? "true" : "false", predictedTarget);
            return std::string(buf);
        }
    } if_id_reg;

    struct IDEXRegister {
        // Control Signals
        std::string aluOp = "NOP";
        bool regWrite = false;
        bool memRead = false;
        bool memWrite = false;
        bool branch = false;
        bool jump = false;
        bool useImm = false;
        int writeBackMux = 0;
        std::string memSize = "WORD";

        // Data
        uint64_t instructionPC = 0;
        uint64_t nextPC = 0;
        uint64_t readData1 = 0;
        uint64_t readData2 = 0;
        uint64_t immediate = 0;
        int rs1 = 0;
        int rs2 = 0;
        int rd = 0;
        std::string debugInstruction = "0x00000000";
        uint64_t instructionNumber = 0;
        bool valid = false;

        void clear() {
            aluOp = "NOP";
            regWrite = false;
            memRead = false;
            memWrite = false;
            branch = false;
            jump = false;
            useImm = false;
            valid = false;
            instructionNumber = 0;
            debugInstruction = "0x00000000";
        }

        std::string toString() {
            char buf[512];
            snprintf(buf, sizeof(buf),
                "ID/EX [Valid:%s]: PC=0x%08lX, Inst#:%lu, Ctrl:[%s,RW:%s,MR:%s,MW:%s,Br:%s,Jmp:%s,Imm:%s,WBMux:%d,Sz:%s], "
                "RVal1=0x%lX, RVal2=0x%lX, Imm=0x%lX, rs1=x%d, rs2=x%d, rd=x%d, IR=%s",
                valid ? "true" : "false", instructionPC, instructionNumber, aluOp.c_str(),
                regWrite ? "true" : "false", memRead ? "true" : "false", memWrite ? "true" : "false",
                branch ? "true" : "false", jump ? "true" : "false", useImm ? "true" : "false",
                writeBackMux, memSize.c_str(), readData1, readData2, immediate, rs1, rs2, rd,
                debugInstruction.substr(2).c_str());
            return std::string(buf);
        }
    } id_ex_reg;

    struct EXMEMRegister {
        // Control Signals
        bool regWrite = false;
        bool memRead = false;
        bool memWrite = false;
        bool branchTaken = false;
        std::string debugInstruction = "0x00000000";
        int writeBackMux = 0;
        std::string memSize = "WORD";

        // Data
        uint64_t instructionPC = 0;
        uint64_t aluResult = 0;
        uint64_t writeData = 0;
        uint64_t branchTarget = 0;
        int rd = 0;
        bool valid = false;
        uint64_t instructionNumber = 0;

        void clear() {
            regWrite = false;
            memRead = false;
            memWrite = false;
            valid = false;
            instructionNumber = 0;
        }

        std::string toString() {
            char buf[512];
            snprintf(buf, sizeof(buf),
                "EX/MEM [Valid:%s]: Inst#:%lu, Ctrl:[RW:%s,MR:%s,MW:%s,BrTaken:%s,WBMux:%d,Sz:%s], "
                "ALURes=0x%lX, WriteData=0x%lX, BrTarget=0x%lX, rd=x%d",
                valid ? "true" : "false", instructionNumber, 
                regWrite ? "true" : "false", memRead ? "true" : "false", 
                memWrite ? "true" : "false", branchTaken ? "true" : "false",
                writeBackMux, memSize.c_str(), aluResult, writeData, branchTarget, rd);
            return std::string(buf);
        }
    } ex_mem_reg, ex_debug;

    struct MEMWBRegister {
        // Control Signals
        bool regWrite = false;
        int writeBackMux = 0;

        // Data
        uint64_t instructionPC = 0;
        uint64_t aluResult = 0;
        uint64_t readData = 0;
        int rd = 0;
        std::string debugInstruction = "0x00000000";
        bool valid = false;
        uint64_t instructionNumber = 0;

        void clear() {
            regWrite = false;
            valid = false;
            instructionNumber = 0;
        }

        std::string toString() {
            char buf[256];
            snprintf(buf, sizeof(buf),
                "MEM/WB [Valid:%s]: Inst#:%lu, Ctrl:[RW:%s,WBMux:%d], "
                "ALURes=0x%lX, ReadData=0x%lX, rd=x%d",
                valid ? "true" : "false", instructionNumber, 
                regWrite ? "true" : "false", writeBackMux,
                aluResult, readData, rd);
            return std::string(buf);
        }
    } mem_wb_reg;

    // Processor State
    uint64_t pc = 0x0;
    uint64_t clockCycle = 0;
    uint64_t instructionCount = 0;
    bool hazardStall = false;
    bool dataForwardingStall = false;
    bool branchMispredictFlush = false;

    static const std::string NOP_INSTRUCTION;
    static const std::string ZERO_REG;
    static const uint64_t INITIAL_SP = 0x7FFFFFDC;

public:
    PipelinedCPU() {
        // Initialize register file
        for (int i = 0; i < 32; i++) {
            registerFile["x" + std::to_string(i)] = formatHex(0);
        }
        registerFile["x2"] = formatHex(INITIAL_SP); // Initialize stack pointer
    }

private:
    // Utility Methods
    static std::string formatHex(uint64_t value) {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
        return oss.str();
    }

    uint64_t parseHex(const std::string& hex) {
        if (hex.empty() || hex.substr(0, 2) != "0x") {
            std::cerr << "Warning: Invalid hex string format: " << hex << std::endl;
            return 0;
        }
        return std::stoul(hex.substr(2), nullptr, 16);
    }

    uint64_t signExtend(const std::string& binary, int bitWidth) {
        uint64_t value = std::stoul(binary, nullptr, 2);
        uint64_t mask = 1ULL << (bitWidth - 1);
        if ((value & mask) != 0) {
            uint64_t signBits = ~((1ULL << bitWidth) - 1);
            value |= signBits;
        }
        return value;
    }

    uint64_t readMemory(uint64_t address, const std::string& size) {
        uint64_t value = 0;
        int bytes = (size == "BYTE") ? 1 : (size == "HALF") ? 2 : 4;

        for (int i = 0; i < bytes; i++) {
            std::string byteAddressHex = formatHex(address + i);
            std::string byteStr = dataMemory.count(byteAddressHex) ? dataMemory[byteAddressHex] : "00";
            uint64_t byteValue = std::stoul(byteStr, nullptr, 16);
            value |= (byteValue << (i * 8));
        }

        // Handle sign extension
        if (size == "BYTE" && (value & 0x80)) {
            value |= 0xFFFFFFFFFFFFFF00ULL;
        } else if (size == "HALF" && (value & 0x8000)) {
            value |= 0xFFFFFFFFFFFF0000ULL;
        }

        return value;
    }

    void writeMemory(uint64_t address, uint64_t data, const std::string& size) {
        int bytes = (size == "BYTE") ? 1 : (size == "HALF") ? 2 : 4;

        for (int i = 0; i < bytes; i++) {
            std::string byteAddressHex = formatHex(address + i);
            uint64_t byteValue = (data >> (i * 8)) & 0xFF;
            char buf[3];
            snprintf(buf, sizeof(buf), "%02lX", byteValue);
            dataMemory[byteAddressHex] = buf;
        }
    }

    // Pipeline Stages
    void instructionFetch() {
        if (hazardStall || dataForwardingStall) {
            return;
        }

        if (branchMispredictFlush) {
            branchMispredictFlush = false;
        }

        std::string instructionHex = textSegment.count(formatHex(pc)) ? textSegment[formatHex(pc)] : NOP_INSTRUCTION;
        uint64_t currentPC = pc;
        uint64_t pcPlus4 = pc + 4;

        // Check if this might be a branch/jump
        bool mightBeBranch = false;
        uint64_t instructionVal = parseHex(instructionHex);
        int opcode = instructionVal & 0x7F;
        
        if (opcode == 0b1100011 || opcode == 0b1101111 || opcode == 0b1100111) {
            mightBeBranch = true;
        }

        // Branch Prediction
        uint64_t predictedNextPC = pcPlus4;
        bool predictedTaken = false;
        
        if (bpu.isInBTB(currentPC)) {
            predictedTaken = bpu.predictTaken(currentPC);
            if (predictedTaken) {
                predictedNextPC = bpu.getPredictedTarget(currentPC);
                if (printBPUEnabled) {
                    std::cout << "BPU: Predicting branch at 0x" << std::hex << currentPC 
                              << " as TAKEN to 0x" << predictedNextPC << " (BTB hit)\n";
                }
            } else if (printBPUEnabled) {
                std::cout << "BPU: Predicting branch at 0x" << std::hex << currentPC 
                          << " as NOT TAKEN (BTB hit but not taken)\n";
            }
        } else if (mightBeBranch) {
            predictedTaken = bpu.predictTaken(currentPC);
            if (predictedTaken) {
                if (printBPUEnabled) {
                    std::cout << "BPU: Predicting branch at 0x" << std::hex << currentPC 
                              << " as TAKEN but target unknown (BTB miss)\n";
                }
                predictedNextPC = pcPlus4;
            } else if (printBPUEnabled) {
                std::cout << "BPU: Predicting branch at 0x" << std::hex << currentPC 
                          << " as NOT TAKEN\n";
            }
        }

        // Prepare for next cycle
        if_id_reg.instructionPC = currentPC;
        if_id_reg.instruction = instructionHex;
        if_id_reg.nextPC = pcPlus4;
        if_id_reg.valid = true;
        if_id_reg.instructionNumber = instructionCount++;
        if_id_reg.predictedTaken = predictedTaken;
        if_id_reg.predictedTarget = predictedNextPC;

        pc = predictedTaken ? predictedNextPC : pcPlus4;
    }

    void instructionDecode() {
        if (!if_id_reg.valid) {
            id_ex_reg.clear();
            return;
        }

        std::string instruction = if_id_reg.instruction;
        uint64_t instructionPC = if_id_reg.instructionPC;
        uint64_t nextPC = if_id_reg.nextPC;

        id_ex_reg.clear();
        id_ex_reg.instructionPC = instructionPC;
        id_ex_reg.nextPC = nextPC;
        id_ex_reg.debugInstruction = instruction;
        id_ex_reg.instructionNumber = if_id_reg.instructionNumber;
        id_ex_reg.valid = true;

        if (instruction == NOP_INSTRUCTION) {
            id_ex_reg.aluOp = "NOP";
            return;
        }

        // Decode Instruction
        uint64_t instructionVal = parseHex(instruction);
        int opcode = instructionVal & 0x7F;
        id_ex_reg.rd = (instructionVal >> 7) & 0x1F;
        int funct3 = (instructionVal >> 12) & 0x7;
        id_ex_reg.rs1 = (instructionVal >> 15) & 0x1F;
        id_ex_reg.rs2 = (instructionVal >> 20) & 0x1F;
        int funct7 = (instructionVal >> 25) & 0x7F;

        // Read Registers
        std::string rs1Name = "x" + std::to_string(id_ex_reg.rs1);
        std::string rs2Name = "x" + std::to_string(id_ex_reg.rs2);
        id_ex_reg.readData1 = parseHex(registerFile.count(rs1Name) ? registerFile[rs1Name] : formatHex(0));
        id_ex_reg.readData2 = parseHex(registerFile.count(rs2Name) ? registerFile[rs2Name] : formatHex(0));

        // Generate Control Signals and Immediate
        id_ex_reg.regWrite = false;
        id_ex_reg.memRead = false;
        id_ex_reg.memWrite = false;
        id_ex_reg.branch = false;
        id_ex_reg.jump = false;
        id_ex_reg.useImm = false;
        id_ex_reg.writeBackMux = 0;
        id_ex_reg.memSize = "WORD";

        switch (opcode) {
            case 0b0110011: // R-Type
                id_ex_reg.aluOp = decodeRType(funct3, funct7);
                id_ex_reg.regWrite = true;
                break;

            case 0b0010011: // I-Type
                id_ex_reg.immediate = signExtend(std::bitset<12>((instructionVal >> 20)).to_string(), 12);
                id_ex_reg.aluOp = decodeITypeArith(funct3, funct7);
                id_ex_reg.useImm = true;
                id_ex_reg.regWrite = true;
                break;

            case 0b0000011: // Load
                id_ex_reg.immediate = signExtend(std::bitset<12>((instructionVal >> 20)).to_string(), 12);
                id_ex_reg.aluOp = "ADD";
                id_ex_reg.memRead = true;
                id_ex_reg.useImm = true;
                id_ex_reg.regWrite = true;
                id_ex_reg.writeBackMux = 1;
                switch (funct3) {
                    case 0b000: id_ex_reg.memSize = "BYTE"; break;
                    case 0b001: id_ex_reg.memSize = "HALF"; break;
                    case 0b010: id_ex_reg.memSize = "WORD"; break;
                    default: id_ex_reg.aluOp = "INVALID"; break;
                }
                break;

            case 0b1100111: // JALR
                id_ex_reg.immediate = signExtend(std::bitset<12>((instructionVal >> 20)).to_string(), 12);
                id_ex_reg.aluOp = "JALR";
                id_ex_reg.regWrite = true;
                id_ex_reg.useImm = true;
                id_ex_reg.jump = true;
                id_ex_reg.writeBackMux = 2;
                break;

            case 0b0100011: // S-Type
                {
                    uint64_t imm_4_0 = (instructionVal >> 7) & 0x1F;
                    uint64_t imm_11_5 = (instructionVal >> 25) & 0x7F;
                    uint64_t immSVal = (imm_11_5 << 5) | imm_4_0;
                    id_ex_reg.immediate = signExtend(std::bitset<12>(immSVal).to_string(), 12);
                    id_ex_reg.aluOp = "ADD";
                    id_ex_reg.memWrite = true;
                    id_ex_reg.useImm = true;
                    switch (funct3) {
                        case 0b000: id_ex_reg.memSize = "BYTE"; break;
                        case 0b001: id_ex_reg.memSize = "HALF"; break;
                        case 0b010: id_ex_reg.memSize = "WORD"; break;
                        default: id_ex_reg.aluOp = "INVALID"; break;
                    }
                }
                break;

            case 0b1100011: // B-Type
                {
                    uint64_t imm_11 = (instructionVal >> 7) & 0x1;
                    uint64_t imm_4_1 = (instructionVal >> 8) & 0xF;
                    uint64_t imm_10_5 = (instructionVal >> 25) & 0x3F;
                    uint64_t imm_12 = (instructionVal >> 31) & 0x1;
                    uint64_t immBVal = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
                    id_ex_reg.immediate = signExtend(std::bitset<13>(immBVal).to_string(), 13);
                    id_ex_reg.aluOp = decodeBType(funct3);
                    id_ex_reg.branch = true;
                }
                break;

            case 0b0110111: // LUI
                id_ex_reg.immediate = signExtend(std::bitset<32>(instructionVal & 0xFFFFF000).to_string(), 32);
                id_ex_reg.aluOp = "LUI";
                id_ex_reg.regWrite = true;
                id_ex_reg.useImm = true;
                break;

            case 0b0010111: // AUIPC
                id_ex_reg.immediate = signExtend(std::bitset<32>(instructionVal & 0xFFFFF000).to_string(), 32);
                id_ex_reg.aluOp = "AUIPC";
                id_ex_reg.regWrite = true;
                id_ex_reg.useImm = true;
                break;

            case 0b1101111: // JAL
                {
                    uint64_t imm_20 = (instructionVal >> 31) & 0x1;
                    uint64_t imm_10_1 = (instructionVal >> 21) & 0x3FF;
                    uint64_t imm_11 = (instructionVal >> 20) & 0x1;
                    uint64_t imm_19_12 = (instructionVal >> 12) & 0xFF;
                    uint64_t immJVal = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
                    id_ex_reg.immediate = signExtend(std::bitset<21>(immJVal).to_string(), 21);
                    id_ex_reg.aluOp = "JAL";
                    id_ex_reg.regWrite = true;
                    id_ex_reg.jump = true;
                    id_ex_reg.writeBackMux = 2;
                }
                break;

            default:
                std::cerr << "Error: Unsupported opcode " << std::bitset<7>(opcode).to_string() 
                          << " at PC " << formatHex(instructionPC) << std::endl;
                id_ex_reg.aluOp = "INVALID";
                id_ex_reg.valid = false;
                break;
        }

        if (pipeliningEnabled) {
            detectAndHandleHazards();
        }

        dataForwardingStall = false;

        if (!dataForwardingEnabled && id_ex_reg.aluOp != "NOP") {
            int rs1 = id_ex_reg.rs1;
            int rs2 = id_ex_reg.rs2;

            bool rs1Needed = true;
            bool rs2Needed = !id_ex_reg.useImm;
            bool storeDataNeedsRs2 = id_ex_reg.memWrite;

            if (ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd != 0) {
                if ((rs1Needed && ex_mem_reg.rd == rs1) ||
                    ((rs2Needed || storeDataNeedsRs2) && ex_mem_reg.rd == rs2)) {
                    dataForwardingStall = true;
                }
            }

            if (mem_wb_reg.valid && mem_wb_reg.regWrite && mem_wb_reg.rd != 0) {
                if ((rs1Needed && mem_wb_reg.rd == rs1 &&
                    !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == rs1)) ||
                    ((rs2Needed || storeDataNeedsRs2) && mem_wb_reg.rd == rs2 &&
                    !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == rs2))) {
                    dataForwardingStall = true;
                }
            }
        }

        if (hazardStall || dataForwardingStall) {
            id_ex_reg.clear();
            id_ex_reg.valid = true;
        } else {
            if_id_reg.valid = false;
        }
    }

    std::string decodeRType(int funct3, int funct7) {
        switch (funct3) {
            case 0b000: return (funct7 == 0b0000000) ? "ADD" : (funct7 == 0b0100000) ? "SUB" : (funct7 == 0b0000001) ? "MUL" : "INVALID";
            case 0b001: return (funct7 == 0b0000000) ? "SLL" : (funct7 == 0b0000001) ? "MULH" : "INVALID";
            case 0b010: return (funct7 == 0b0000000) ? "SLT" : (funct7 == 0b0000001) ? "MULHSU" : "INVALID";
            case 0b011: return (funct7 == 0b0000000) ? "SLTU" : (funct7 == 0b0000001) ? "MULHU" : "INVALID";
            case 0b100: return (funct7 == 0b0000000) ? "XOR" : (funct7 == 0b0000001) ? "DIV" : "INVALID";
            case 0b101: return (funct7 == 0b0000000) ? "SRL" : (funct7 == 0b0100000) ? "SRA" : (funct7 == 0b0000001) ? "DIVU" : "INVALID";
            case 0b110: return (funct7 == 0b0000000) ? "OR" : (funct7 == 0b0000001) ? "REM" : "INVALID";
            case 0b111: return (funct7 == 0b0000000) ? "AND" : (funct7 == 0b0000001) ? "REMU" : "INVALID";
            default: return "INVALID";
        }
    }

    std::string decodeITypeArith(int funct3, int funct7) {
        switch (funct3) {
            case 0b000: return "ADDI";
            case 0b010: return "SLTI";
            case 0b011: return "SLTIU";
            case 0b100: return "XORI";
            case 0b110: return "ORI";
            case 0b111: return "ANDI";
            case 0b001: return "SLLI";
            case 0b101: return (funct7 == 0b0000000) ? "SRLI" : (funct7 == 0b0100000) ? "SRAI" : "INVALID";
            default: return "INVALID";
        }
    }

    std::string decodeBType(int funct3) {
        switch (funct3) {
            case 0b000: return "BEQ";
            case 0b001: return "BNE";
            case 0b100: return "BLT";
            case 0b101: return "BGE";
            case 0b110: return "BLTU";
            case 0b111: return "BGEU";
            default: return "INVALID";
        }
    }

    void execute() {
        ex_debug = ex_mem_reg;
        if (!id_ex_reg.valid) {
            ex_mem_reg.clear();
            return;
        }

        // Forwarding Logic
        uint64_t operand1 = id_ex_reg.readData1;
        uint64_t operand2 = id_ex_reg.useImm ? id_ex_reg.immediate : id_ex_reg.readData2;
        int sourceReg1 = id_ex_reg.rs1;
        int sourceReg2 = id_ex_reg.rs2;

        if (pipeliningEnabled && dataForwardingEnabled) {
            if (ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd != 0) {
                if (ex_mem_reg.rd == sourceReg1) {
                    operand1 = ex_mem_reg.aluResult;
                }
                if (!id_ex_reg.useImm && !id_ex_reg.memWrite && ex_mem_reg.rd == sourceReg2) {
                    operand2 = ex_mem_reg.aluResult;
                }
                if (id_ex_reg.memWrite && ex_mem_reg.rd == sourceReg2) {
                    id_ex_reg.readData2 = ex_mem_reg.aluResult;
                }
            }

            if (mem_wb_reg.valid && mem_wb_reg.regWrite && mem_wb_reg.rd != 0) {
                uint64_t wbData = (mem_wb_reg.writeBackMux == 1) ? mem_wb_reg.readData : mem_wb_reg.aluResult;

                if (mem_wb_reg.rd == sourceReg1 &&
                    !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == sourceReg1)) {
                    operand1 = wbData;
                }
                if (!id_ex_reg.useImm && !id_ex_reg.memWrite && mem_wb_reg.rd == sourceReg2 &&
                    !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == sourceReg2)) {
                    operand2 = wbData;
                }
                if (id_ex_reg.memWrite && mem_wb_reg.rd == sourceReg2 &&
                    !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == sourceReg2)) {
                    id_ex_reg.readData2 = wbData;
                }
            }
        }

        ex_mem_reg.clear();
        ex_mem_reg.valid = true;
        ex_mem_reg.instructionNumber = id_ex_reg.instructionNumber;
        ex_mem_reg.instructionPC = id_ex_reg.instructionPC;

        // ALU Execution
        uint64_t aluResult = 0;
        bool branchConditionMet = false;
        uint64_t branchTarget = 0;
        uint64_t linkAddress = id_ex_reg.nextPC;

        if (id_ex_reg.aluOp == "ADDI") aluResult = operand1 + operand2;
        else if (id_ex_reg.aluOp == "SUB") aluResult = operand1 - operand2;
        else if (id_ex_reg.aluOp == "MUL") aluResult = operand1 * operand2;
        else if (id_ex_reg.aluOp == "XOR" || id_ex_reg.aluOp == "XORI") aluResult = operand1 ^ operand2;
        else if (id_ex_reg.aluOp == "OR" || id_ex_reg.aluOp == "ORI") aluResult = operand1 | operand2;
        else if (id_ex_reg.aluOp == "AND" || id_ex_reg.aluOp == "ANDI") aluResult = operand1 & operand2;
        else if (id_ex_reg.aluOp == "SLL" || id_ex_reg.aluOp == "SLLI") aluResult = operand1 << (operand2 & 0x1F);
        else if (id_ex_reg.aluOp == "SRL" || id_ex_reg.aluOp == "SRLI") aluResult = operand1 >> (operand2 & 0x1F);
        else if (id_ex_reg.aluOp == "SRA" || id_ex_reg.aluOp == "SRAI") aluResult = (int64_t)operand1 >> (operand2 & 0x1F);
        else if (id_ex_reg.aluOp == "SLT" || id_ex_reg.aluOp == "SLTI") aluResult = (operand1 < operand2) ? 1 : 0;
        else if (id_ex_reg.aluOp == "SLTU" || id_ex_reg.aluOp == "SLTIU") aluResult = (operand1 < operand2) ? 1 : 0;
        else if (id_ex_reg.aluOp == "BEQ") branchConditionMet = (operand1 == operand2);
        else if (id_ex_reg.aluOp == "BNE") branchConditionMet = (operand1 != operand2);
        else if (id_ex_reg.aluOp == "BLT") branchConditionMet = ((int64_t)operand1 < (int64_t)operand2);
        else if (id_ex_reg.aluOp == "BGE") branchConditionMet = ((int64_t)operand1 >= (int64_t)operand2);
        else if (id_ex_reg.aluOp == "BLTU") branchConditionMet = (operand1 < operand2);
        else if (id_ex_reg.aluOp == "BGEU") branchConditionMet = (operand1 >= operand2);
        else if (id_ex_reg.aluOp == "JAL") {
            aluResult = linkAddress;
            branchTarget = id_ex_reg.instructionPC + id_ex_reg.immediate;
            branchConditionMet = true;
        }
        else if (id_ex_reg.aluOp == "JALR") {
            aluResult = linkAddress;
            branchTarget = (operand1 + id_ex_reg.immediate) & ~1ULL;
            branchConditionMet = true;
        }
        else if (id_ex_reg.aluOp == "LUI") aluResult = id_ex_reg.immediate;
        else if (id_ex_reg.aluOp == "AUIPC") aluResult = id_ex_reg.instructionPC + id_ex_reg.immediate;
        else if (id_ex_reg.aluOp == "ADD") {
            if (id_ex_reg.memRead || id_ex_reg.memWrite) {
                aluResult = operand1 + operand2;
            } else {
                aluResult = operand1 + operand2;
            }
        }
        else if (id_ex_reg.aluOp != "NOP") {
            std::cerr << "Unknown ALU operation in EX: " << id_ex_reg.aluOp << std::endl;
        }

        if (id_ex_reg.branch || id_ex_reg.jump) {
            if (id_ex_reg.branch) {
                branchTarget = id_ex_reg.instructionPC + id_ex_reg.immediate;
                ex_mem_reg.branchTaken = branchConditionMet;
                
                bool predictedTaken = pipeliningEnabled ? bpu.predictTaken(id_ex_reg.instructionPC) : if_id_reg.predictedTaken;
                uint64_t predictedTarget = pipeliningEnabled ? bpu.getPredictedTarget(id_ex_reg.instructionPC) : if_id_reg.predictedTarget;
                
                bpu.update(id_ex_reg.instructionPC, branchConditionMet, branchTarget);
                
                bool targetMismatch = branchConditionMet && (predictedTarget != branchTarget);
                if (predictedTaken != branchConditionMet || targetMismatch) {
                    branchMispredictFlush = true;
                    pc = branchConditionMet ? branchTarget : id_ex_reg.nextPC;
                    
                    if (printBPUEnabled || printPipelineRegsEnabled) {
                        if (targetMismatch) {
                            std::cout << "BRANCH TARGET MISPREDICT at 0x" << std::hex << id_ex_reg.instructionPC 
                                      << ": Predicted 0x" << predictedTarget << ", Actual 0x" << branchTarget 
                                      << ". Correcting PC.\n";
                        } else {
                            std::cout << "BRANCH DIRECTION MISPREDICT at 0x" << std::hex << id_ex_reg.instructionPC 
                                      << ": Predicted " << (predictedTaken ? "TAKEN" : "NOT TAKEN") 
                                      << ", Actual " << (branchConditionMet ? "TAKEN" : "NOT TAKEN") 
                                      << ". Correcting PC to 0x" << pc << "\n";
                        }
                    }
                }
            } else {
                if (id_ex_reg.aluOp == "JAL") {
                    branchTarget = id_ex_reg.instructionPC + id_ex_reg.immediate;
                } else {
                    branchTarget = (operand1 + id_ex_reg.immediate) & ~1ULL;
                }
                
                ex_mem_reg.branchTaken = true;
                uint64_t predictedTarget = pipeliningEnabled ? bpu.getPredictedTarget(id_ex_reg.instructionPC) : if_id_reg.predictedTarget;
                bpu.update(id_ex_reg.instructionPC, true, branchTarget);
                
                if (predictedTarget != branchTarget) {
                    branchMispredictFlush = true;
                    pc = branchTarget;
                    
                    if (printBPUEnabled || printPipelineRegsEnabled) {
                        std::cout << "JUMP TARGET MISPREDICT at 0x" << std::hex << id_ex_reg.instructionPC 
                                  << ": Predicted 0x" << predictedTarget << ", Actual 0x" << branchTarget 
                                  << ". Correcting PC.\n";
                    }
                }
            }
            
            ex_mem_reg.branchTarget = branchTarget;
        }

        ex_mem_reg.aluResult = aluResult;
        ex_mem_reg.writeData = id_ex_reg.useImm ? 0 : operand2;
        if (id_ex_reg.memWrite) {
            ex_mem_reg.writeData = id_ex_reg.readData2;
        }
        ex_mem_reg.rd = id_ex_reg.rd;
        ex_mem_reg.branchTarget = branchTarget;

        ex_mem_reg.regWrite = id_ex_reg.regWrite;
        ex_mem_reg.memRead = id_ex_reg.memRead;
        ex_mem_reg.memWrite = id_ex_reg.memWrite;
        ex_mem_reg.debugInstruction = id_ex_reg.debugInstruction;
        ex_mem_reg.writeBackMux = id_ex_reg.writeBackMux;
        ex_mem_reg.memSize = id_ex_reg.memSize;

        id_ex_reg.valid = false;
    }

    void memoryAccess() {
        if (!ex_mem_reg.valid) {
            mem_wb_reg.clear();
            return;
        }
        mem_wb_reg.clear();
        mem_wb_reg.valid = true;
        mem_wb_reg.debugInstruction = ex_mem_reg.debugInstruction;
        mem_wb_reg.instructionNumber = ex_mem_reg.instructionNumber;
        mem_wb_reg.instructionPC = ex_mem_reg.instructionPC;

        uint64_t addr = ex_mem_reg.aluResult;
        uint64_t writeData = ex_mem_reg.writeData;

        uint64_t readDataResult = 0;
        if (ex_mem_reg.memRead) {
            readDataResult = readMemory(addr, ex_mem_reg.memSize);
            if (printPipelineRegsEnabled || 
                (traceInstructionNum != -1 && traceInstructionNum == mem_wb_reg.instructionNumber)) {
                std::cout << "      MEM: Read " << ex_mem_reg.memSize << " from 0x" << std::hex << addr 
                          << ", Value=0x" << readDataResult << "\n";
            }
        } else if (ex_mem_reg.memWrite) {
            writeMemory(addr, writeData, ex_mem_reg.memSize);
            if (printPipelineRegsEnabled || 
                (traceInstructionNum != -1 && traceInstructionNum == mem_wb_reg.instructionNumber)) {
                std::cout << "      MEM: Wrote " << ex_mem_reg.memSize << " to 0x" << std::hex << addr 
                          << ", Value=0x" << writeData << "\n";
            }
        }

        mem_wb_reg.aluResult = ex_mem_reg.aluResult;
        mem_wb_reg.readData = readDataResult;
        mem_wb_reg.rd = ex_mem_reg.rd;

        mem_wb_reg.regWrite = ex_mem_reg.regWrite;
        mem_wb_reg.writeBackMux = ex_mem_reg.writeBackMux;
    }

    void writeBack() {
        if (!mem_wb_reg.valid || !mem_wb_reg.regWrite || mem_wb_reg.rd == 0) {
            mem_wb_reg.valid = false;
            return;
        }

        uint64_t writeData;
        switch (mem_wb_reg.writeBackMux) {
            case 0: writeData = mem_wb_reg.aluResult; break;
            case 1: writeData = mem_wb_reg.readData; break;
            case 2: writeData = mem_wb_reg.aluResult; break;
            default:
                std::cerr << "Error: Invalid writeBackMux value in WB: " << mem_wb_reg.writeBackMux << std::endl;
                writeData = 0;
                break;
        }

        std::string rdName = "x" + std::to_string(mem_wb_reg.rd);
        registerFile[rdName] = formatHex(writeData);

        if (printPipelineRegsEnabled || printRegistersEnabled ||
            (traceInstructionNum != -1 && traceInstructionNum == mem_wb_reg.instructionNumber)) {
            std::cout << "      WB: Write 0x" << std::hex << writeData << " to " << rdName 
                      << " (Inst# " << std::dec << mem_wb_reg.instructionNumber << ")\n";
        }
    }

    void detectAndHandleHazards() {
        hazardStall = false;

        if (id_ex_reg.valid && ex_mem_reg.valid && ex_mem_reg.memRead && ex_mem_reg.regWrite && ex_mem_reg.rd != 0) {
            bool rs1Match = id_ex_reg.rs1 == ex_mem_reg.rd;
            bool idUsesRs2 = (id_ex_reg.aluOp == "ADD" && id_ex_reg.memWrite) ||
                             (!id_ex_reg.useImm && !id_ex_reg.jump && id_ex_reg.aluOp != "LUI" && id_ex_reg.aluOp != "AUIPC");
            bool rs2Match = idUsesRs2 && (id_ex_reg.rs2 == ex_mem_reg.rd);

            if (rs1Match || rs2Match) {
                hazardStall = true;
                if (printPipelineRegsEnabled || traceInstructionNum != -1) {
                    std::cout << ">>> Load-Use Hazard Detected! Stalling pipeline. <<<\n";
                    std::cout << "    ID wants r" << id_ex_reg.rs1 << "/r" << id_ex_reg.rs2 
                              << ", EX is LW to r" << ex_mem_reg.rd << "\n";
                }
            }
        }
    }

    void handleFlush() {
        if (branchMispredictFlush) {
            if (printPipelineRegsEnabled) {
                std::cout << ">>> Branch/Jump Misprediction! Flushing pipeline. <<<\n";
            }
            
            if_id_reg.clear();
            id_ex_reg.clear();
            
            branchMispredictFlush = false;
            hazardStall = false;
        }
    }

    void runPipeline() {
        std::cout << "--- Starting Pipelined Simulation ---\n";
        std::cout << "Knobs: Forwarding=" << (dataForwardingEnabled ? "true" : "false") 
                  << ", RegPrint=" << (printRegistersEnabled ? "true" : "false")
                  << ", PipePrint=" << (printPipelineRegsEnabled ? "true" : "false")
                  << ", BPPrint=" << (printBPUEnabled ? "true" : "false")
                  << ", TraceInst#=" << traceInstructionNum << "\n";

        while (mem_wb_reg.debugInstruction != "0xDEADBEEF") {
            clockCycle++;

            std::cout << "\n--- Cycle: " << clockCycle << " ---\n";

            execute();
            EXMEMRegister temp = ex_mem_reg;
            ex_mem_reg = ex_debug;
            writeBack();
            memoryAccess();
            ex_mem_reg = temp;

            handleFlush();

            instructionDecode();
            instructionFetch();

            if (printPipelineRegsEnabled) {
                std::cout << if_id_reg.toString() << "\n";
                std::cout << id_ex_reg.toString() << "\n";
                std::cout << ex_mem_reg.toString() << "\n";
                std::cout << mem_wb_reg.toString() << "\n";
            } else if (traceInstructionNum != -1) {
                if (if_id_reg.valid && if_id_reg.instructionNumber == traceInstructionNum)
                    std::cout << if_id_reg.toString() << "\n";
                if (id_ex_reg.valid && id_ex_reg.instructionNumber == traceInstructionNum)
                    std::cout << id_ex_reg.toString() << "\n";
                if (ex_mem_reg.valid && ex_mem_reg.instructionNumber == traceInstructionNum)
                    std::cout << ex_mem_reg.toString() << "\n";
                if (mem_wb_reg.valid && mem_wb_reg.instructionNumber == traceInstructionNum)
                    std::cout << mem_wb_reg.toString() << "\n";
            }

            if (printRegistersEnabled) {
                printRegisterFileState();
            }
            if (printBPUEnabled) {
                std::cout << bpu.toString();
            }

            bool pipelineEmpty = !if_id_reg.valid && !id_ex_reg.valid && !ex_mem_reg.valid && !mem_wb_reg.valid;
            bool noMoreInstructions = textSegment.count(formatHex(pc)) ? 
                                      textSegment[formatHex(pc)] == NOP_INSTRUCTION : true;

            if (pipelineEmpty && noMoreInstructions) {
                std::cout << "\n--- Pipeline Empty and PC points to NOP. Simulation finished. ---\n";
                break;
            }
        }
    }

    void runSingleCycle() {
        std::cout << "--- Starting Single-Cycle Simulation (Pipelining Disabled) ---\n";
        std::string ir;
        uint64_t pcTemp = 0;
        uint64_t currentPC = pc;
        if_id_reg.clear();
        id_ex_reg.clear();
        ex_mem_reg.clear();
        mem_wb_reg.clear();

        while (true) {
            clockCycle++;
            std::cout << "\n--- Cycle: " << clockCycle << " (PC=" << formatHex(currentPC) << ") ---\n";

            ir = textSegment.count(formatHex(currentPC)) ? textSegment[formatHex(currentPC)] : NOP_INSTRUCTION;
            std::cout << "Fetch: IR = " << ir << "\n";
            if (ir == NOP_INSTRUCTION || ir == "0xDEADBEEF") {
                std::cout << "Termination: " << ir << "\n";
                break;
            }
            pcTemp = currentPC + 4;

            id_ex_reg.clear();
            ex_mem_reg.clear();
            mem_wb_reg.clear();

            if_id_reg.instruction = ir;
            if_id_reg.instructionPC = currentPC;
            if_id_reg.nextPC = pcTemp;
            if_id_reg.valid = true;
            instructionDecode();
            std::cout << "Decode: " << id_ex_reg.toString() << "\n";
            if (!id_ex_reg.valid || id_ex_reg.aluOp == "INVALID") {
                std::cerr << "Decode Error. Halting.\n";
                break;
            }

            pc = pcTemp;
            execute();
            std::cout << "Execute: " << ex_mem_reg.toString() << "\n";
            if (!ex_mem_reg.valid) {
                std::cerr << "Execute Error. Halting.\n";
                break;
            }

            memoryAccess();
            std::cout << "Memory: " << mem_wb_reg.toString() << "\n";
            if (!mem_wb_reg.valid) {
                std::cerr << "Memory Stage Error. Halting.\n";
                break;
            }

            writeBack();
            currentPC = pc;

            if (printRegistersEnabled) {
                printRegisterFileState();
            }

            if (clockCycle > 5000) {
                std::cerr << "Error: Single-cycle execution exceeded 5000 cycles.\n";
                break;
            }
        }
    }


    void printRegisterFileState() {
        std::cout << "Register File State:\n";
        for (int i = 0; i < 32; i++) {
            std::string regName = "x" + std::to_string(i);
            std::cout << "  " << regName << ": " << registerFile[regName] << " ";
            if ((i + 1) % 4 == 0) std::cout << "\n";
        }
        if (32 % 4 != 0) std::cout << "\n";
    }

    void printDataMemoryState() {
        std::cout << "\nData Memory State (Non-zero Bytes):\n";
        if (dataMemory.empty()) {
            std::cout << "  <Empty or All Zeroes>\n";
            return;
        }

        std::map<uint64_t, std::string> sortedMemory;
        for (const auto& entry : dataMemory) {
            try {
                uint64_t addr = parseHex(entry.first);
                sortedMemory[addr] = entry.second;
            } catch (...) {}
        }

        uint64_t currentWordAddr = -1;
        std::string wordLine;
        int bytesInLine = 0;

        for (const auto& entry : sortedMemory) {
            uint64_t addr = entry.first;
            std::string byteVal = entry.second;
            uint64_t wordAddr = addr & ~3ULL;

            if (wordAddr != currentWordAddr) {
                if (bytesInLine > 0) {
                    std::cout << "  0x" << std::hex << std::setw(8) << std::setfill('0') << currentWordAddr 
                              << ": " << wordLine << "\n";
                }
                currentWordAddr = wordAddr;
                wordLine.clear();
                for (uint64_t padAddr = wordAddr; padAddr < addr; padAddr++) {
                    wordLine += "   ";
                    bytesInLine++;
                }
                wordLine += byteVal + " ";
                bytesInLine++;
            } else {
                for (uint64_t padAddr = (addr - bytesInLine); padAddr < addr; padAddr++) {
                    wordLine += "   ";
                }
                wordLine += byteVal + " ";
                bytesInLine++;
            }
            if (bytesInLine == 4) {
                std::cout << "  0x" << std::hex << std::setw(8) << std::setfill('0') << currentWordAddr 
                          << ": " << wordLine << "\n";
                wordLine.clear();
                bytesInLine = 0;
                currentWordAddr = -1;
            }
        }
        if (bytesInLine > 0) {
            std::cout << "  0x" << std::hex << std::setw(8) << std::setfill('0') << currentWordAddr 
                      << ": " << wordLine << "\n";
        }
    }

    void printFinalState() {
        std::cout << "\n--- Simulation Complete ---\n";
        std::cout << "Total Clock Cycles: " << clockCycle << "\n";
        if (pipeliningEnabled && printBPUEnabled) {
            std::cout << bpu.toString();
        }
        printRegisterFileState();
        printDataMemoryState();
    }

public:
    void parseMachineCodeFromFile(const std::string& filePath) {
    textSegment.clear();
    dataMemory.clear();
    uint64_t basePC = -1;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (line.empty() || line[0] == '#') continue;

        size_t commaPos = line.find(',');
        size_t spacePos = line.find(' ');
        if (spacePos == std::string::npos) continue;

        std::string addressStr = line.substr(0, spacePos);
        std::string valueStr = line.substr(spacePos + 1, commaPos != std::string::npos ? commaPos - spacePos - 1 : std::string::npos);

        if (addressStr.substr(0, 2) != "0x" || valueStr.substr(0, 2) != "0x") {
            std::cerr << "Skipping invalid line (hex format error): " << line << "\n";
            continue;
        }

        uint64_t address = std::stoul(addressStr.substr(2), nullptr, 16);
        uint64_t value = std::stoul(valueStr.substr(2), nullptr, 16);

        if (commaPos != std::string::npos) {
            if (value == 0xDEADBEEF) {
                continue;
            }

            if (basePC == -1) {
                basePC = address;
            }
            textSegment[formatHex(address)] = formatHex(value);

            for (int i = 0; i < 4; i++) {
                uint64_t byteVal = (value >> (i * 8)) & 0xFF;
                dataMemory[formatHex(address + i)] = formatHex(byteVal).substr(2);
            }
        } else {
            if (value > 0xFF) {
                std::cerr << "Warning: Data value '" << valueStr << "' larger than a byte (0xFF) found on data line: "
                          << line << ". Storing truncated byte.\n";
                value = value & 0xFF;
            }
            dataMemory[formatHex(address)] = formatHex(value).substr(2);
        }
    }

    if (basePC != -1) {
        pc = basePC;
        std::cout << "Set initial PC to: " << formatHex(pc) << "\n";
    } else {
        std::cout << "No instructions found in text segment. PC remains at 0x0.\n";
    }
    instructionCount = 0;
    std::cout << "Parsing done. Loaded " << textSegment.size() << " instructions.\n";
}

    void run() {
        if (pipeliningEnabled) {
            runPipeline();
        } else {
            runSingleCycle();
        }
        printFinalState();
    }
};

const std::string PipelinedCPU::NOP_INSTRUCTION = "0x00000000";
const std::string PipelinedCPU::ZERO_REG = "x0";

int main(int argc, char* argv[]) {
    std::string filePath = "output.mc";
    if (argc > 1) {
        filePath = argv[1];
        std::cout << "Using machine code file: " << filePath << "\n";
    } else {
        std::cout << "Using default machine code file: " << filePath << "\n";
    }

    PipelinedCPU cpu;
    cpu.parseMachineCodeFromFile(filePath);
    cpu.run();

    return 0;
}