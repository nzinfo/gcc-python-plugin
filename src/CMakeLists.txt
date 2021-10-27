cmake_minimum_required(VERSION 3.16)
project(gcc_python_plugin)

set(PLUGIN_NAME pygcc)

set( GCC_PYTHON_PLUGIN_SRCS
        gcc-python.cpp
        python-ggc-wrapper.cpp
        gcc-python-closure.cpp
        gcc-python-callback.cpp
        gcc-python-passmanager.cpp
    )

if(WIN32)
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

add_test(NAME pygcc_plugin_arg_1
    COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> ${CMAKE_SOURCE_DIR}/tests/stubs/dummy.c "-fplugin-arg-pygcc-command=print\('abc'\)"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    # COMMENT "Running GCC plugin check..."
)
set_tests_properties(pygcc_plugin_arg_1 PROPERTIES
        PASS_REGULAR_EXPRESSION "abc"
        )

# 检查 sys.plugin_full_name 是否设置正确
add_test(NAME pygcc_plugin_arg_2
        COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> ${CMAKE_SOURCE_DIR}/tests/stubs/dummy.c "-fplugin-arg-pygcc-command=import sys; print\(sys.plugin_full_name\)"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        # COMMENT "Running GCC plugin check..."
        )
set_tests_properties(pygcc_plugin_arg_2 PROPERTIES
        PASS_REGULAR_EXPRESSION $<TARGET_FILE:${PLUGIN_NAME}>
        )

# 检查 regist callback 是否存在
add_test(NAME pygcc_plugin_arg_3
        COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> ${CMAKE_SOURCE_DIR}/tests/stubs/dummy.c "-fplugin-arg-pygcc-command=import gcc; gcc.register_callback\(gcc.PLUGIN_FINISH, lambda: print\('hello'\)\)"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        # COMMENT "Running GCC plugin check..."
        )
set_tests_properties(pygcc_plugin_arg_3 PROPERTIES
        PASS_REGULAR_EXPRESSION "hello"
        )

add_test(NAME pygcc_plugin_arg_4
        COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> ${CMAKE_SOURCE_DIR}/tests/stubs/dummy.c "-fplugin-arg-pygcc-command=import gcc; gcc.register_callback\(gcc.PLUGIN_FINISH, lambda s: print\('hello', s\), 'world'\)"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        # COMMENT "Running GCC plugin check..."
        )
set_tests_properties(pygcc_plugin_arg_4 PROPERTIES
        PASS_REGULAR_EXPRESSION "hello world"
        )

add_test(NAME pygcc_plugin_pass_1
        COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> ${CMAKE_SOURCE_DIR}/tests/stubs/dummy.c "-fplugin-arg-pygcc-command=import gcc; pm = gcc.PassManger(); print\(pm\);"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        # COMMENT "Running GCC plugin check..."
        )
set_tests_properties(pygcc_plugin_pass_1 PROPERTIES
        PASS_REGULAR_EXPRESSION "No constructor defined!"
        )

add_test(NAME pygcc_plugin_pass_2
        COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> ${CMAKE_SOURCE_DIR}/tests/stubs/dummy.c "-fplugin-arg-pygcc-command=import gcc; print\(dir\(gcc.passManger\)\); print\(dir\(gcc.passManger\)\);"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        # COMMENT "Running GCC plugin check..."
        )
set_tests_properties(pygcc_plugin_pass_2 PROPERTIES
        PASS_REGULAR_EXPRESSION "__self__"
        )

add_test(NAME pygcc_plugin_pass_enum
        COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> ${CMAKE_SOURCE_DIR}/tests/stubs/dummy.c "-fplugin-arg-pygcc-command=import gcc; print\(dir\(gcc.PassManger.PassPosition\)\);"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        # COMMENT "Running GCC plugin check..."
        )
set_tests_properties(pygcc_plugin_pass_enum PROPERTIES
        PASS_REGULAR_EXPRESSION "REPLACE"
        )

add_test(NAME pygcc_plugin_pass_register
        COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> ${CMAKE_SOURCE_DIR}/tests/stubs/dummy.c "-fplugin-arg-pygcc-command=import gcc; print\(dir\(gcc.PassManger\)\);"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        # COMMENT "Running GCC plugin check..."
        )
set_tests_properties(pygcc_plugin_pass_register PROPERTIES
        PASS_REGULAR_EXPRESSION "register_pass"
        )

add_test(NAME pygcc_plugin_cb_includefile
        COMMAND g++ -fplugin=$<TARGET_FILE:${PLUGIN_NAME}> ${CMAKE_SOURCE_DIR}/tests/stubs/dummy.c "-fplugin-arg-pygcc-script=${CMAKE_SOURCE_DIR}/tests/test_eventcb_include_file.py"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        # COMMENT "Running GCC plugin check..."
        )
set_tests_properties(pygcc_plugin_cb_includefile PROPERTIES
        PASS_REGULAR_EXPRESSION "stdio.h"
        )
