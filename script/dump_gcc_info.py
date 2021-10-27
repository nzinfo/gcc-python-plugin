# -*- coding: utf-8 -*-

"""
    用于 根据 CMake 检测到的 元数据 GCC_PLUGINS_DIR 读取具体的文件

    为简单起见，直接 dump 原始文件
"""
import os
import sys
import re


def get_gcc_plugin_location():
    # 读取当前路径 ， 读取 CMakeCache.txt ，找到对应变量
    pwd = os.path.curdir
    gcc_plugins = []
    with open(os.path.join(pwd, "CMakeCache.txt"), 'r') as f:
        ctx = f.read()
        gcc_plugins = re.findall(r'GCC_PLUGINS_DIR:INTERNAL=(.*)', ctx)
    if len(gcc_plugins):
        return gcc_plugins[0]
    else:
        raise KeyError


def dump_file(filename, strip=False):
    with open(filename, 'r') as f:
        ctx = f.read()
    if strip:
        events = re.findall(r'\n\n/\*(.*?)\*/[\n|\r\n]DEFEVENT\s\((.*?)\)', ctx, re.MULTILINE|re.DOTALL)
        # print(events)
        ctx = ""
        return events
    return ctx


def main_plugin():
    gcc_plugin_loc = get_gcc_plugin_location()
    def_filename = os.path.join(gcc_plugin_loc, 'include', 'plugin.def')
    for e in dump_file(def_filename, True):
        print("%s:\t%s" % (e[1], e[0].strip().replace("\n", " ")))



if __name__ == '__main__':
    cmd = sys.argv[1]
    available_cmds = ["plugin", ]
    if cmd not in available_cmds:
        print("only sub-commands available as ", available_cmds)

    if cmd == "plugin":
        main_plugin()

# eof
