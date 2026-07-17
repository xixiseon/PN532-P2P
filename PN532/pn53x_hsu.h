/*
 * @Author: AniuZen zhangwuji_work@163.com
 * @Date: 2024-02-03 10:17:32
 * @LastEditors: AniuZen zhangwuji_work@163.com
 * @LastEditTime: 2024-02-03 11:07:59
 * @FilePath: \stm32usart\pn532\pn53x_hsu.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __pn53x_hsu_h__
#define __pn53x_hsu_h__
#include "pn53xInterface.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    PN53x_Interface super;
    uint32_t        baudrate;

} Hsu_Interface;

void    Hsu_Interface_ctor(Hsu_Interface *const me, uint32_t cnt, uint32_t bps);
int     HsuWriteCommand(uint8_t *pBuf, uint32_t wLen);
void    HsuHostSendAck(void);
int16_t HsuReadResponse(uint8_t command, uint8_t *pbuf, uint32_t wlen,
                        uint32_t timeoutMs);
void    PN53xUartSetBaudrate(uint32_t baudrate);
bool    HsuWakeUp(void);
void    Pn53xUartInit(void);
void    Pn53xHsuClearReceive(void);
void    Pn53xTask(void);
void    Pn53xUartSend(uint8_t *send_data, uint16_t len);

#endif
