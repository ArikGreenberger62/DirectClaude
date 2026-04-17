---
name: embedded-general/coding
description: General embedded C coding best practices. Read when writing any firmware C code.
---

# Embedded C Coding Practices

## Types — always use stdint.h
```c
#include <stdint.h>
uint8_t / int8_t    // 8-bit
uint16_t / int16_t  // 16-bit
uint32_t / int32_t  // 32-bit
```

## Memory
- Always initialise variables before use
- `const` for read-only data (saves RAM, goes to flash)
- `static` for file-scoped variables
- Prefer stack over heap; never use `malloc`/`free`
- No recursion (stack overflow risk on small MCUs)

## Hardware Registers
```c
volatile uint32_t *reg = (volatile uint32_t *)0x40000000;
// Set bit:   reg |=  (1 << N);
// Clear bit: reg &= ~(1 << N);
// Toggle:    reg ^=  (1 << N);
```

## Avoid
- `float` arithmetic (slow without FPU; prefer fixed-point)
- Reimplementing what HAL/libraries already provide
- Long functions — if it doesn't fit on screen, split it

## Debug Trace (mandatory in every project)
- One UART as dedicated trace port (blocking TX, no DMA)
- Provide `Trace_Print(const char *msg, uint16_t len)` usable from anywhere
- Every major init step: `[MODULE] description: PASS` or `FAIL - reason`
- Print key register values after peripheral init
- 1 Hz runtime status with tag prefix (`RAW`, `CAL`, `MOVE`, etc.)
- Temporary debug prints tagged `[DBG]`; remove when issue is resolved
