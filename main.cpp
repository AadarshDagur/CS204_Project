// CS204 - Project Phase 1
// Group - 8
// Members:
// 1. Adarsh Chaudhary (2023CSB1321)
// 2. Deepanshu (2023CSB1117)
// 3. Lavudya Sai Mani Chaitanya (2023CSB1133)

//---------------------------------------------------------------------------------------

// Including the required header files
#include <iostream>
#include <bits/stdc++.h>
using namespace std;

// Function prototypes
int getNextToken();
string convertToBinary(int num, int bits);
void processInstruction(vector<pair<string, int>> &tokens, ofstream &mcFile, int &address);
void assemble();
string decimalToHex(int decimal);
string decimalToHexDword(unsigned long long decimal);
void parse();

// Global variables
ifstream asmFile;
string currentOperator;
string currentLabel;
string currentDirective;
int currentRegisterValue = -1;
string currentImmediateValue;
int lastChar = ' ';
bool isTextSegment = true;
unordered_map<string, int> labelAddressMap;
map<string, string> memoryMap;
int dataAddress = 0x10000000;


// Enum for token types
enum TokenType {
    TOK_OPERATOR = -1,
    TOK_LABEL = -2,
    TOK_LABEL2 = -8,
    TOK_REGISTER = -3,
    TOK_IMMEDIATE = -4,
    TOK_EOF = -5,
    TOK_DIRECTIVE = -6,
    TOK_NEWLINE = -7,
};


// Set of valid operators
unordered_set<string> operators = {
    "add", "and", "or", "sll", "slt", "sra", "srl", "sub", "xor", "mul", "div", "rem", 
    "addi", "andi", "ori", "xori", "slli", "srli", "srai", "slti", "sltiu", "lb", "ld", 
    "lh", "lw", "jalr", "sb", "sw", "sd", "sh", "beq", "bne", "bge", "blt", "auipc", 
    "lui", "jal"
};


// Map of register names to their values
unordered_map<string, int> registerMap = {
    {"ra", 1}, {"sp", 2}, {"gp", 3}, {"tp", 4}, {"t0", 5}, {"t1", 6}, {"t2", 7}, 
    {"s0", 8}, {"s1", 9}, {"a0", 10}, {"a1", 11}, {"a2", 12}, {"a3", 13}, {"a4", 14}, 
    {"a5", 15}, {"a6", 16}, {"a7", 17}, {"s2", 18}, {"s3", 19}, {"s4", 20}, {"s5", 21}, 
    {"s6", 22}, {"s7", 23}, {"s8", 24}, {"s9", 25}, {"s10", 26}, {"s11", 27}, {"t3", 28}, 
    {"t4", 29}, {"t5", 30}, {"t6", 31}
};


// Execute the assembler
void execute(){
    parse();
    asmFile.clear();
    asmFile.seekg(0);
    lastChar = ' ';
    assemble();
}


// Get the next token from the input file
int getNextToken() {
    while (isspace(lastChar)) {
        if (lastChar == '\n') {
            lastChar = asmFile.get();
            return TOK_NEWLINE;
        }
        lastChar = asmFile.get();
    }

    if (lastChar == ',') {
        while (lastChar == ',') lastChar = asmFile.get();
    }

    if (lastChar == EOF) return TOK_EOF;
    if (lastChar == '#') {
        while (lastChar != '\n' && lastChar != EOF) lastChar = asmFile.get();
        if (lastChar == EOF) return TOK_EOF;
        return TOK_NEWLINE;
    }
    if (lastChar == '.') {
        currentDirective = "";
        lastChar = asmFile.get();
        while (isalnum(lastChar)) {
            currentDirective += lastChar;
            lastChar = asmFile.get();
        }
        return TOK_DIRECTIVE;
    }
    if (lastChar == 'x' && isdigit(asmFile.peek())) {
        lastChar = asmFile.get();
        string num = "";
        while (isdigit(lastChar)) {
            num += lastChar;
            lastChar = asmFile.get();
        }
        currentRegisterValue = stoi(num);
        if (currentRegisterValue >= 0 && currentRegisterValue <= 31) return TOK_REGISTER;
    }
    if (isalpha(lastChar)) {
        currentOperator = "";
        while ((isalnum(lastChar) || lastChar == '_') && lastChar != EOF) {
            currentOperator += lastChar;
            lastChar = asmFile.get();
        }
        if (registerMap.find(currentOperator) != registerMap.end()) {
            currentRegisterValue = registerMap[currentOperator];
            return TOK_REGISTER;
        }
        if (operators.find(currentOperator) == operators.end()) {
            currentLabel = currentOperator;
            if (lastChar == ':') return TOK_LABEL;
            else return TOK_LABEL2;
        }
        return TOK_OPERATOR;
    }
    if (isdigit(lastChar) || lastChar == '-') {
        string num = "";
        if (lastChar == '-') {
            num += lastChar;
            lastChar = asmFile.get();
        }
        if (lastChar == '0' && asmFile.peek() == 'x') {
            num += lastChar;
            lastChar = asmFile.get();
            num += lastChar;
            lastChar = asmFile.get();
        }
        while (isxdigit(lastChar)) {
            num += lastChar;
            lastChar = asmFile.get();
        }
        currentImmediateValue = num;
        return TOK_IMMEDIATE;
    }
    lastChar = asmFile.get();
    return getNextToken();
}


// Register number to binary string mapping
unordered_map<string, string> registerBinaryMap = {
    {"x0", "00000"}, {"x1", "00001"}, {"x2", "00010"}, {"x3", "00011"}, {"x4", "00100"}, 
    {"x5", "00101"}, {"x6", "00110"}, {"x7", "00111"}, {"x8", "01000"}, {"x9", "01001"}, 
    {"x10", "01010"}, {"x11", "01011"}, {"x12", "01100"}, {"x13", "01101"}, {"x14", "01110"}, 
    {"x15", "01111"}, {"x16", "10000"}, {"x17", "10001"}, {"x18", "10010"}, {"x19", "10011"}, 
    {"x20", "10100"}, {"x21", "10101"}, {"x22", "10110"}, {"x23", "10111"}, {"x24", "11000"}, 
    {"x25", "11001"}, {"x26", "11010"}, {"x27", "11011"}, {"x28", "11100"}, {"x29", "11101"}, 
    {"x30", "11110"}, {"x31", "11111"}, {"ra", "00001"}, {"sp", "00010"}, {"gp", "00011"}, 
    {"tp", "00100"}, {"t0", "00101"}, {"t1", "00110"}, {"t2", "00111"}, {"s0", "01000"}, 
    {"s1", "01001"}, {"a0", "01010"}, {"a1", "01011"}, {"a2", "01100"}, {"a3", "01101"}, 
    {"a4", "01110"}, {"a5", "01111"}, {"a6", "10000"}, {"a7", "10001"}, {"s2", "10010"}, 
    {"s3", "10011"}, {"s4", "10100"}, {"s5", "10101"}, {"s6", "10110"}, {"s7", "10111"}, 
    {"s8", "11000"}, {"s9", "11001"}, {"s10", "11010"}, {"s11", "11011"}, {"t3", "11100"}, 
    {"t4", "11101"}, {"t5", "11110"}, {"t6", "11111"}
};


// Instruction information
struct InstructionInfo {
    string opcode, func3, func7;
};


// Instruction to binary opcode, func3, and func7 mapping
unordered_map<string, InstructionInfo> instructionMap = {
    {"add", {"0110011", "000", "0000000"}}, {"sub", {"0110011", "000", "0100000"}}, 
    {"and", {"0110011", "111", "0000000"}}, {"or", {"0110011", "110", "0000000"}}, 
    {"xor", {"0110011", "100", "0000000"}}, {"sll", {"0110011", "001", "0000000"}}, 
    {"srl", {"0110011", "101", "0000000"}}, {"sra", {"0110011", "101", "0100000"}}, 
    {"slt", {"0110011", "010", "0000000"}}, {"mul", {"0110011", "000", "0000001"}}, 
    {"div", {"0110011", "100", "0000001"}}, {"rem", {"0110011", "110", "0000001"}},
    {"addi", {"0010011", "000", ""}}, {"andi", {"0010011", "111", ""}}, 
    {"ori", {"0010011", "110", ""}}, {"lb", {"0000011", "000", ""}}, 
    {"lh", {"0000011", "001", ""}}, {"lw", {"0000011", "010", ""}}, 
    {"ld", {"0000011", "011", ""}}, {"jalr", {"1100111", "000", ""}}, 
    {"sb", {"0100011", "000", ""}}, {"sh", {"0100011", "001", ""}}, 
    {"sw", {"0100011", "010", ""}}, {"sd", {"0100011", "011", ""}}, 
    {"beq", {"1100011", "000", ""}}, {"bne", {"1100011", "001", ""}}, 
    {"blt", {"1100011", "100", ""}}, {"bge", {"1100011", "101", ""}}, 
    {"auipc", {"0010111", "", ""}}, {"lui", {"0110111", "", ""}}, 
    {"jal", {"1101111", "", ""}}
};


// Convert a decimal number to a binary string
string convertToBinary(int num, int bits) {
    string binary = "";
    for (int i = bits - 1; i >= 0; i--) {
        binary += (num & (1 << i)) ? '1' : '0';
    }
    return binary;
}


// Machine code structure
struct MachineCode {
    string formatType;
    string opcode;
    string funct3;
    string funct7;
    string rs1;
    string rs2;
    string rd;
    string imm;
};


// Process an instruction and write its respective machine code to the output file
void processInstruction(vector<pair<string, int>> &tokens, ofstream &mcFile, int &address) {
    if (tokens.empty()) return;

    string op;
    vector<string> operands;

    // Get the operator and operands
    if (tokens[0].second == TOK_OPERATOR) {
        op = tokens[0].first;
        for (int i = 1; i < tokens.size(); i++) operands.push_back(tokens[i].first);
        if (instructionMap.find(op) == instructionMap.end()) {
            cerr << "[Error] !! Unknown instruction: " << op << endl;
            return;
        }
    } 
    
    // Get the label and operator
    else if (tokens[0].second == TOK_LABEL) {
        if (tokens.size() == 1) return;
        op = tokens[1].first;
        for (int i = 2; i < tokens.size(); i++) operands.push_back(tokens[i].first);
        if (instructionMap.find(op) == instructionMap.end()) {
            cerr << "[Error] !! Unknown instruction: " << op << endl;
            return;
        }
    }


    // Set the machine code values based on the instruction format
    InstructionInfo inst = instructionMap[op];
    MachineCode mc;
    mc.formatType = "";
    mc.opcode = inst.opcode;
    mc.funct3 = inst.func3;
    mc.funct7 = inst.func7;
    mc.rd = "";
    mc.rs1 = "";
    mc.rs2 = "";
    mc.imm = "";
    
    // Set the machine code values based on the instruction format

    // R-type instruction
    if (inst.opcode == "0110011") {
        if (operands.size() < 3) {
            cerr << "[Error] !! Insufficient operands for: " << op << endl;
            return;
        }
        mc.formatType = "R";
        mc.rd = registerBinaryMap[operands[0]];
        mc.rs1 = registerBinaryMap[operands[1]];
        mc.rs2 = registerBinaryMap[operands[2]];
    } 
    
    // I-type instruction
    else if (inst.opcode == "0010011" || inst.opcode == "1100111") {
        if (operands.size() < 3) {
            cerr << "[Error] !! Insufficient operands for: " << op << endl;
            return;
        }
        mc.formatType = "I";
        mc.rd = registerBinaryMap[operands[0]];
        mc.rs1 = registerBinaryMap[operands[1]];
        int imm = stoi(operands[2]);
        if (operands[2][0] == '0' && (operands[2][1] == 'x' || operands[2][1] == 'X')) {
            imm = stoi(operands[2], nullptr, 16);
        }
        mc.imm = convertToBinary(imm, 12);
    } 
    
    // S-Type instruction
    else if (inst.opcode == "0100011") {
        if (operands.size() < 3) {
            cerr << "[Error] !! Insufficient operands for: " << op << endl;
            return;
        }
        mc.formatType = "S";
        mc.rs2 = registerBinaryMap[operands[0]];
        mc.rs1 = registerBinaryMap[operands[2]];
        int imm = stoi(operands[1]);
        if (operands[1][0] == '0' && (operands[1][1] == 'x' || operands[1][1] == 'X')) {
            imm = stoi(operands[1], nullptr, 16);
        }
        mc.imm = convertToBinary(imm, 12);
    } 
    
    // U-Type instruction
    else if (inst.opcode == "0110111" || inst.opcode == "0010111") {
        if (operands.size() < 2) {
            cerr << "[Error] !! Insufficient operands for: " << op << endl;
            return;
        }
        mc.formatType = "U";
        mc.rd = registerBinaryMap[operands[0]];
        int imm = stoi(operands[1]);
        if (operands[1][0] == '0' && (operands[1][1] == 'x' || operands[1][1] == 'X')) {
            imm = stoi(operands[1], nullptr, 16);
        }
        mc.imm = convertToBinary(imm, 20);
    } 
    
    // UJ-Type instruction
    else if (inst.opcode == "1101111") {
        if (operands.size() < 2) {
            cerr << "[Error] !! Insufficient operands for: " << op << endl;
            return;
        }
        mc.formatType = "UJ";
        mc.rd = registerBinaryMap[operands[0]];
        mc.imm = convertToBinary(labelAddressMap[operands[1]] - address, 21);
    } 
    
    // I-Type instruction
    else if (inst.opcode == "0000011") {
        if (operands.size() < 3) {
            cerr << "[Error] !! Insufficient operands for: " << op << endl;
            return;
        }
        mc.formatType = "I";
        mc.rd = registerBinaryMap[operands[0]];
        mc.rs1 = registerBinaryMap[operands[2]];
        int imm = stoi(operands[1]);
        if (operands[1][0] == '0' && (operands[1][1] == 'x' || operands[1][1] == 'X')) {
            imm = stoi(operands[1], nullptr, 16);
        }
        mc.imm = convertToBinary(imm, 12);
    } 
    
    // SB-Type instruction
    else if (inst.opcode == "1100011") {
        if (operands.size() < 3) {
            cerr << "[Error] !! Insufficient operands for: " << op << endl;
            return;
        }
        mc.formatType = "SB";
        mc.rs1 = registerBinaryMap[operands[0]];
        mc.rs2 = registerBinaryMap[operands[1]];
        mc.imm = convertToBinary(labelAddressMap[operands[2]] - address, 13);
    } 
    
    // Invalid instruction
    else {
        cerr << "[Error] !! Invalid Instruction format for" << op << endl;
        return;
    }


    // Write the machine code to the output file
    mcFile << "0x" << hex << address << " ";
    if (mc.formatType == "R") {
        mcFile << mc.funct7 << mc.rs2 << mc.rs1 << mc.funct3 << mc.rd << mc.opcode << "  ";
        mcFile << op << " ";
        for (const auto &t : operands) mcFile << t << " ";
        mcFile << " # " << mc.opcode << "-" << mc.funct3 << "-" << mc.funct7 << "-" << mc.rd << "-" << mc.rs1 << "-" << mc.rs2 << "-" << "NULL";
    } else if (mc.formatType == "I") {
        mcFile << mc.imm << mc.rs1 << mc.funct3 << mc.rd << mc.opcode << "  ";
        mcFile << op << " ";
        for (const auto &t : operands) mcFile << t << " ";
        mcFile << " # " << mc.opcode << "-" << mc.funct3 << "-" << "NULL" << "-" << mc.rd << "-" << mc.rs1 << "-" << mc.imm;
    } else if (mc.formatType == "S") {
        mcFile << mc.imm.substr(0, 7) << mc.rs2 << mc.rs1 << mc.funct3 << mc.imm.substr(7, 5) << mc.opcode << " ";
        mcFile << op << " ";
        for (const auto &t : operands) mcFile << t << " ";
        mcFile << " # " << mc.opcode << "-" << mc.funct3 << "-" << mc.funct7 << "-" << mc.rs1 << "-" << mc.rs2 << "-" << mc.imm;
    } else if (mc.formatType == "U") {
        mcFile << mc.imm << mc.rd << mc.opcode << " ";
        mcFile << op << " ";
        for (const auto &t : operands) mcFile << t << " ";
        mcFile << " # " << mc.opcode << "-" << "NULL" << "-" << "NULL" << mc.rd << "-" << mc.imm;
    } else if (mc.formatType == "UJ") {
        mcFile << mc.imm.substr(0, 1) << mc.imm.substr(10, 10) << mc.imm.substr(9, 1) << mc.imm.substr(1, 8) << mc.rd << mc.opcode << " ";
        mcFile << op << " ";
        for (const auto &t : operands) mcFile << t << " ";
        mcFile << " # " << mc.opcode << "-" << "NULL" << "-" << "NULL" << mc.rd << "-" << mc.imm;
    } else if (mc.formatType == "SB") {
        mcFile << mc.imm.substr(0, 1) << mc.imm.substr(2, 6) << mc.rs2 << mc.rs1 << mc.funct3 << mc.imm.substr(8, 4) << mc.imm.substr(1, 1) << mc.opcode << " ";
        mcFile << op << " ";
        for (const auto &t : operands) mcFile << t << " ";
        mcFile << " # " << mc.opcode << "-" << mc.funct3 << "-" << mc.funct7 << "-" << mc.rs1 << "-" << mc.rs2 << "-" << mc.imm;
    }
    mcFile << endl;

    address += 4; // Increment the address
}


// Assemble the input file
void assemble() {
    ofstream mcFile("output.mc");
    if (!mcFile) {
        cerr << "[Error] !! Unable to create output.mc" << endl;
        return;
    }
    mcFile << "Memory Contents:" << endl;
    for (const auto &it : memoryMap) {
        mcFile << it.first << ": " << it.second << endl;
    }
    mcFile<<"--------------------------------"<<endl;
    mcFile << "Text Segment:" << endl;
    int address = 0;
    vector<pair<string, int>> tokens;

    while (true) {
        int tok = getNextToken();
        if (tok == TOK_DIRECTIVE) {
            if (currentDirective == "text") isTextSegment = true;
            else if (currentDirective == "data") isTextSegment = false;
        }
        if (isTextSegment) {
            if (tok == TOK_EOF) {
                if (!tokens.empty()) processInstruction(tokens, mcFile, address);
                break;
            }
            if (tok == TOK_NEWLINE) {
                if (!tokens.empty()) {
                    processInstruction(tokens, mcFile, address);
                    tokens.clear();
                }
            } else {
                if (tok == TOK_OPERATOR) tokens.push_back({currentOperator, TOK_OPERATOR});
                else if (tok == TOK_REGISTER) tokens.push_back({"x" + to_string(currentRegisterValue), TOK_REGISTER});
                else if (tok == TOK_IMMEDIATE) tokens.push_back({currentImmediateValue, TOK_IMMEDIATE});
                else if (tok == TOK_LABEL) tokens.push_back({currentLabel, TOK_LABEL});
                else if (tok == TOK_LABEL2) tokens.push_back({currentLabel, TOK_LABEL2});
            }
        }
        if (tok == TOK_EOF) break;
    }
    mcFile << "--------------------------------" << endl;
    mcFile << "0x" << hex << address << " " << "Terminate";
    mcFile.close();
}

string decimalToHex(int decimal) {
    char hexStr[11];
    sprintf(hexStr, "0x%08X", decimal);
    return string(hexStr);
}

string decimalToHexDword(unsigned long long decimal) {
    char hexStr[19];
    sprintf(hexStr, "0x%016llX", decimal);
    return string(hexStr);
}

void parse() {
    asmFile.clear();
    asmFile.seekg(0);
    int address = 0;
    vector<pair<string, int>> tokens;

    while (true) {
        int tok = getNextToken();
        if (tok == TOK_DIRECTIVE) {
            if (currentDirective == "data") isTextSegment = false;
            else if (currentDirective == "text") isTextSegment = true;
        }
        if (isTextSegment) {
            if (tok == TOK_EOF) {
                if (!tokens.empty()) {
                    if (tokens[0].second == TOK_LABEL) {
                        labelAddressMap[tokens[0].first] = address;
                        tokens.erase(tokens.begin());
                    }
                    if (!tokens.empty()) address += 4;
                }
                break;
            }
            if (tok == TOK_NEWLINE) {
                if (!tokens.empty()) {
                    if (tokens[0].second == TOK_LABEL) {
                        labelAddressMap[tokens[0].first] = address;
                        tokens.erase(tokens.begin());
                    }
                    if (!tokens.empty()) address += 4;
                }
                tokens.clear();
            } else {
                if (tok == TOK_OPERATOR) tokens.push_back({currentOperator, TOK_OPERATOR});
                else if (tok == TOK_REGISTER) tokens.push_back({"x" + to_string(currentRegisterValue), TOK_REGISTER});
                else if (tok == TOK_IMMEDIATE) tokens.push_back({currentImmediateValue, TOK_IMMEDIATE});
                else if (tok == TOK_LABEL) tokens.push_back({currentLabel, TOK_LABEL});
            }
        } else {
            if (tok == TOK_LABEL) {
                tok = getNextToken();
                if (tok == TOK_DIRECTIVE) {
                    tok = getNextToken();
                    while (tok == TOK_IMMEDIATE) {
                        if (currentDirective == "word") {
                            int imm = stoi(currentImmediateValue, nullptr, 0);
                            currentImmediateValue = decimalToHex(imm);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(8, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(6, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(4, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(2, 2);
                        } else if (currentDirective == "byte") {
                            int imm = stoi(currentImmediateValue, nullptr, 0);
                            currentImmediateValue = decimalToHex(imm);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(8, 2);
                        } else if (currentDirective == "half") {
                            int imm = stoi(currentImmediateValue, nullptr, 0);
                            currentImmediateValue = decimalToHex(imm);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(8, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(6, 2);
                        } else if (currentDirective == "dword") {
                            int imm = stoi(currentImmediateValue, nullptr, 0);
                            currentImmediateValue = decimalToHexDword(imm);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(16, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(14, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(12, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(10, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(8, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(6, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(4, 2);
                            memoryMap[decimalToHex(dataAddress++)] = currentImmediateValue.substr(2, 2);
                        }
                        tok = getNextToken();
                    }
                }
            }
        }
    }
}

int main() {
    asmFile.open("input.asm");
    if (!asmFile) {
        cerr << "[Error] !! Unable to open input.asm" << endl;
        return 0;
    }

    cout<<"Assembling the input file..."<<endl;
    execute();
    cout<<"Assembling completed successfully."<<endl;
    cout<<"--------------------------------"<<endl;
    cout<< "Output machine code is stored in output.mc file"<<endl;
    return 0;
}
