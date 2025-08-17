cmake_minimum_required(VERSION 3.20)

project(MutexStringDemo LANGUAGES CXX)

# 표준 설정
set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD 17) # C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 실행 파일 구성
add_executable(mutex_string_demo
    src/main.cpp
    src/MutexString.cpp
    src/MutexString.hpp   # 헤더를 목록에 넣으면 IDE에서 보기 편합니다(컴파일엔 영향 없음)
)

# 헤더 탐색 경로
target_include_directories(mutex_string_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 스레드 라이브러리
find_package(Threads REQUIRED)
target_link_libraries(mutex_string_demo PRIVATE Threads::Threads)

# 컴파일 경고 옵션(선택)
if (MSVC)
  target_compile_options(mutex_string_demo PRIVATE /W4 /permissive- /EHsc /Zc:preprocessor)
else()
  target_compile_options(mutex_string_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# LTO(선택): Release에서만 시도
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok OUTPUT ipo_msg)
if (ipo_ok)
  set_property(TARGET mutex_string_demo PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# ---- vcpkg 사용 안내 ----
# vcpkg toolchain은 CMake configure 단계에서 지정해야 합니다.
# 예시:
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
