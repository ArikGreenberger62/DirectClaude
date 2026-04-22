cmake_minimum_required(VERSION 3.22)

project(sms_modem
    VERSION 1.0
    DESCRIPTION "09-sms-modem: EG915N SMS via USART2, commands on UART7"
    LANGUAGES C ASM
)

set(LL  "${CMAKE_SOURCE_DIR}/../../LowLevel")
set(HAL "${LL}/Drivers/STM32H5xx_HAL_Driver")
set(GCC_BIN
    "C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin"
)

add_executable(${PROJECT_NAME}.elf)

# ── Application sources ───────────────────────────────────────────────────────
target_sources(${PROJECT_NAME}.elf PRIVATE
    Core/Src/main.c
    Core/Src/modem.c
    Core/Src/ring_buf.c
    Core/Src/stm32h5xx_it.c
)

# ── LowLevel CubeMX-generated Core/Src ───────────────────────────────────────
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

# ── HAL driver sources ────────────────────────────────────────────────────────
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${HAL}/Src/stm32h5xx_hal.c
    ${HAL}/Src/stm32h5xx_hal_cortex.c
    ${HAL}/Src/stm32h5xx_hal_gpio.c
    ${HAL}/Src/stm32h5xx_hal_pwr.c
    ${HAL}/Src/stm32h5xx_hal_pwr_ex.c
    ${HAL}/Src/stm32h5xx_hal_rcc.c
    ${HAL}/Src/stm32h5xx_hal_rcc_ex.c
    ${HAL}/Src/stm32h5xx_hal_flash.c
    ${HAL}/Src/stm32h5xx_hal_flash_ex.c
    ${HAL}/Src/stm32h5xx_hal_dma.c
    ${HAL}/Src/stm32h5xx_hal_dma_ex.c
    ${HAL}/Src/stm32h5xx_hal_exti.c
    ${HAL}/Src/stm32h5xx_hal_uart.c
    ${HAL}/Src/stm32h5xx_hal_uart_ex.c
    ${HAL}/Src/stm32h5xx_hal_tim.c
    ${HAL}/Src/stm32h5xx_hal_tim_ex.c
    ${HAL}/Src/stm32h5xx_hal_adc.c
    ${HAL}/Src/stm32h5xx_hal_adc_ex.c
    ${HAL}/Src/stm32h5xx_hal_fdcan.c
    ${HAL}/Src/stm32h5xx_hal_i2c.c
    ${HAL}/Src/stm32h5xx_hal_i2c_ex.c
    ${HAL}/Src/stm32h5xx_hal_iwdg.c
    ${HAL}/Src/stm32h5xx_hal_rtc.c
    ${HAL}/Src/stm32h5xx_hal_rtc_ex.c
    ${HAL}/Src/stm32h5xx_hal_spi.c
    ${HAL}/Src/stm32h5xx_hal_spi_ex.c
)

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc
    ${LL}/Core/Inc
    ${LL}
    ${HAL}/Inc
    ${HAL}/Inc/Legacy
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

# ── Suppress -Wunused-parameter in ST sources only ────────────────────────────
file(GLOB HAL_C_SOURCES "${HAL}/Src/*.c")
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
target_link_libraries(${PROJECT_NAME}.elf PRIVATE -lm)

# ── Post-build: size + hex ────────────────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND "${GCC_BIN}/arm-none-eabi-size.exe" $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMAND "${GCC_BIN}/arm-none-eabi-objcopy.exe"
            -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${PROJECT_NAME}.hex
    COMMENT "Size report and generating ${PROJECT_NAME}.hex"
)
