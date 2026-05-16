/* ring_buf.c — Circular byte buffer implementation.
 *
 * ISR writes via RingBuf_Put; main loop reads via RingBuf_Get.
 * No mutex needed: single producer, single consumer, 16-bit aligned indices.
 */
#include "ring_buf.h"

void RingBuf_Init(RingBuf_t *rb)
{
    rb->head = 0U;
    rb->tail = 0U;
}

void RingBuf_Put(RingBuf_t *rb, uint8_t b)
{
    uint16_t next = (uint16_t)((rb->head + 1U) % RING_BUF_SIZE);
    if (next != rb->tail) {    /* buffer not full — drop on overflow */
        rb->data[rb->head] = b;
        rb->head = next;
    }
}

int RingBuf_Get(RingBuf_t *rb, uint8_t *b)
{
    if (rb->tail == rb->head) { return 0; }   /* empty */
    *b = rb->data[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1U) % RING_BUF_SIZE);
    return 1;
}

void RingBuf_Flush(RingBuf_t *rb)
{
    rb->tail = rb->head;
}
