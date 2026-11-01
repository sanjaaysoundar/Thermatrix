# Dependencies and Licenses

THERMATRIX is built strictly using free, open-source, and permissive-licensed libraries with zero commercial restrictions.

## 1. Third-Party Libraries

| Library | Version / Git Tag | License | Purpose | Source / URL |
| :--- | :--- | :--- | :--- | :--- |
| **Dear ImGui** | v1.91.8 (docking / master) | MIT License | Immediate mode graphical user interface and rendering pipeline | [github.com/ocornut/imgui](https://github.com/ocornut/imgui) |
| **ImPlot** | v0.17+ | MIT License | High-performance 2D plotting extension for Dear ImGui | [github.com/epezent/implot](https://github.com/epezent/implot) |
| **GLFW** | 3.4.0 | zlib / libpng License | Multi-platform window creation, OpenGL context management, and input polling | [glfw.org](https://www.glfw.org) |
| **OpenGL** | 3.0+ / OpenGL32 | System Runtime (Windows) | Hardware-accelerated 2D/3D graphics rendering | Microsoft Windows SDK / System DLL |
| **OpenMP** | 4.5+ (libgomp / vcomp) | GNU LGPL / MSVC CRT EULA | Multi-core CPU parallelization of FEM assembly and iterative backpropagation | Compiler Built-in |

## 2. Standard Library and Toolchain Requirements

- **C++ Standard**: ISO C++17 (`-std=c++17` / `/std:c++17`)
- **Build System**: CMake 3.16 or later
- **Compilers Supported**:
  - GCC / MinGW-w64 10.0+ (Tested with GCC 16.2.0 on MSYS2)
  - Clang / LLVM 12.0+
  - Microsoft Visual C++ (MSVC) 2019 / 2022 (v142 / v143)

## 3. License Texts

### Dear ImGui (MIT License)
```
The MIT License (MIT)
Copyright (c) 2014-2026 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### ImPlot (MIT License)
```
The MIT License (MIT)
Copyright (c) 2020-2026 Evan Pezent

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### GLFW (zlib / libpng License)
```
Copyright (c) 2002-2006 Marcus Geelnard
Copyright (c) 2006-2024 Camilla Löwy <elmindreda@glfw.org>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```
