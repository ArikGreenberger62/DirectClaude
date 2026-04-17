cmake_minimum_required(VERSION 3.22)

project(threadx_blink
    VERSION 1.0
    DESCRIPTION "ThreadX blink — LED_R via tx_timer, LED_G via dedicated thread"
    LANGUAGES C ASM
)

# ── STM32Cube FW + ThreadX middleware (installed on this machine) ────────────
set(CUBE_H5_ROOT "C:/Users/arikg/STM32Cube/Repository/STM32Cube_FW_H5_V1.6.0")
set(HAL_INC   "${CUBE_H5_ROOT}/Drivers/STM32H5xx_HAL_Driver/Inc")
set(HAL_SRC   "${CUBE_H5_ROOT}/Drivers/STM32H5xx_HAL_Driver/Src")
set(CMSIS_INC "${CUBE_H5_ROOT}/Drivers/CMSIS/Include")
set(CMSIS_DEV "${CUBE_H5_ROOT}/Drivers/CMSIS/Device/ST/STM32H5xx/Include")
set(CMSIS_TMPL "${CUBE_H5_ROOT}/Drivers/CMSIS/Device/ST/STM32H5xx/Source/Templates")

set(TX_ROOT   "${CUBE_H5_ROOT}/Middlewares/ST/threadx")
set(TX_COMMON "${TX_ROOT}/common")
set(TX_PORT   "${TX_ROOT}/ports/cortex_m33/gnu")

# ── ThreadX common sources (185 files) ───────────────────────────────────────
file(GLOB TX_COMMON_SOURCES CONFIGURE_DEPENDS "${TX_COMMON}/src/*.c")

# ── ThreadX Cortex-M33 port sources ──────────────────────────────────────────
# Project's own tx_initialize_low_level.S lives in Core/Src (SYSTEM_CLOCK patched)
# and MUST replace the port's version — otherwise the default 250 MHz reload
# gives a wrong tick rate.
set(TX_PORT_SOURCES
    ${TX_PORT}/src/tx_thread_context_restore.S
    ${TX_PORT}/src/tx_thread_context_save.S
    ${TX_PORT}/src/tx_thread_interrupt_control.S
    ${TX_PORT}/src/tx_thread_interrupt_disable.S
    ${TX_PORT}/src/tx_thread_interrupt_restore.S
    ${TX_PORT}/src/tx_thread_schedule.S
    ${TX_PORT}/src/tx_thread_stack_build.S
    ${TX_PORT}/src/tx_thread_system_return.S
    ${TX_PORT}/src/tx_timer_interrupt.S
)

add_executable(${PROJECT_NAME}.elf)

target_sources(${PROJECT_NAME}.elf PRIVATE
    Core/Src/main.c
    Core/Src/app_threadx.c
    Core/Src/stm32h5xx_hal_msp.c
    Core/Src/tx_initialize_low_level.S

    ${CMSIS_TMPL}/system_stm32h5xx.c
    ${CMSIS_TMPL}/gcc/startup_stm32h573xx.s

    ${HAL_SRC}/stm32h5xx_hal.c
    ${HAL_SRC}/stm32h5xx_hal_cortex.c
    ${HAL_SRC}/stm32h5xx_hal_gpio.c
    ${HAL_SRC}/stm32h5xx_hal_pwr.c
    ${HAL_SRC}/stm32h5xx_hal_pwr_ex.c
    ${HAL_SRC}/stm32h5xx_hal_rcc.c
    ${HAL_SRC}/stm32h5xx_hal_rcc_ex.c
    ${HAL_SRC}/stm32h5xx_hal_flash.c
    ${HAL_SRC}/stm32h5xx_hal_flash_ex.c
    ${HAL_SRC}/stm32h5xx_hal_uart.c
    ${HAL_SRC}/stm32h5xx_hal_uart_ex.c
    ${HAL_SRC}/stm32h5xx_hal_dma.c
    ${HAL_SRC}/stm32h5xx_hal_dma_ex.c

    ${TX_COMMON_SOURCES}
    ${TX_PORT_SOURCES}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc
    ${HAL_INC}
    ${HAL_INC}/Legacy
    ${CMSIS_INC}
    ${CMSIS_DEV}
    ${TX_COMMON}/inc
    ${TX_PORT}/inc
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    STM32H573xx
    USE_HAL_DRIVER
    TX_INCLUDE_USER_DEFINE_FILE     # pulls in Core/Inc/tx_user.h
    TX_SINGLE_MODE_NON_SECURE       # no TrustZone → strip secure-stack glue
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    -Wall
    -Wextra
    -Werror
    -fdata-sections
    -ffunction-sections
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2>
)

# ── Suppress -Wunused-parameter in ST sources we cannot modify ──────────────
file(GLOB HAL_C_SOURCES "${HAL_SRC}/*.c")
set_source_files_properties(
    ${HAL_C_SOURCES}
    ${TX_COMMON_SOURCES}
    PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter"
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/STM32H573VITXQ_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--print-memory-usage
    --specs=nosys.specs
)

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
