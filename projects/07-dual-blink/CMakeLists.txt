cmake_minimum_required(VERSION 3.22)

project(dual_blink
    VERSION 1.0
    DESCRIPTION "07-dual-blink: RED 1Hz 30% duty + GREEN 2x50% in RED-off window"
    LANGUAGES C ASM
)

# ── LowLevel folder (CubeMX-generated sources, Drivers, linker script) ────────
set(LL "${CMAKE_SOURCE_DIR}/../../LowLevel")

# ── GCC toolchain bin path ────────────────────────────────────────────────────
set(GCC_BIN
    "C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin"
)

# ── Target executable ─────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf)

# ── Application sources ───────────────────────────────────────────────────────
# gpio.c and stm32h5xx_it.c are project overrides of their LowLevel counterparts
# (M1 pattern: copy → edit → list here → remove from LL sources block below).
target_sources(${PROJECT_NAME}.elf PRIVATE
    Core/Src/main.c
    Core/Src/gpio.c          # overrides ${LL}/Core/Src/gpio.c — LED + P3V3_SW_EN only
    Core/Src/stm32h5xx_it.c  # overrides ${LL}/Core/Src/stm32h5xx_it.c + __errno stub
)

# ── LowLevel CubeMX sources (gpio.c and stm32h5xx_it.c excluded) ─────────────
target_sources(${PROJECT_NAME}.elf PRIVATE
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

# ── HAL sources (explicit list matching project 04 canonical reference) ──────
set(HAL "${LL}/Drivers/STM32H5xx_HAL_Driver")
set(HAL_C_SOURCES
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
    ${HAL}/Src/stm32h5xx_hal_spi.c
    ${HAL}/Src/stm32h5xx_hal_spi_ex.c
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
)
target_sources(${PROJECT_NAME}.elf PRIVATE ${HAL_C_SOURCES})

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc                                            # project main.h
    ${LL}/Core/Inc                                      # gpio.h, usart.h, hal_conf.h …
    ${LL}                                               # stm32_lock.h
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

# -lm: required when nano.specs pulls in libm; __errno stub in stm32h5xx_it.c.
target_link_libraries(${PROJECT_NAME}.elf PRIVATE -lm)

# ── Post-build: size report + hex ─────────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND "${GCC_BIN}/arm-none-eabi-size.exe" $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMAND "${GCC_BIN}/arm-none-eabi-objcopy.exe"
            -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${PROJECT_NAME}.hex
    COMMENT "Size report and generating ${PROJECT_NAME}.hex"
)
