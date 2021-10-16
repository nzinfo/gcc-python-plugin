cmake_minimum_required(VERSION 3.16)
project(gcc_python_plugin)

# Global setting: build everything position independent
set( CMAKE_POSITION_INDEPENDENT_CODE ON )

# Global setting: Use C++14
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package (Python3 COMPONENTS Interpreter Development) # 只需要支持 Python 3 即可， Python 2 EOL
# FIND_PACKAGE(PythonLibs 3 REQUIRED)

if (NOT PYTHONINTERP_FOUND)
    message(FATAL_ERROR "Python not found")
    return()
endif()

message(STATUS "PYTHON_LIBRARIES = ${PYTHON_LIBRARIES}")
message(STATUS "PYTHON_EXECUTABLE = ${PYTHON_EXECUTABLE}")
message(STATUS "PYTHON_INCLUDE_DIRS = ${PYTHON_INCLUDE_DIRS}")


add_subdirectory(src)