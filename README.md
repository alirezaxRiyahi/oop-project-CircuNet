# CircuNet

A C++ project for **circuit analysis** and **networked simulation/visualization** using **SDL2**.  
This project is built with **CMake** and uses external libraries like **SDL2**, **SDL2_ttf**, and **cereal**, with **WinSock (ws2_32)** for networking support.

---


## 🚀 Features
- ⚡ Circuit simulation basics
- 🎨 Graphical interface with SDL2
- 📝 Text rendering with SDL2_ttf
- 💾 Data serialization with cereal
- 🌐 Networking support with WinSock (client/server communication)
- 📂 Resource management (assets, data)

---

## 📦 Requirements
- CMake >= 3.20
- MinGW (with g++)
- SDL2
- SDL2_ttf
- cereal (included as header-only library)
- Windows (with WinSock2)

---

## ⚙️ Build Instructions
```bash
git clone https://github.com/alirezaxRiyahi/oop-project-CircuNet.git
cd CircuNet
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
