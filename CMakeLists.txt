cmake_minimum_required(VERSION 3.10)

project(MutexStringDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mutex_string_demo
    src/main.cpp
    src/MutexString.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(mutex_string_demo PRIVATE Threads::Threads)

# (선택) Windows + MinGW 환경에서 vcpkg 설정
if (WIN32 AND MINGW)
  set(VCPKG_ROOT $ENV{USERPROFILE})
  set(CMAKE_PREFIX_PATH    "${VCPKG_ROOT}/vcpkg/installed/x64-mingw-static")
  set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

