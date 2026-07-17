#ifndef __ringbuf_h__
#define __ringbuf_h__

#include <stdint.h>
#include <stdbool.h>
typedef struct {
    uint8_t *pBuf;
    uint32_t WritePos;
    uint32_t ReadPos;
    uint32_t size;
    uint32_t cnt;
} RingBuf_t;

typedef RingBuf_t RingBufHnd_t;

void     RingbufInit(RingBufHnd_t *hnd, uint8_t *pbuf, uint32_t size);
void     RingBufferQueue(RingBufHnd_t *hnd, uint8_t data);
int16_t  RingBufferDequeue(RingBufHnd_t *hnd);
bool     IsRingBufferEmpty(RingBufHnd_t *hnd);
void     RingbufClear(RingBufHnd_t *hnd);
uint32_t RingBufGetDataCount(RingBufHnd_t *hnd);
uint8_t *RingBuferGetReadPointer(RingBufHnd_t *hnd);

#endif
