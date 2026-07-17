/**
 * @file pn53x.c
 * @author zhangwuji (zhangwuji_work@163.com)
 * @brief pn53x communction function impletement.
 * @version 0.1
 * @date 2022-04-21
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "pn53x_hsu.h"
#include "Pn53xInterface.h"
#include "pn53x.h"
#include "ringbuf.h"
#include "timer.h"
#include <string.h>
#include "stm32f10x.h"

#define PN53X_USART_IRQHandler USART2_IRQHandler
#define PN53X_BUFF_SIZE        256
#define PN53X_UART             USART2
#define PN53X_UART_IRQ         USART2_IRQn
#define PN53X_UART_PORT        GPIOA
#define PN53X_UART_TX_PIN      GPIO_Pin_2
#define PN53X_UART_RX_PIN      GPIO_Pin_3

static RingBufHnd_t Pn532RingBufHnd;
uint8_t             usart_rx_buffer[PN53X_BUFF_SIZE];
static volatile bool FrameReceiveDone = false;

static bool HsuWaitForData(uint32_t count, uint32_t *timeout,
                           uint32_t delayUs)
{
    while (RingBufGetDataCount(&Pn532RingBufHnd) < count) {
        if (*timeout == 0) {
            return false;
        }
        (*timeout)--;
        SysTickDelayUs(delayUs);
    }
    return true;
}

static void HsuClearReceive(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    RingbufClear(&Pn532RingBufHnd);
    FrameReceiveDone = false;
    __set_PRIMASK(primask);
}

void Hsu_Interface_ctor(Hsu_Interface *const me, uint32_t cnt, uint32_t bps)
{
    static const struct InterfaceVtable vtable = {
        &HsuReadResponse,
        &HsuWriteCommand,
        &HsuWakeUp,
        &HsuHostSendAck,
    };
    PN53x_Interface_ctor(&(me->super), cnt);
    me->baudrate   = bps;
    me->super.vptr = &vtable;
    /*硬件串口初始化*/
    Pn53xUartInit();
}

/**
 * @brief  this function handles usart2 gpio config.
 * @param  none
 * @retval none
 */
static void Pn53xUartConfig(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel                   = PN53X_UART_IRQ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Init(PN53X_UART, &USART_InitStructure);
    USART_Cmd(PN53X_UART, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = PN53X_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(PN53X_UART_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = PN53X_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(PN53X_UART_PORT, &GPIO_InitStructure);
}

void PN53xUartSetBaudrate(uint32_t baudrate)
{
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate            = baudrate;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Cmd(PN53X_UART, DISABLE);
    USART_Init(PN53X_UART, &USART_InitStructure);
    USART_Cmd(PN53X_UART, ENABLE);
}

/**
 * @brief use usart1 as pn53x port.
 *
 */
void Pn53xUartInit(void)
{
    /* enable the usart2 and gpio clock */
    Pn53xUartConfig();
    RingbufInit(&Pn532RingBufHnd, usart_rx_buffer, PN53X_BUFF_SIZE);
    USART_ITConfig(PN53X_UART, USART_IT_RXNE, ENABLE);
    USART_ITConfig(PN53X_UART, USART_IT_IDLE, ENABLE);
}

void Pn53xUartSend(uint8_t *send_data, uint16_t len)
{
    uint16_t index = 0;
    for (index = 0; index < len; index++) {
        do {
            ;
        } while (USART_GetFlagStatus(PN53X_UART, USART_FLAG_TXE) == RESET);
        USART_SendData(PN53X_UART, send_data[index]);
    }
}

/**
 * @brief  this function handles usart2 handler.
 * @param  none
 * @retval none
 */
void PN53X_USART_IRQHandler(void)
{
    if (USART_GetITStatus(PN53X_UART, USART_IT_RXNE) != RESET) {
        USART_ClearITPendingBit(PN53X_UART, USART_IT_RXNE);

        RingBufferQueue(&Pn532RingBufHnd, USART_ReceiveData(PN53X_UART));
    }
    if (USART_GetITStatus(PN53X_UART, USART_IT_IDLE)) {
        /*一帧数据接收完成了*/
        USART_ReceiveData(PN53X_UART);
        FrameReceiveDone = true;
    }
}

bool HsuWakeUp(void)
{
    uint8_t wakeupbuf[25] = {
        0x55,
        0x55,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };
    Pn53xUartSend(wakeupbuf, sizeof(wakeupbuf));
    return true;
}

static int HsuReadAckFrame(void)
{
    static const uint8_t ack[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    static const uint8_t prefix[] = {0, 1, 0, 1, 0, 1};
    uint32_t timeout = PN532_ACK_WAIT_TIME;
    uint8_t  matched = 0;
    uint8_t  value;

    while (matched < sizeof(ack)) {
        if (!HsuWaitForData(1, &timeout, 10)) {
            HsuClearReceive();
            return NFC_ETIMEOUT;
        }
        value = RingBufferDequeue(&Pn532RingBufHnd);
        while (matched > 0 && value != ack[matched]) {
            matched = prefix[matched - 1];
        }
        if (value == ack[matched]) {
            matched++;
        }
    }
    return NFC_SUCCESS;
}

int16_t HsuReadResponse(uint8_t command, uint8_t *pbuf, uint32_t wlen,
                        uint32_t timeoutMs)
{
    static const uint8_t frameHead[] = {0x00, 0x00, 0xFF};
    uint32_t timeout = timeoutMs * 5;
    uint32_t dataLength;
    uint32_t i;
    uint8_t  headIndex = 0;
    uint8_t  length;
    uint8_t  lcs;
    uint8_t  value;
    uint8_t  sum = 0;
    uint8_t  checksum;
    uint8_t  postamble;
    int16_t  result = NFC_SUCCESS;

    while (headIndex < sizeof(frameHead)) {
        if (!HsuWaitForData(1, &timeout, 40)) {
            HsuClearReceive();
            return NFC_ETIMEOUT;
        }
        value = RingBufferDequeue(&Pn532RingBufHnd);
        if (value == frameHead[headIndex]) {
            headIndex++;
        } else if (value == 0x00) {
            headIndex = (headIndex == 2) ? 2 : 1;
        } else {
            headIndex = 0;
        }
    }

    if (!HsuWaitForData(2, &timeout, 40)) {
        HsuClearReceive();
        return NFC_ETIMEOUT;
    }
    length = RingBufferDequeue(&Pn532RingBufHnd);
    lcs    = RingBufferDequeue(&Pn532RingBufHnd);
    if (length < 2 || (uint8_t)(length + lcs) != 0) {
        HsuClearReceive();
        return NFC_EINVRESPOND;
    }

    if (!HsuWaitForData((uint32_t)length + 2, &timeout, 40)) {
        HsuClearReceive();
        return NFC_ETIMEOUT;
    }

    dataLength = length - 2;
    if (dataLength > wlen) {
        result = NFC_EOVFLOW;
    } else if (dataLength > 0 && pbuf == NULL) {
        result = NFC_EINVARG;
    }

    for (i = 0; i < length; i++) {
        value = RingBufferDequeue(&Pn532RingBufHnd);
        sum += value;
        if (i == 0 && value != PN532_PN532TOHOST) {
            result = NFC_EINVRESPOND;
        } else if (i == 1 && value != (uint8_t)(command + 1)) {
            result = NFC_EINVRESPOND;
        } else if (i >= 2 && dataLength <= wlen && pbuf != NULL) {
            pbuf[i - 2] = value;
        }
    }

    checksum = RingBufferDequeue(&Pn532RingBufHnd);
    postamble = RingBufferDequeue(&Pn532RingBufHnd);
    if ((uint8_t)(sum + checksum) != 0 || postamble != PN532_POSTAMBLE) {
        return NFC_EINVRESPOND;
    }
    if (result < 0) {
        return result;
    }
    return dataLength;
}

int HsuWriteCommand(uint8_t *pBuf, uint32_t wLen)
{
    if (NULL == pBuf || wLen <= 0) {
        return NFC_EINVARG;
    }
    Pn53xUartSend(pBuf, wLen);
    return HsuReadAckFrame();
}

void HsuHostSendAck(void)
{
    uint8_t ack[6] = {0, 0, 0xFF, 0, 0xFF, 0};
    Pn53xUartSend(ack, 6);
}
