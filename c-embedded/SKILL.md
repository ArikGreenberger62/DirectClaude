---
name: c-embedded
description: C programming best practices for embedded systems. Use when creating firmware, working with microcontrollers, or writing embedded C code.
---

# C Embedded Programming Best Practices

## Memory Management
- Always initialize variables
- Use `const` for read-only data to save RAM
- Prefer stack allocation over dynamic allocation
- Use `static` for file-scoped variables

## Integer Types
Use exact-width types from `<stdint.h>`:
- `uint8_t`, `int8_t` for 8-bit values
- `uint16_t`, `int16_t` for 16-bit values
- `uint32_t`, `int32_t` for 32-bit values

## GPIO Best Practices
- Use bit-shift operations: `(1 << PIN_NUMBER)`
- Mask operations: `register |= (1 << bit)` to set
- Clear bits: `register &= ~(1 << bit)`

## Common Patterns

### Delay Functions
```c
void delay_ms(uint32_t ms) {
    // Use hardware timer, not busy loops
}
```

### Pin Toggle
```c
GPIO_PORT ^= (1 << PIN);  // Toggle
```

### Volatile for Hardware Registers
```c
volatile uint32_t *reg = (volatile uint32_t *)0x40000000;
```

## STM32 HAL Usage

- Always prefer `HAL_*` functions over bare-register access.
- Exception: use direct register access only when HAL overhead is proven to be a bottleneck (measure first).
- Include path for HAL headers: `C:/Users/arikg/STM32Cube/Repository/STM32Cube_FW_H5_V1.6.0/Drivers/STM32H5xx_HAL_Driver/Inc/`
- Include path for CMSIS: `C:/Users/arikg/STM32Cube/Repository/STM32Cube_FW_H5_V1.6.0/Drivers/CMSIS/`
- Standard include pattern:
```c
#include "stm32h5xx_hal.h"   // pulls in all HAL modules
```

## Avoid
- `float` operations (slow on most MCUs)
- `malloc`/`free` (fragmentation risk)
- Recursion (stack overflow risk)
- Reimplementing what HAL already provides
- Copying HAL/CMSIS files into the project — use include paths to the repository

## Debug Trace & Closed-Loop Testing

Every project must include a UART-based trace channel that allows the AI agent to observe
firmware behaviour in real time and close the loop on testing.

### Trace output rules
- Designate one UART as the **debug trace port** (typically UART7 on this board, 115200 8N1).
- Provide a `Trace_Print(const char *msg, uint16_t len)` helper (or equivalent) usable from anywhere.
- Every major init step must print a tagged status line:
  `[MODULE] description: PASS` or `[MODULE] description: FAIL - reason`
- After init, print key register values so SPI/I2C/clock configuration can be verified without a debugger.
- During runtime, print periodic status at a human-readable rate (1 Hz typical) with a tag prefix
  (e.g. `RAW`, `CAL`, `MOVE`) so output can be parsed programmatically.

### Closed-loop testing workflow
- The AI agent reads the trace port (e.g. via `pyserial` on the host) after flashing.
- It compares actual output against expected patterns to confirm correct behaviour.
- If output does not match expectations, the agent diagnoses the issue from the trace,
  applies a fix, rebuilds, reflashes, and re-reads — repeating until the feature works.
- Diagnostic prints (tagged `[DBG]`) may be added temporarily during debugging and
  removed once the issue is resolved.
- The trace port must remain available (not locked by another tool) so the agent can
  open and close it as needed during iterative build-flash-test cycles.
