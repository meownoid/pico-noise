cmake_minimum_required(VERSION 3.12)

set(NAME pico-noise)

include(pico_sdk_import.cmake)
include(pimoroni_pico_import.cmake)

project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${NAME}
    main.cpp
)

target_link_libraries(${NAME} pico_stdlib pico_audio_i2s hardware_adc)

target_compile_definitions(
    ${NAME}
    PRIVATE
    PICO_AUDIO_I2S_MONO_INPUT=1
    USE_AUDIO_I2S=1
    PICO_FLOAT_SUPPORT_ROM_V1=0
    PICO_DOUBLE_SUPPORT_ROM_V1=0
)

pico_add_extra_outputs(${NAME})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2
    DESTINATION .
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)
