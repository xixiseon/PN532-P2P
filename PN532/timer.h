#ifndef __timer_h__
#define __timer_h__

#include <stdint.h>

void SysTickDelayInit(void);
void SysTickDelayUs(uint32_t nus);
void SysTickDelayMs(uint16_t nms);
void SysTickDelaySec(uint16_t sec);

#endif
