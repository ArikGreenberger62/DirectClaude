# Shared ARM Cortex-M33 toolchain file
# Used by all DirectClaude projects via:
#   cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# ── Toolchain paths (STM32CubeIDE 1.19.0 bundled GCC 14.3.rel1) ──────────────
set(GCC_ROOT
    "C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin"
)

set(CMAKE_C_COMPILER   "${GCC_ROOT}/arm-none-eabi-gcc.exe")
set(CMAKE_CXX_COMPILER "${GCC_ROOT}/arm-none-eabi-g++.exe")
set(CMAKE_ASM_COMPILER "${GCC_ROOT}/arm-none-eabi-gcc.exe")
set(CMAKE_OBJCOPY      "${GCC_ROOT}/arm-none-eabi-objcopy.exe")
set(CMAKE_SIZE         "${GCC_ROOT}/arm-none-eabi-size.exe")

# ── CPU flags for STM32H573 (Cortex-M33, FPU, no TrustZone) ──────────────────
set(CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS} -specs=nano.specs")

# ── Prevent CMake from testing the compiler (cross-compile) ──────────────────
set(CMAKE_C_COMPILER_WORKS   1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ── HAL and CMSIS paths (STM32Cube FW_H5 V1.6.0) ─────────────────────────────
set(STM32CUBE_H5_ROOT "C:/Users/arikg/STM32Cube/Repository/STM32Cube_FW_H5_V1.6.0")
set(HAL_INC   "${STM32CUBE_H5_ROOT}/Drivers/STM32H5xx_HAL_Driver/Inc")
set(HAL_SRC   "${STM32CUBE_H5_ROOT}/Drivers/STM32H5xx_HAL_Driver/Src")
set(CMSIS_INC "${STM32CUBE_H5_ROOT}/Drivers/CMSIS/Include")
set(CMSIS_DEV "${STM32CUBE_H5_ROOT}/Drivers/CMSIS/Device/ST/STM32H5xx/Include")
