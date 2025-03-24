# RISC-V Assembler and Simulator

## Overview
This project implements a 32-bit RISC-V assembler and a pipeline simulator in C++. The assembler (Phase 1) reads RISC-V assembly code and translates it into machine code, while the simulator (Phase 2) simulates the generated machine code.

## Features
### Phase 1: Assembler
- Converts RISC-V assembly code (`input.asm`) into machine code (`output.mc`)
- Supports `.text` and `.data` assembler directives
- Implements instructions in R, I, S, SB, U, UJ formats
- Stores memory contents and labels for branching

### Phase 2: Simulator
- Simulates instruction execution with a 5-stage pipeline (Fetch, Decode, Execute, Memory, Write-back)
- Maintains a register file and memory state
- Supports 31 RISC-V instructions
- Outputs execution details and register updates
- Generates a memory dump file (`data_memory.txt`) after execution

## Project Structure
```
📂 Project Directory
├── phase1.cpp        # RISC-V assembler implementation
├── phase2.cpp        # RISC-V simulator implementation
├── input.asm         # Sample RISC-V assembly input file
├── output.mc         # Machine code generated from the assembler
├── data_memory.txt   # Memory dump after execution
└── README.md         # Project documentation
```

## Compilation and Execution
### 1. Compile the Assembler and Simulator
```sh
g++ -o assembler phase1.cpp
g++ -o simulator phase2.cpp
```

### 2. Run the Assembler
```sh
./assembler
```
This generates `output.mc` containing machine code.

### 3. Run the Simulator
```sh
./simulator
```
This simulates execution and generates `data_memory.txt`.

## Usage
1. Write RISC-V assembly code in `input.asm`
2. Run the assembler to generate `output.mc`
3. Run the simulator to execute the machine code
4. View register updates and memory contents in `data_memory.txt`

## Contributors
- **Adarsh Chaudhary** (2023CSB1321)
- **Deepanshu** (2023CSB1117)
- **Lavudya Sai Mani Chaitanya** (2023CSB1133)

## License
This project is for educational purposes and follows an open-source license.

