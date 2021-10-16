gcc-pytho-2
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
