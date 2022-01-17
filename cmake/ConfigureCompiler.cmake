set(CMAKE_C_COMPILER "x86_64-w64-mingw32-gcc-posix")
set(CMAKE_CXX_COMPILER "x86_64-w64-mingw32-g++-posix")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -static -static-libgcc -static-libstdc++")
set(CMAKE_SYSTEM_NAME "Windows")
set(CMAKE_CROSSCOMPILING_EMULATOR "wine")

set(CMAKE_CXX_STANDARD 17)
