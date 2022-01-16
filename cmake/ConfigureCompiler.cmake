set(CMAKE_C_COMPILER "x86_64-w64-mingw32-gcc")
set(CMAKE_CXX_COMPILER "x86_64-w64-mingw32-g++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -std=gnu++11 -static -static-libgcc -static-libstdc++ -w")
set(CMAKE_SYSTEM_NAME "Windows")
set(CMAKE_CROSSCOMPILING_EMULATOR "wine")

set(CMAKE_CXX_STANDARD 11)
