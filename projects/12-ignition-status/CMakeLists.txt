cmake_minimum_required(VERSION 3.22)

project(ignition_status
    VERSION 1.0
    DESCRIPTION "12-ignition-status: ADC1 Ignition (PC0/CH10) → LED_G (PC9) ON/OFF at threshold 300; P3V3_SW_EN permanently ON"
    LANGUAGES C ASM
)

# ── LowLevel folder (CubeMX-generated sources, Drivers, linker script) ────────
set(LL "${CMAKE_SOURCE_DIR}/../../LowLevel")

# ── Target executable ─────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf)

# ── Project application sources ───────────────────────────────────────────────
target_sources(${PROJECT_NAME}.elf PRIVATE
    Core/Src/main.c
    Core/Src/stm32h5xx_it.c
)

# ── LowLevel CubeMX-generated Core/Src (used verbatim) ───────────────────────
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${LL}/Core/Src/gpio.c
    ${LL}/Core/Src/adc.c
    ${LL}/Core/Src/fdcan.c
    ${LL}/Core/Src/gpdma.c
    ${LL}/Core/Src/i2c.c
    ${LL}/Core/Src/iwdg.c
    ${LL}/Core/Src/rtc.c
    ${LL}/Core/Src/spi.c
    ${LL}/Core/Src/tim.c
    ${LL}/Core/Src/usart.c
    ${LL}/Core/Src/stm32h5xx_hal_msp.c
    ${LL}/Core/Src/stm32h5xx_hal_timebase_tim.c
    ${LL}/Core/Src/syscalls.c
    ${LL}/Core/Src/sysmem.c
    ${LL}/Core/Src/system_stm32h5xx.c
    ${LL}/newlib_lock_glue.c
    ${LL}/startup_stm32h573xx.s
)

# ── LowLevel HAL driver sources ───────────────────────────────────────────────
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_cortex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gpio.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr_ex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc_ex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma_ex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_exti.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_spi.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_spi_ex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart_ex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_tim.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_tim_ex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_adc.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_adc_ex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_fdcan.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_i2c.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_i2c_ex.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_iwdg.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rtc.c
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rtc_ex.c
)

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc
    ${LL}/Core/Inc
    ${LL}
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Inc
    ${LL}/Drivers/STM32H5xx_HAL_Driver/Inc/Legacy
    ${LL}/Drivers/CMSIS/Device/ST/STM32H5xx/Include
    ${LL}/Drivers/CMSIS/Include
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    STM32H573xx
    USE_HAL_DRIVER
    STM32_THREAD_SAFE_STRATEGY=2
    $<$<CONFIG:Debug>:DEBUG>
)

# ── Compile options ───────────────────────────────────────────────────────────
target_compile_options(${PROJECT_NAME}.elf PRIVATE
    -Wall
    -Wextra
    -Werror
    -fdata-sections
    -ffunction-sections
    -fstack-usage
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-Os -g0>
)

# ── Suppress -Wunused-parameter in ST HAL and LowLevel CubeMX sources ─────────
file(GLOB HAL_C_SOURCES "${LL}/Drivers/STM32H5xx_HAL_Driver/Src/*.c")
file(GLOB LL_CORE_C_SOURCES "${LL}/Core/Src/*.c")
set_source_files_properties(
    ${HAL_C_SOURCES}
    ${LL_CORE_C_SOURCES}
    ${LL}/newlib_lock_glue.c
    PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter"
)

# ── Linker ────────────────────────────────────────────────────────────────────
target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${LL}/STM32H573xx_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--print-memory-usage
)

# ── Post-build: size + hex ────────────────────────────────────────────────────
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
