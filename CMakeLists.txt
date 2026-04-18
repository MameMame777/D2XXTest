cmake_minimum_required(VERSION 3.20)
project(D2XXTest LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Language standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /WX /utf-8)
    # Suppress MSVC C4996 (deprecated POSIX names) where needed
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ---------------------------------------------------------------------------
# D2XX SDK location
# Users must place the FTDI D2XX SDK files in this directory.
# See third_party/ftd2xx/README.txt for details.
# ---------------------------------------------------------------------------
set(FTDI_D2XX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ftd2xx"
    CACHE PATH "Path to the FTDI D2XX SDK directory (contains ftd2xx.h)")

# Accept official SDK layout (libftd2xx/ subfolder with ftd2xx.h + WinTypes.h)
if(EXISTS "${FTDI_D2XX_DIR}/libftd2xx/ftd2xx.h")
    set(FTDI_D2XX_INCLUDE_DIR "${FTDI_D2XX_DIR}/libftd2xx")
    message(STATUS "D2XX SDK headers: ${FTDI_D2XX_INCLUDE_DIR} (official SDK)")
elseif(EXISTS "${FTDI_D2XX_DIR}/ftd2xx.h")
    set(FTDI_D2XX_INCLUDE_DIR "${FTDI_D2XX_DIR}")
    message(STATUS "D2XX SDK headers: ${FTDI_D2XX_INCLUDE_DIR}")
else()
    message(FATAL_ERROR
        "ftd2xx.h not found in '${FTDI_D2XX_DIR}' or '${FTDI_D2XX_DIR}/libftd2xx'.\n"
        "Download the D2XX SDK from https://ftdichip.com/drivers/d2xx-drivers/\n"
        "and place the SDK files in '${FTDI_D2XX_DIR}/libftd2xx/'.\n"
        "See third_party/ftd2xx/README.txt for the expected directory layout.")
endif()

# ---------------------------------------------------------------------------
# Find the D2XX library
# ---------------------------------------------------------------------------
if(WIN32)
    # Prefer Static (no DLL dependency), then import lib (needs ftd2xx.dll at run-time)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_arch_dirs
            "${FTDI_D2XX_DIR}/Static/amd64"
            "${FTDI_D2XX_DIR}/amd64"
            "${FTDI_D2XX_DIR}")
    else()
        set(_arch_dirs
            "${FTDI_D2XX_DIR}/Static/i386"
            "${FTDI_D2XX_DIR}/i386"
            "${FTDI_D2XX_DIR}")
    endif()

    find_library(FTDI_LIB
        NAMES ftd2xx
        PATHS ${_arch_dirs}
        NO_DEFAULT_PATH
        REQUIRED)

    message(STATUS "D2XX library: ${FTDI_LIB}")
elseif(UNIX)
    find_library(FTDI_LIB
        NAMES ftd2xx
        PATHS "${FTDI_D2XX_DIR}" "${FTDI_D2XX_DIR}/build"
        REQUIRED)
    message(STATUS "D2XX library: ${FTDI_LIB}")
endif()

# ---------------------------------------------------------------------------
# Static library: d2xx_mpsse
# ---------------------------------------------------------------------------
add_library(d2xx_mpsse STATIC
    src/MpsseDevice.cpp
    src/SpiMaster.cpp
    src/Gpio.cpp
    src/Eeprom.cpp
)

target_include_directories(d2xx_mpsse
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"   # exposes d2xx_mpsse/*.hpp to consumers
    PRIVATE
        "${FTDI_D2XX_INCLUDE_DIR}"              # ftd2xx.h (not leaked to consumers)
)

if(WIN32)
    target_link_libraries(d2xx_mpsse PUBLIC "${FTDI_LIB}" SetupAPI)
elseif(UNIX)
    target_link_libraries(d2xx_mpsse PUBLIC "${FTDI_LIB}" pthread dl)
endif()

# ---------------------------------------------------------------------------
# Helper to add test executables
# ---------------------------------------------------------------------------
function(add_d2xx_test NAME)
    add_executable(${NAME} "tests/${NAME}.cpp")
    target_link_libraries(${NAME} PRIVATE d2xx_mpsse)
    # Place executables next to DLL (Windows copy helper — optional)
    set_target_properties(${NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

# ---------------------------------------------------------------------------
# Test executables
# ---------------------------------------------------------------------------
add_d2xx_test(test_open)          # Enumerate devices, open Ch-B, MPSSE sync, close
add_d2xx_test(test_loopback)      # MPSSE internal loopback echo test
add_d2xx_test(test_eeprom_ftdi)   # FTDI internal EEPROM word dump (FT_ReadEE)
add_d2xx_test(test_eeprom_spi)    # External 25-series SPI EEPROM read
add_d2xx_test(test_debug)         # 4-step SPI debug (FT4232HL デバッグ切り分けガイド)

# ---------------------------------------------------------------------------
# On Windows: copy ftd2xx.dll next to executables (if import-lib was chosen)
# ---------------------------------------------------------------------------
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_dll_dir "${FTDI_D2XX_DIR}/amd64")
    else()
        set(_dll_dir "${FTDI_D2XX_DIR}/i386")
    endif()

    if(EXISTS "${_dll_dir}/ftd2xx.dll")
        message(STATUS "Will copy ftd2xx.dll from ${_dll_dir} to bin/")
        foreach(_tgt test_open test_loopback test_eeprom_ftdi test_eeprom_spi)
            add_custom_command(TARGET ${_tgt} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_dll_dir}/ftd2xx.dll"
                    "$<TARGET_FILE_DIR:${_tgt}>/ftd2xx.dll"
                COMMENT "Copying ftd2xx.dll to output directory")
        endforeach()
    endif()
endif()
