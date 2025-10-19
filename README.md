# Data Oriented Design - SDL Integration

This repository contains a minimal SDL2 setup for the Data Oriented Design course on Windows.  
The goal is to show a working SDL2 integration using CMake and vcpkg.

## Project description
The program initializes SDL2, opens a simple 800x600 window, and closes it when the ESC key or the window close button is pressed.

## Steps followed

1. Installed vcpkg in `C:\vcpkg`:
   ```powershell
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   .\bootstrap-vcpkg.bat
   ```

2. Installed SDL2 through vcpkg:
   ```powershell
   C:\vcpkg\vcpkg.exe install sdl2:x64-windows
   ```

3. Created the project structure:
   ```
   game-dod-sdl/
   ├─ src/
   │  └─ main.cpp
   ├─ CMakeLists.txt
   ├─ .gitignore
   └─ README.md
   ```

4. Configured and built the project using CMake:
   ```powershell
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

5. Ran the executable:
   ```powershell
   .\build\Release\game.exe
   ```

When executed, a black SDL2 window appears and closes properly when ESC or X is pressed.
