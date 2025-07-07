# OS22 – Operating System Simulator

This project is a C-based simulation of an operating system that models process scheduling, instruction execution, memory management, and synchronization using mutexes.

## 🔧 Features

- Process Control Block (PCB) management
- Instruction parsing from text files
- First-Come-First-Served (FCFS), Round-Robin (RR) and Multi-Level-Feedback-Queue (MLFQ) scheduling
- Mutex-based synchronization
- Modular components (`gui`, `pcb`, `mutex`, etc.)
- Build automation via `makefile`


## 🛠️ How to Build & Run

```bash
make            # Compiles the project
./main          # Runs the simulator (if output is named main)
