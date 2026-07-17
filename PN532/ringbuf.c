#include "ringbuf.h"
#include "stm32f10x.h"
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>

void RingbufInit(RingBufHnd_t *hnd, uint8_t *pbuf, uint32_t size)
{
    if (NULL == hnd || NULL == pbuf) {
        return;
    }
    hnd->pBuf     = pbuf;
    hnd->ReadPos  = 0x00;
    hnd->WritePos = 0x00;
    hnd->size     = size;
    hnd->cnt      = 0x00;
}

/**
 * @brief Clear all element in ringbuf
 *
 */
void RingbufClear(RingBufHnd_t *hnd)
{
    uint32_t primask;

    if (NULL == hnd) {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    hnd->ReadPos  = 0x00;
    hnd->WritePos = 0x00;
    hnd->cnt      = 0x00;
    __set_PRIMASK(primask);
}

bool IsRingBufferFull(RingBufHnd_t *hnd)
{
    if (NULL == hnd) {
        return false;
    }
    return hnd->cnt == hnd->size;
}

bool IsRingBufferEmpty(RingBufHnd_t *hnd)
{
    if (NULL == hnd) {
        return false;
    }
    return hnd->cnt == 0;
}

uint32_t RingBufGetDataCount(RingBufHnd_t *hnd)
{
    if (NULL == hnd) {
        return false;
    }
    return hnd->cnt;
}

void RingBufferQueue(RingBufHnd_t *hnd, uint8_t data)
{
    if (NULL == hnd) {
        return;
    }
    if (IsRingBufferFull(hnd)) {
        return;
    }
    hnd->pBuf[hnd->WritePos] = data;
    hnd->WritePos++;
    hnd->cnt++;
    if (hnd->WritePos > (hnd->size - 1)) {
        hnd->WritePos = 0x00;
    }
}

uint8_t *RingBuferGetReadPointer(RingBufHnd_t *hnd)
{
    if (NULL == hnd) {
        return NULL;
    }
    if (IsRingBufferFull(hnd)) {
        return NULL;
    }
    return (uint8_t *)&(hnd->pBuf[hnd->ReadPos]);
}

int16_t RingBufferDequeue(RingBufHnd_t *hnd)
{
    int16_t ret;
    uint32_t primask;

    if (NULL == hnd) {
        return -1;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    if (IsRingBufferEmpty(hnd)) {
        __set_PRIMASK(primask);
        return -1;
    }
    ret = hnd->pBuf[hnd->ReadPos];
    hnd->cnt--;
    hnd->ReadPos++;
    if (hnd->ReadPos > (hnd->size - 1)) {
        hnd->ReadPos = 0x00;
    }
    __set_PRIMASK(primask);
    return ret & 0x00ff;
}
