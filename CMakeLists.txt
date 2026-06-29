cmake_minimum_required(VERSION 3.22)

project(trq_2026)

include(cmake/gcc-arm-none-eabi.cmake)
enable_language(C CXX ASM)

# ============================================================
# Git hash
# ============================================================
execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "00000000")
endif()

message(STATUS "Git hash: ${GIT_COMMIT_HASH}")

# ============================================================
# STM32CubeProgrammer Path
# ============================================================
if(CMAKE_HOST_WIN32)
    set(_exe ".exe")
else()
    set(_exe "")
endif()

set(STM32_PROGRAMMER_CLI
    "${CLT_PATH}/STM32CubeProgrammer/bin/STM32_Programmer_CLI${_exe}")

if(NOT EXISTS "${STM32_PROGRAMMER_CLI}")
    message(FATAL_ERROR
        "STM32_Programmer_CLI not found at: ${STM32_PROGRAMMER_CLI}\n"
        "Check that STM32CubeProgrammer is installed inside STM32CubeCLT.")
endif()

# ============================================================
# Python
# ============================================================
find_program(PYTHON python3 REQUIRED)

set(VENV_DIR ${CMAKE_SOURCE_DIR}/.venv)

if(CMAKE_HOST_WIN32)
    set(VENV_PYTHON ${VENV_DIR}/Scripts/python.exe)
else()
    set(VENV_PYTHON ${VENV_DIR}/bin/python)
endif()

if(NOT EXISTS ${VENV_PYTHON})
    execute_process(
        COMMAND ${PYTHON} -m venv ${VENV_DIR}
        RESULT_VARIABLE VENV_RESULT
    )

    if(NOT VENV_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to create virtual environment")
    endif()

    execute_process(COMMAND ${VENV_PYTHON} -m pip install --upgrade pip)

    execute_process(
        COMMAND ${VENV_PYTHON} -m pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt
        RESULT_VARIABLE PIP_RESULT
    )

    if(NOT PIP_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install Python dependencies")
    endif()
endif()

execute_process(
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/tools/find_stm32cubeclt.py ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE _clt_exit
)

if(NOT _clt_exit EQUAL 0)
    message(FATAL_ERROR "find_stm32cubeclt.py failed (exit ${_clt_exit})")
endif()

set(PYTHON ${VENV_PYTHON})

# ============================================================
# Memory layout
# ============================================================
set(BL_OFFSET       0x0000)
set(UPDATER_OFFSET  0x4000)
set(APP_OFFSET      0x20000)

set(UPDATER_BASE    0x08004000)
set(APP_BASE        0x08020000)

# ============================================================
# Output files
# ============================================================
set(BL_MAP              ${CMAKE_BINARY_DIR}/bootloader.map)
set(BL_BIN              ${CMAKE_BINARY_DIR}/bootloader.bin)

set(UPDATER_MAP         ${CMAKE_BINARY_DIR}/updater.map)
set(UPDATER_BIN_RAW     ${CMAKE_BINARY_DIR}/updater_raw.bin)
set(UPDATER_BIN_PATCHED ${CMAKE_BINARY_DIR}/updater_patched.bin)

set(APP_MAP             ${CMAKE_BINARY_DIR}/app.map)
set(APP_BIN_RAW         ${CMAKE_BINARY_DIR}/app_raw.bin)
set(APP_BIN_PATCHED     ${CMAKE_BINARY_DIR}/app_patched.bin)

set(MERGED_BIN          ${CMAKE_BINARY_DIR}/firmware.bin)

# ============================================================
# MCU FLAGS
# ============================================================
set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

add_library(mcu INTERFACE)

target_compile_options(mcu INTERFACE
    ${CPU_FLAGS}
    -Wall -Wextra -Wno-unused-parameter
    -ffunction-sections -fdata-sections
    -fno-builtin
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-Os>
)

target_link_options(mcu INTERFACE
    ${CPU_FLAGS}
    -Wl,--gc-sections
    -Wl,--print-memory-usage
)

target_compile_definitions(mcu INTERFACE
    STM32F407xx
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
)

target_include_directories(mcu INTERFACE
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
)

# ============================================================
# HAL DRIVER
# ============================================================
set(HAL_PATH ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src)

add_library(stm32f4_hal STATIC
    ${HAL_PATH}/stm32f4xx_hal.c
    # ${HAL_PATH}/stm32f4xx_hal_adc.c
    # ${HAL_PATH}/stm32f4xx_hal_adc_ex.c
    ${HAL_PATH}/stm32f4xx_hal_rcc.c
    ${HAL_PATH}/stm32f4xx_hal_rcc_ex.c
    ${HAL_PATH}/stm32f4xx_hal_flash.c
    ${HAL_PATH}/stm32f4xx_hal_flash_ex.c
    # ${HAL_PATH}/stm32f4xx_hal_flash_ramfunc.c
    ${HAL_PATH}/stm32f4xx_hal_gpio.c
    # ${HAL_PATH}/stm32f4xx_hal_dma_ex.c
    # ${HAL_PATH}/stm32f4xx_hal_dma.c
    # ${HAL_PATH}/stm32f4xx_hal_pwr.c
    # ${HAL_PATH}/stm32f4xx_hal_pwr_ex.c
    ${HAL_PATH}/stm32f4xx_hal_cortex.c
    # ${HAL_PATH}/stm32f4xx_hal_exti.c
    # ${HAL_PATH}/stm32f4xx_hal_dac.c
    # ${HAL_PATH}/stm32f4xx_hal_dac_ex.c
    # ${HAL_PATH}/stm32f4xx_hal_tim.c
    # ${HAL_PATH}/stm32f4xx_hal_tim_ex.c
    ${HAL_PATH}/stm32f4xx_hal_uart.c
)

target_include_directories(stm32f4_hal
    PUBLIC
        ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
        ${CMAKE_SOURCE_DIR}/common/inc
)

target_compile_definitions(stm32f4_hal
    PUBLIC
        USE_HAL_DRIVER
)

target_link_libraries(stm32f4_hal PUBLIC mcu)

# ============================================================
# APPLICATION
# ============================================================
add_executable(app.elf)

target_sources(app.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/core/src/stm32f4xx_hal_msp.c
    ${CMAKE_SOURCE_DIR}/core/startup/startup_stm32f407vgtx.s
    ${CMAKE_SOURCE_DIR}/core/src/system_stm32f4xx.c
    ${CMAKE_SOURCE_DIR}/core/src/sysmem.c
    ${CMAKE_SOURCE_DIR}/core/src/syscalls.c
    ${CMAKE_SOURCE_DIR}/core/src/stm32f4xx_it.c
    ${CMAKE_SOURCE_DIR}/core/src/main.c
    ${CMAKE_SOURCE_DIR}/core/src/image_header.c
    ${CMAKE_SOURCE_DIR}/common/src/nextion.c
)

target_include_directories(app.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/core/inc
    ${CMAKE_SOURCE_DIR}/common/inc
)

target_link_options(app.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/linker/app.ld
    -Wl,-Map=${APP_MAP}
    --specs=nano.specs
    --specs=nosys.specs
)

target_link_libraries(app.elf PRIVATE stm32f4_hal mcu)

# ------------------------------------------------------------
# RAW BIN - APPLICATION
# ------------------------------------------------------------
add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:app.elf> ${APP_BIN_RAW}
)

# ------------------------------------------------------------
# MEMAP - APPLICATION
# ------------------------------------------------------------
add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/tools/memap.py
            -t GCC_ARM ${APP_MAP} -d 3
    COMMENT "Memory usage analysis (app)"
)

add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/patch_image_header.py
            ${APP_BIN_RAW}
            ${APP_BIN_PATCHED}
            --base-addr ${APP_BASE}
)

# ============================================================
# BOOTLOADER
# ============================================================
add_executable(bootloader.elf)

target_sources(bootloader.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/bootloader/startup/startup_stm32f407vgtx.s
    ${CMAKE_SOURCE_DIR}/bootloader/src/bootloader.c
    ${CMAKE_SOURCE_DIR}/bootloader/src/system_stm32f4xx.c
)

target_include_directories(bootloader.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/bootloader/inc
    ${CMAKE_SOURCE_DIR}/common/inc
)

target_link_options(bootloader.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/linker/bootloader.ld
    -Wl,-Map=${BL_MAP}
    --specs=nano.specs
    --specs=nosys.specs
)

target_link_libraries(bootloader.elf PRIVATE stm32f4_hal mcu)

# ------------------------------------------------------------
# RAW BIN - BOOTLOADER
# ------------------------------------------------------------
add_custom_command(TARGET bootloader.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bootloader.elf> ${BL_BIN}
    COMMENT "Generating bootloader binary"
)

# ============================================================
# UPDATER
# ============================================================
if(NOT DEFINED UPDATER_BACKEND)
    set(UPDATER_BACKEND "USB_OTG")
endif()

# USBH and FatFS Paths 
set(UPDATER_MW_USB ${CMAKE_SOURCE_DIR}/updater/Middlewares/ST/STM32_USB_Host_Library)
set(UPDATER_MW_FATFS ${CMAKE_SOURCE_DIR}/updater/Middlewares/Third_Party/FatFs/src)

if(UPDATER_BACKEND STREQUAL "CH376_UART")
    set(_UPDATER_BACKEND_DEFINE UPDATER_BACKEND_CH376_UART)
    set(_UPDATER_BACKEND_SOURCES 
        ${CMAKE_SOURCE_DIR}/updater/src/storage_ch376_uart.c
    )
    set(_UPDATER_BACKEND_INCLUDES)
elseif(UPDATER_BACKEND STREQUAL "USB_OTG")
    set(_UPDATER_BACKEND_DEFINE UPDATER_BACKEND_USB_OTG)
    set(_UPDATER_BACKEND_SOURCES 
        ${CMAKE_SOURCE_DIR}/updater/src/storage_usb_otg.c

        ${UPDATER_MW_USB}/Core/Src/usbh_core.c
        ${UPDATER_MW_USB}/Core/Src/usbh_ctlreq.c
        ${UPDATER_MW_USB}/Core/Src/usbh_ioreq.c
        ${UPDATER_MW_USB}/Core/Src/usbh_pipes.c

        ${UPDATER_MW_USB}/Class/MSC/Src/usbh_msc.c
        ${UPDATER_MW_USB}/Class/MSC/Src/usbh_msc_bot.c
        ${UPDATER_MW_USB}/Class/MSC/Src/usbh_msc_scsi.c

        ${UPDATER_MW_FATFS}/ff.c
        ${UPDATER_MW_FATFS}/ff_gen_drv.c
        ${UPDATER_MW_FATFS}/diskio.c

        ${CMAKE_SOURCE_DIR}/updater/USB_HOST/Target/usbh_conf.c
        ${CMAKE_SOURCE_DIR}/updater/USB_HOST/App/usb_host.c
        ${CMAKE_SOURCE_DIR}/updater/FATFS/Target/usbh_diskio.c
        ${CMAKE_SOURCE_DIR}/updater/FATFS/App/fatfs.c

        ${HAL_PATH}/stm32f4xx_hal_hcd.c
        ${HAL_PATH}/stm32f4xx_ll_usb.c
    )

    set(_UPDATER_BACKEND_INCLUDES
        ${UPDATER_MW_USB}/Core/Inc
        ${UPDATER_MW_USB}/Class/MSC/Inc
        ${UPDATER_MW_FATFS}
        ${CMAKE_SOURCE_DIR}/updater/FATFS/Target
        ${CMAKE_SOURCE_DIR}/updater/FATFS/App
        ${CMAKE_SOURCE_DIR}/updater/USB_HOST/Target
        ${CMAKE_SOURCE_DIR}/updater/USB_HOST/App
    )

else()
    message(FATAL_ERROR "Unknown UPDATER_BACKEND '${UPDATER_BACKEND}'.\n"
                        "Valid values: CH376_UART  USB_OTG\n")
endif()

message(STATUS "Updater backend: ${UPDATER_BACKEND}  (${_UPDATER_BACKEND_DEFINE})")

add_executable(updater.elf)

target_sources(updater.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/updater/startup/startup_stm32f407vgtx.s
    ${CMAKE_SOURCE_DIR}/updater/src/system_stm32f4xx.c
    ${CMAKE_SOURCE_DIR}/updater/src/sysmem.c
    ${CMAKE_SOURCE_DIR}/updater/src/syscalls.c
    ${CMAKE_SOURCE_DIR}/updater/src/updater.c
    ${CMAKE_SOURCE_DIR}/updater/src/image_header.c
    ${CMAKE_SOURCE_DIR}/updater/src/firmware_validator.c
    ${CMAKE_SOURCE_DIR}/common/src/flash.c
    ${CMAKE_SOURCE_DIR}/common/src/crc.c
    ${CMAKE_SOURCE_DIR}/common/src/nextion.c
    ${_UPDATER_BACKEND_SOURCES}
)

target_include_directories(updater.elf PRIVATE
    ${_UPDATER_BACKEND_INCLUDES}
    ${CMAKE_SOURCE_DIR}/updater/inc
    ${CMAKE_SOURCE_DIR}/common/inc
)

target_compile_definitions(updater.elf PRIVATE
    ${_UPDATER_BACKEND_DEFINE}
)

target_link_options(updater.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/linker/updater.ld
    -Wl,-Map=${UPDATER_MAP}
    --specs=nano.specs
    --specs=nosys.specs
)

target_link_libraries(updater.elf PRIVATE stm32f4_hal mcu)

# ------------------------------------------------------------
# RAW BIN - UPDATER
# ------------------------------------------------------------
add_custom_command(TARGET updater.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:updater.elf> ${UPDATER_BIN_RAW}
    COMMENT "Generating updater raw binary"
)

# ------------------------------------------------------------
# MEMAP - UPDATER
# ------------------------------------------------------------
add_custom_command(TARGET updater.elf POST_BUILD
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/tools/memap.py
            -t GCC_ARM ${UPDATER_MAP} -d 3
    COMMENT "Memory usage analysis (updater)"
)

add_custom_command(TARGET updater.elf POST_BUILD
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/patch_image_header.py
            ${UPDATER_BIN_RAW}
            ${UPDATER_BIN_PATCHED}
            --base-addr ${UPDATER_BASE}
    COMMENT "Patching updater image header"
)

# ============================================================
# MERGE - Boot + Updater + App -> firmware.bin
# ============================================================
add_dependencies(app.elf bootloader.elf updater.elf)

add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${PYTHON}
            ${CMAKE_SOURCE_DIR}/scripts/merge_bootloader.py
            ${BL_BIN}
            ${UPDATER_BIN_PATCHED}
            ${APP_BIN_PATCHED}
            ${MERGED_BIN}
            --updater-offset ${UPDATER_OFFSET}
            --app-offset     ${APP_OFFSET}
    COMMENT "Merging Boot + Updater + Application → firmware.bin"
)

# ============================================================
# FLASH TARGETS
# ============================================================
# Full
add_custom_target(flash
    DEPENDS app.elf
    COMMAND "${STM32_PROGRAMMER_CLI}"
        -c port=swd mode=UR
        -d ${MERGED_BIN} 0x08000000
        -v -hardRst
    COMMENT "Flashing Boot + Updater + App"
)

add_custom_target(flash_app
    DEPENDS app.elf
    COMMAND "${STM32_PROGRAMMER_CLI}"
        -c port=swd mode=UR
        -d ${APP_BIN_PATCHED} ${APP_BASE}
        -v -hardRst
)

add_custom_target(flash_updater
    DEPENDS updater.elf
    COMMAND "${STM32_PROGRAMMER_CLI}"
        -c port=swd mode=UR
        -d ${UPDATER_BIN_PATCHED} ${UPDATER_BASE}
        -v -hardRst
)