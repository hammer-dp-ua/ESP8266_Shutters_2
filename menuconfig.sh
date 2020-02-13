#!/bin/sh
# Executed outside CLion at first time to configure project’s ESP8266_RTOS_SDK settings
# From C:\msys32\msys2.exe shell

export IDF_PATH=/c/Users/USER/ESP8266/ESP8266_RTOS_SDK_latest &&\
export MSYSTEM=MINGW32 &&\
export MINGW_BIN_PATH=/c/msys32/mingw32/bin &&\
export GIT_PATH=/c/msys32/Git/mingw64/bin &&\
export XTENSA_BIN_PATH=/c/Users/USER/ESP8266/xtensa-lx106-elf-win32-1.22.0-100-ge567ec7-5.2.0/bin &&\
export CMAKE_BIN_PATH=/c/Users/USER/IntellijIdea/CLion-2019.3.3.win/bin/cmake/win/bin &&\
export PATH=$PATH:$MINGW_BIN_PATH:$GIT_PATH:$XTENSA_BIN_PATH:$CMAKE_BIN_PATH &&\
export PYTHON=$MINGW_BIN_PATH/python.exe

echo PATH: $PATH
$PYTHON $IDF_PATH/tools/idf.py menuconfig
