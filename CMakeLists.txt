cmake_minimum_required(VERSION 3.28)
project(PancakesBASIC LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_library(DIA_LIB NAMES diaguids.lib
        PATHS
        "C:/Program Files/Microsoft Visual Studio/18/Insiders/DIA SDK/lib/amd64"
        "C:/Program Files/Microsoft Visual Studio/18/Insiders/VC/Tools/MSVC/14.50.35717/lib/x64"
        REQUIRED
)
add_library(diaguids_fix UNKNOWN IMPORTED)

set_target_properties(diaguids_fix PROPERTIES
        IMPORTED_LOCATION "${DIA_LIB}"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG
        PATHS "C:/Program Files/LLVM/lib/cmake/llvm"
)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(PancakesBASIC src/main.cpp
)

target_sources(PancakesBASIC
        PRIVATE
        FILE_SET CXX_MODULES FILES
        src/pancakes/basic/frontend/util.cppm
        src/pancakes/basic/core/visitor.cppm
        src/pancakes/basic/settings/settings.cppm
        src/pancakes/basic/frontend/tokentype.cppm
        src/pancakes/basic/frontend/symbols.cppm
        src/pancakes/basic/frontend/keywords.cppm
        src/pancakes/basic/frontend/token.cppm
        src/pancakes/basic/frontend/lexer.cppm
        src/pancakes/basic/core/AST.cppm
        src/pancakes/basic/core/parser.cppm
        src/pancakes/execution/compiler.cppm
        src/pancakes/execution/interpreter.cppm
)

target_include_directories(PancakesBASIC PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(PancakesBASIC PRIVATE ${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs
        core
        support
        native
        codegen
        target
        analysis
        mc
        bitreader
        remarks
        bitstreamreader
        targetparser
        profiledata
        demangle
        ipo
        globalisel
)

add_library(pancakes_runtime STATIC
        runtime/pancakes_runtime.cpp
)
target_include_directories(pancakes_runtime PUBLIC runtime)

target_link_libraries(PancakesBASIC PRIVATE
        ${llvm_libs}
        diaguids_fix
        ws2_32
        dbghelp
        advapi32
        ntdll
        pancakes_runtime
)

if(MSVC)
    target_compile_options(PancakesBASIC PRIVATE /EHsc /wd4624)
endif()

set(PANCAKES_RUNTIME_LIB_DIR "${CMAKE_INSTALL_PREFIX}/PancakesBASIC/lib")
configure_file(runtime/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)
target_include_directories(PancakesBASIC PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(MSVC)
    target_compile_options(pancakes_runtime PRIVATE
            /GS-
            /O2
    )
endif()

install(TARGETS pancakes_runtime
        ARCHIVE DESTINATION PancakesBASIC/lib
        LIBRARY DESTINATION PancakesBASIC/lib
        RUNTIME DESTINATION PancakesBASIC/bin
)

install(TARGETS PancakesBASIC
        RUNTIME DESTINATION bin
)