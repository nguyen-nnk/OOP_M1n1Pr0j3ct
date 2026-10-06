cmake_minimum_required(VERSION 3.16)
project(xocdia_oop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(xocdia_core
    src/BettingStrategy.cpp
    src/Simulator.cpp
    src/XocDiaGame.cpp
)

target_include_directories(xocdia_core PUBLIC include)
target_compile_options(xocdia_core PRIVATE -Wall -Wextra -pedantic)

add_executable(xocdia src/main.cpp)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NCURSESW REQUIRED ncursesw)
target_include_directories(xocdia PRIVATE ${NCURSESW_INCLUDE_DIRS})
target_link_directories(xocdia PRIVATE ${NCURSESW_LIBRARY_DIRS})
target_link_libraries(xocdia PRIVATE xocdia_core ${NCURSESW_LIBRARIES})
target_compile_options(xocdia PRIVATE ${NCURSESW_CFLAGS_OTHER})
target_compile_options(xocdia PRIVATE -Wall -Wextra -pedantic)

add_executable(strategy_tests tests/strategy_tests.cpp)
target_link_libraries(strategy_tests PRIVATE xocdia_core)
target_compile_options(strategy_tests PRIVATE -Wall -Wextra -pedantic)

enable_testing()
add_test(NAME strategy_tests COMMAND strategy_tests)
