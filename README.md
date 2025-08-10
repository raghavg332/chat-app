# C++ Multi-Threaded Chat Application

A high-performance, terminal-based chat system built in modern C++ with **POSIX sockets** and **ncurses**.  
Supports **multi-client messaging**, **group chat**, and a simple command interface — engineered for concurrency, stability, and real-time feedback.

---
## System Architecture
<img width="595" height="667" alt="system architecture" src="https://github.com/user-attachments/assets/461be6ff-fcfc-45d0-8a07-5596418ad5ff" />

---
## Message Flow
<img width="1490" height="800" alt="message flow" src="https://github.com/user-attachments/assets/a027c1d5-89e4-4837-ae88-fa85cd20cbf6" />

---
## Features

### 🖥 Server
- **Multi-threaded TCP server** with one thread per client.
- **Group chat support** (`/join <group>`, `/leave`, `/groups`).
- **Global chat mode** for broadcasting to all connected users.
- **User list** command (`/users`) to view participants in real-time.
- **Thread-safe state management** with `std::mutex` to prevent race conditions.
- **Graceful client disconnect handling**.

### 💬 Client
- **Ncurses-based UI** with split chat & input windows.
- **Color-coded messages**:
  - Your messages in **cyan**.
  - Incoming user names/prefixes in **red**.
  - System/global messages in **green**.
- **Responsive input box** with prompt.
- **Ctrl+C safe exit** — cleans up sockets and UI.

---

## 🛠 Tech Stack
- **Language:** C++17
- **Libraries:** `pthread`, `ncurses`
- **Protocols:** TCP (IPv4)
- **Platform:** POSIX-compliant systems (Linux, macOS)
- **Build:** `g++` with `-pthread -lncurses`

---

## 📂 Project Structure
```
├── server
│   └── main.cpp # Multithreaded chat server
│   └── main
├── client
│   └── main.cpp # Ncurses chat client
│   └── main
├── testing/
│   └── load_tester.py  # Async load tester for performance benchmarking
└── README.md
```

---

## 🚀 Getting Started

### 1. Compile
```bash
cd server
g++ main.cpp -o main -pthread
cd client
g++ main.cpp -o main -pthread -lncurses
```

### 2. Run the server
```bash
cd server
./main
```
By default, the server listens on **port 8081**.

Note: An instance of the server is already hosted at 13.200.235.191:8081 that the client can easily connect to.

### 3. Run clients
```bash
cd client
./main
```
Enter a username when prompted, then chat using:
- `/users` — List connected users
- `/join <group>` — Create/join a group
- `/groups` — List available groups
- `/leave` — Leave current group

---

## 📊 Performance Benchmark

All benchmarks were run on:
- **AWS EC2 Free Tier** (t2.micro — 1 vCPU, 1 GB RAM)
- **Amazon Linux (latest)**
- Server compiled with `g++` on Amazon Linux
- Clients simulated using the provided async load tester (`load_tester.py`), measuring **Time-to-First-Byte (TTFB)** for `/users` command responses.
- Clients and server ran on different instances.

### **Baseline Run** — 100 concurrent clients
| Metric                   | Value   |
|--------------------------|---------|
| Connections OK           | 200     |
| Connections Failed       | 0       |
| Requests Sent            | 1,500   |
| Responses Observed       | 1,500   |
| **p50 TTFB**              | 53.27 ms |
| **p90 TTFB**              | 76.43 ms |
| **p95 TTFB**              | 83.28 ms |
| **p99 TTFB**              | 94.37 ms |
| Bytes Sent               | 10,180  |
| Bytes Received           | 2,253,882 |
| Duration                 | 30.02 s |

---

### **Scalability Sweep** — Increasing concurrent clients until degradation
| Clients | p50 TTFB (ms) | p90 TTFB (ms) | Connections OK | Connections Failed |
|---------|--------------|--------------|----------------|--------------------|
| 50      | 34.44        | 99.99        | 100            | 0                  |
| 100     | 55.65        | 84.35        | 200            | 0                  |
| 150     | 72.41        | 198.23       | 300            | 0                  |
| 200     | 119.21       | 207.47       | 400            | 0                  |
| 250     | 84.46        | 173.66       | 500            | 0                  |
| 300     | 143.09       | 246.62       | 600            | 0                  |
| 350     | 150.64       | 450.58       | 700            | 0                  |
| **400** | **165.09**   | **401.11**   | **800**        | **0**              |
| 450     | 233.21       | 832.76       | 900            | 0                  |

**Peak stable concurrency (per criteria):** **400 concurrent clients** before median TTFB exceeded acceptable latency threshold.

---

## 📈 Key Takeaways
- Handles **100 concurrent clients** with **p50 latency ~53 ms** on a remote **t2.micro** instance.
- Scales to **400 concurrent clients** before significant median latency growth (>160 ms).
- Zero connection failures observed up to 450 concurrent clients in this environment.
- Memory & CPU remained stable during testing; main bottleneck is **single-thread-per-client model** on limited CPU.
