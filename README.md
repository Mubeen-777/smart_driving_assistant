<p align="center">
  <img src="assets/logo.png" width="200" alt="Smart Drive Logo">
</p>

<h1 align="center">Smart Driving Assistant</h1>

<p align="center">
  <strong>A high-performance, minimalist, and AI-powered backend for modern driving telemetry and safety management.</strong>
</p>

<br>

<div align="center">
  <img src="https://img.shields.io/badge/C++-14-blue?style=for-the-badge&logo=c%2B%2B" alt="C++">
  <img src="https://img.shields.io/badge/Status-Ultra_Lean-success?style=for-the-badge" alt="Status">
  <img src="https://img.shields.io/badge/Architecture-Modular-orange?style=for-the-badge" alt="Architecture">
</div>

<br>

## 🚀 Overview

Smart Driving Assistant is a robust C++ backend engineered for extreme performance and scalability. This repository houses the core logic for trip management, telemetry processing, and safety analytics, stripped to its purest form for maximum speed and maintainability.

### 🌟 Key Features

- 🏎️ **Trip Telemetry**: Real-time tracking of speed, distance, and fuel efficiency.
- 🛡️ **Safety Analytics**: Automated detection of harsh braking, rapid acceleration, and sharp turns.
- 📊 **Dynamic Indexing**: Custom B+Tree and Segment Tree implementations for high-speed data retrieval.
- 📉 **Optimized Caching**: Triple-tier LRU cache mechanism for minimized database latency.
- 🌐 **Web Integration**: Seamless API layer for modern dashboard visualization.

## 🛠️ Technology Stack

- **Core Logic**: Modern C++14
- **Computer Vision**: OpenCV, Dlib
- **Data Persistence**: Custom High-Performance File-Based Database
- **Indexing**: B-Tree, B+Tree, Segment Trees
- **Networking**: Multi-threaded Socket Server

## 📂 Project Structure

- `source/`: Architecture-first source code.
  - `core/`: Business logic and manager classes.
  - `data_structures/`: Custom performance-specialized data structures.
  - `server/`: High-concurrency request handling layer.
  - `modules/`: Feature-specific modules (Computer Vision, UDP).
- `frontend/`: Modern responsive dashboard implementation.
- `include/`: Unified header definitions and system configurations.

## ⚙️ Building and Deployment

The project is designed to be lean. Building the core server requires `g++` and common library dependencies (OpenCV, SQLite3, JSONCPP).

```bash
# Example build command for the CLI server
g++ -std=c++14 -o main main.cpp \
    -lopencv_core -lpthread -lsqlite3 -ljsoncpp -ldlib -lcrypto
```

---

<p align="center">
  Developed with precision for the next generation of safe driving.
</p>