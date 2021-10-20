gcc-python-2
==========

This is a plugin for GCC, which links against libpython, and (I hope) allows
you to invoke arbitrary Python scripts from inside the compiler.  The aim is to
allow you to write GCC plugins in Python.

The plugin is Free Software, licensed under the GPLv3 (or later).

从 github.com/davidmalcolm/gcc-python-plugin 衍生而来。

1. 使用 PyBind11 替代了原有的 Python C-API 调用；
2. 使用 CMake 管理工程，支持 mingw64 上的 gcc-9 (mingw64 上的 gcc-10 因为默认关闭了 plugin 的编译开关，无法支持)
    - 去掉了原项目中的代码生成机制
    - 重新整理源代码到 src 目录
3. 支持 Python3.8 及其后续版本


## Note for build on Mingw64

1. 不能直接使用 默认的 cmake ，需要是 mingw-w64-x86_64-cmake
2. 无法直接使用默认的 make ，需要换用 ninja , mingw-w64-x86_64-ninja

```
Run cmake from the mingw64 shell, not the msys2 shell (usually C:/msys64/mingw64.exe)
Make sure you installed mingw-w64-x86_64-cmake, not just cmake or mingw-w64-cmake.
Make sure you installed mingw-w64-x86_64-ninja, not just ninja or mingw-w64-ninja.
```

## Note for build in Clion

1. SET cmake environment variable `MSYSTEM` = 1
2. SET PYTHON_EXECUTABLE to mingw's python.exe


## Note for CTest 

1. 如果 stderr 有输出，则 CTest 会认为测试失败。
2. 设置环境变量 PYGCC_DEBUG = 1 （或其他任何值）， 会激活对 stderr 的调试输出