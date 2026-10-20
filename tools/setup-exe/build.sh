#!/bin/sh
# Builds Setup.exe in the repo root with MinGW-w64 (Debian: g++-mingw-w64-x86-64-posix).
set -e
cd "$(dirname "$0")"
[ -f setup.ico ] || python3 make-icon.py
x86_64-w64-mingw32-windres setup.rc -O coff -o setup.res
x86_64-w64-mingw32-g++-posix -std=c++20 -O2 -s -static -municode -mwindows \
  -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 setup.cpp setup.res -o ../../Setup.exe \
  -lgdiplus -lcomctl32 -lcomdlg32 -ldwmapi -luxtheme -lshell32 -lole32
rm -f setup.res
echo "built ../../Setup.exe"
