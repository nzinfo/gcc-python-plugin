cmake_minimum_required(VERSION 3.16)
project(gcc_python_plugin)

# Global setting: build everything position independent
set( CMAKE_POSITION_INDEPENDENT_CODE ON )

# Global setting: Use C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR OFF)

# only gcc compiler can build the project
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "GCC Plugin can ONLY been built with GNU compilers.")
endif()

# mingw can not get varible from execute_process
if(CMAKE_COMPILER_IS_MINGW)
    set(SHELL_CMD sh)
    set(SHELL_CMD_ARGS "-c")
endif()

#[[

# find_package (Python3 COMPONENTS Interpreter Development) # 只需要支持 Python 3 即可， Python 2 EOL
find_package(PythonInterp 3 REQUIRED)
# FIND_PACKAGE(PythonLibs REQUIRED)

# helper macro , TODO: move it to helper file.
macro(convert_real_path file_list)
  foreach(src_file ${${file_list}})
    get_filename_component(src_file_path "${src_file}" REALPATH)
    list(REMOVE_ITEM ${file_list} "${src_file}")
    list(APPEND ${file_list} "${src_file_path}")
    # message(STATUS "PYTHON_EXECUTABLE = ${src_file_path}")
  endforeach()
endmacro()


if (PYTHONINTERP_FOUND)
    get_filename_component(PYTHON_INTERP ${PYTHON_EXECUTABLE} NAME_WE)  # mingw / windows only, remove file extension(.exe)
    set(PYCONFIG ${PYTHON_INTERP}-config)
    # In mingw64 , FIND_PACKAGE(PythonLibs REQUIRED) can not detected correct python's header file, use python-config 
    # execute_process(COMMAND ${SHELL_CMD} "${SHELL_CMD_ARGS}" "${PYCONFIG} --includes" OUTPUT_VARIABLE PYTHON_INCLUDES)
    execute_process(COMMAND ${SHELL_CMD} "${SHELL_CMD_ARGS}" "${PYCONFIG} --ldflags"  OUTPUT_VARIABLE PYTHON_LIBRARIES OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${SHELL_CMD} "${SHELL_CMD_ARGS}" "${PYCONFIG} --includes"  OUTPUT_VARIABLE PYTHON_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    
    string(REGEX REPLACE "^[-I]" "" PYTHON_INCLUDE_DIR "${PYTHON_INCLUDE_DIR}")
    string(REGEX REPLACE "[ ]-I" " " PYTHON_INCLUDE_DIR "${PYTHON_INCLUDE_DIR}")
    separate_arguments(PYTHON_INCLUDE_DIR)
    convert_real_path(PYTHON_INCLUDE_DIR)
    # 虽然进行了如上处理，但是 pybind11 自己有检查 Python 的代码，实际用不到
else()
    message(FATAL_ERROR "Python not found")
endif()

message(STATUS "PYTHON_EXECUTABLE = ${PYTHON_EXECUTABLE}")

# 实际用不上
# message(STATUS "PYTHON_INCLUDE_DIR = ${PYTHON_INCLUDE_DIR}")
# message(STATUS "PYTHON_LIBRARIES = ${PYTHON_LIBRARIES}")
]]

# check gcc plugin dir
execute_process(COMMAND ${SHELL_CMD} "${SHELL_CMD_ARGS}" "${CMAKE_C_COMPILER} --print-file-name=plugin"  OUTPUT_VARIABLE GCC_PLUGINS_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
# get_filename_component(GCC_PLUGINS_DIR ${GCC_PLUGINS_DIR} REALPATH)
message(STATUS "GCC_PLUGINS_DIR = ${GCC_PLUGINS_DIR}")

add_subdirectory(pybind11) # import marco in pybind11
add_subdirectory(src)