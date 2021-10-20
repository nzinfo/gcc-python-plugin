cmake_minimum_required(VERSION 3.16)
project(gcc_python_plugin)

set(PLUGIN_NAME pygcc)

set( GCC_PYTHON_PLUGIN_SRCS
    gcc-python.cpp
    )

if(MSYS)
    set(LIBCC1 cc1plus.exe)
else()
    set(LIBCC1 cp1plugin)
endif()

# get_filename_component(PYTHON_INCLUDE_DIR ${PYTHON_INCLUDE_DIR} REALPATH)
pybind11_add_module(${PLUGIN_NAME} SHARED  ${GCC_PYTHON_PLUGIN_SRCS} WITHOUT_SOABI)

set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
target_include_directories(${PLUGIN_NAME} PUBLIC 
    ${GCC_PLUGINS_DIR}/include ${CMAKE_SOURCE_DIR}/pybind11/include )
set_target_properties(${PLUGIN_NAME} PROPERTIES LINK_FLAGS " -L${GCC_PLUGINS_DIR}")
target_link_libraries(${PLUGIN_NAME} PUBLIC pybind11::headers pybind11::embed)
target_link_libraries(${PLUGIN_NAME} PRIVATE ${LIBCC1})
# pybind11 require frtti, but gcc might require fno-rtti....
# target_compile_options(${PLUGIN_NAME} PRIVATE -Wall -Wextra -pedantic -fno-rtti -g -O3) 
target_compile_options(${PLUGIN_NAME} PRIVATE -Wall -Wextra -pedantic -g -O3) 

add_test(NAME pygcc_run_demo
    COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> demo.c -fplugin-arg-pygcc-command=abc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    # COMMENT "Running GCC plugin check..."
)

