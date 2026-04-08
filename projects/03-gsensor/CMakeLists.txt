cmake_minimum_required(VERSION 3.22)

project(gsensor
    VERSION 1.0
    DESCRIPTION "TFL_CONNECT_2 LSM6DSO G-Sensor read with rotation-matrix calibration"
    LANGUAGES C ASM
)

# ── Paths (installed on this machine) ────────────────────────────────────────
set(CUBE_H5_ROOT "C:/Users/arikg/STM32Cube/Repository/STM32Cube_FW_H5_V1.6.0")
set(HAL_INC    "${CUBE_H5_ROOT}/Drivers/STM32H5xx_HAL_Driver/Inc")
set(HAL_SRC    "${CUBE_H5_ROOT}/Drivers/STM32H5xx_HAL_Driver/Src")
set(CMSIS_INC  "${CUBE_H5_ROOT}/Drivers/CMSIS/Include")
set(CMSIS_DEV  "${CUBE_H5_ROOT}/Drivers/CMSIS/Device/ST/STM32H5xx/Include")
set(CMSIS_TMPL "${CUBE_H5_ROOT}/Drivers/CMSIS/Device/ST/STM32H5xx/Source/Templates")

# ── Target executable ─────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(${PROJECT_NAME}.elf PRIVATE
    # Application
    Core/Src/main.c
    Core/Src/lsm6dso.c
    Core/Src/calibration.c
    Core/Src/stm32h5xx_hal_msp.c
    Core/Src/stm32h5xx_it.c

    # CMSIS system init
    ${CMSIS_TMPL}/system_stm32h5xx.c

    # Startup (GCC)
    ${CMSIS_TMPL}/gcc/startup_stm32h573xx.s

    # HAL modules used by this project
    ${HAL_SRC}/stm32h5xx_hal.c
    ${HAL_SRC}/stm32h5xx_hal_cortex.c
    ${HAL_SRC}/stm32h5xx_hal_gpio.c
    ${HAL_SRC}/stm32h5xx_hal_pwr.c
    ${HAL_SRC}/stm32h5xx_hal_pwr_ex.c
    ${HAL_SRC}/stm32h5xx_hal_rcc.c
    ${HAL_SRC}/stm32h5xx_hal_rcc_ex.c
    ${HAL_SRC}/stm32h5xx_hal_spi.c
    ${HAL_SRC}/stm32h5xx_hal_spi_ex.c
    ${HAL_SRC}/stm32h5xx_hal_uart.c
    ${HAL_SRC}/stm32h5xx_hal_uart_ex.c
    ${HAL_SRC}/stm32h5xx_hal_flash.c
    ${HAL_SRC}/stm32h5xx_hal_flash_ex.c
    ${HAL_SRC}/stm32h5xx_hal_dma.c
    ${HAL_SRC}/stm32h5xx_hal_dma_ex.c
)

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc
    ${HAL_INC}
    ${HAL_INC}/Legacy
    ${CMSIS_INC}
    ${CMSIS_DEV}
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    STM32H573xx
    USE_HAL_DRIVER
)

# ── Compile options ───────────────────────────────────────────────────────────
target_compile_options(${PROJECT_NAME}.elf PRIVATE
    -Wall
    -Wextra
    -Werror
    -fdata-sections
    -ffunction-sections
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2>
)

# ── Linker ────────────────────────────────────────────────────────────────────
target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/STM32H573VITXQ_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    --specs=nosys.specs
)

# ── Suppress warnings in ST HAL sources (we cannot modify them) ──────────────
file(GLOB HAL_C_SOURCES "${HAL_SRC}/*.c")
set_source_files_properties(${HAL_C_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-parameter"
)

# ── Post-build: print size and generate .hex ──────────────────────────────────
set(GCC_BIN
    "C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin"
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND "${GCC_BIN}/arm-none-eabi-size.exe" $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMAND "${GCC_BIN}/arm-none-eabi-objcopy.exe"
            -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${PROJECT_NAME}.hex
    COMMENT "Size report and generating ${PROJECT_NAME}.hex"
)
