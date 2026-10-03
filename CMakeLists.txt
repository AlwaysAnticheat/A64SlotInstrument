cmake_minimum_required(VERSION 3.22)

project(A64SlotInstrument
    VERSION 0.1.0
    DESCRIPTION "AArch64 writable function-pointer slot instrumentation"
    LANGUAGES CXX ASM
)

option(A64SLOTINSTRUMENT_BUILD_EXAMPLES "Build the examples/ snippets as a static library" ON)

add_library(A64SlotInstrument STATIC
    src/A64SlotInstrument.cpp
    src/A64SlotInstrumentEntry.S
)

add_library(A64SlotInstrument::A64SlotInstrument ALIAS A64SlotInstrument)

target_include_directories(A64SlotInstrument PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(A64SlotInstrument PUBLIC cxx_std_17)

set_target_properties(A64SlotInstrument PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

if(A64SLOTINSTRUMENT_BUILD_EXAMPLES)
    add_library(A64SlotInstrumentExamples STATIC
        examples/01_filter_by_argument.cpp
        examples/02_fp_neon_inspect.cpp
        examples/03_frame_lr_sp.cpp
    )
    target_link_libraries(A64SlotInstrumentExamples PRIVATE A64SlotInstrument)
    set_target_properties(A64SlotInstrumentExamples PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
endif()
