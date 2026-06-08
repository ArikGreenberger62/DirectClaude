/* ring_buf.h — Generic power-of-2 circular byte buffer (ISR-safe producer).
 *
 * Usage:
 *   RingBuf_Put  — called from ISR to store received bytes.
 *   RingBuf_Get  — called from main loop to drain bytes one at a time.
 *
 * Thread safety: single producer (ISR), single consumer (main loop).
 * No critical section needed on Cortex-M33 for 16-bit aligned head/tail.
 */
#ifndef RING_BUF_H
#define RING_BUF_H

#include <stdint.h>

#define RING_BUF_SIZE  512U   /* must be power of 2 */

typedef struct {
    uint8_t           data[RING_BUF_SIZE];
    volatile uint16_t head;   /* next write index  (modified by ISR) */
    volatile uint16_t tail;   /* next read index   (modified by main) */
} RingBuf_t;

void RingBuf_Init(RingBuf_t *rb);
void RingBuf_Put(RingBuf_t *rb, uint8_t b);       /* ISR-safe write */
int  RingBuf_Get(RingBuf_t *rb, uint8_t *b);      /* main-safe read; 1=got, 0=empty */
void RingBuf_Flush(RingBuf_t *rb);                /* discard all pending bytes */

#endif /* RING_BUF_H */
