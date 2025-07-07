# OS22 – Operating System Simulator

This project is a C-based simulation of an operating system that models process scheduling, instruction execution, memory management, and synchronization using mutexes.

## 🔧 Features

- Process Control Block (PCB) management
- Instruction parsing from text files
- First-Come-First-Served (FCFS) scheduling
- Mutex-based synchronization
- Modular components (`gui`, `pcb`, `mutex`, etc.)
- Build automation via `makefile`

## 📁 Project Structure

| File             | Description |
|------------------|-------------|
| `main.c`         | Entry point and main simulation loop |
| `pcb.c/.h`       | Defines and manages PCBs |
| `instruction.c/.h` | Instruction parsing and handling |
| `mutex.c/.h`     | Synchronization primitives |
| `gui.c/.h`       | Optional GUI components |
| `makefile`       | For compiling all components |
| `Program_1.txt`  | Sample instruction file |

## 🛠️ How to Build & Run

```bash
make            # Compiles the project
./main          # Runs the simulator (if output is named main)
