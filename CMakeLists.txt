cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

if(NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Set it to your pico-sdk folder, then run make again.")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(pico_hill_drive C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_hill_drive
    main.c
    src/display_st7789.c
    src/input.c
    src/game.c
    src/sound.c
    src/score_store.c
)
target_include_directories(pico_hill_drive PRIVATE include)
target_link_libraries(pico_hill_drive
    pico_stdlib pico_multicore pico_flash pico_util
    hardware_spi hardware_pwm hardware_dma hardware_flash hardware_clocks
    m
)
target_compile_definitions(pico_hill_drive PRIVATE
    PICO_DEFAULT_SPI=0
    PICO_DEFAULT_SPI_SCK_PIN=18
    PICO_DEFAULT_SPI_TX_PIN=19
)
pico_enable_stdio_usb(pico_hill_drive 0)
pico_enable_stdio_uart(pico_hill_drive 0)
pico_override_flash_size(pico_hill_drive 4190208)
target_link_options(pico_hill_drive PRIVATE
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/pico_hill_drive.map"
)
pico_add_extra_outputs(pico_hill_drive)
