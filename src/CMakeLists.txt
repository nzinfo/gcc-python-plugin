cmake_minimum_required(VERSION 3.16)
project(gcc_python_plugin)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
#set(GCC_SRC_DIR "/home/sbcd90/Documents/programs/gcc-10/gcc-10.1.0")
#set(GCC_BUILD_DIR "/home/sbcd90/Documents/programs/gcc-10/gcc-build")
#set(GCC_PLUGIN_DIR "/home/sbcd90/Documents/programs/gcc-10/gcc-install/lib/gcc/x86_64-pc-linux-gnu/10.1.0/plugin")
#set(GCC_LIB_CPP "/home/sbcd90/Documents/programs/gcc-10/gcc-10.1.0/libcpp")

set( GCC_PYTHON_PLUGIN_SRCS
    gcc-python.cpp
    )

# get_filename_component(PYTHON_INCLUDE_DIR ${PYTHON_INCLUDE_DIR} REALPATH)
pybind11_add_module(gcc_python_plugin SHARED ${GCC_PYTHON_PLUGIN_SRCS})

set_target_properties(gcc_python_plugin PROPERTIES PREFIX "")
target_include_directories(gcc_python_plugin PUBLIC 
    ${GCC_PLUGINS_DIR}/include ${CMAKE_SOURCE_DIR}/pybind11/include )
set_target_properties(gcc_python_plugin PROPERTIES LINK_FLAGS " -L${GCC_PLUGINS_DIR}")
target_link_libraries(gcc_python_plugin PRIVATE cc1plus.exe)
# pybind11 require frtti, but gcc might require fno-rtti....
# target_compile_options(gcc_python_plugin PRIVATE -Wall -Wextra -pedantic -fno-rtti -g -O3) 
target_compile_options(gcc_python_plugin PRIVATE -Wall -Wextra -pedantic -g -O3) 

add_custom_command(
    COMMAND g++ -fplugin=$<TARGET_FILE:gcc_python_plugin> demo.c
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT  pygcc_run_demo.out
    # COMMENT "Running GCC plugin check..."
)
add_custom_target(
    pygcc ALL
    DEPENDS pygcc_run_demo.out
) 

message(STATUS "Targ =")
