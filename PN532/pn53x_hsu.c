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
static bool         FrameReceiveDone = false;

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
    USART_ITConfig(PN53X_UART, USART_IT_RXNE, ENABLE);
    USART_ITConfig(PN53X_UART, USART_IT_IDLE, ENABLE);
    RingbufInit(&Pn532RingBufHnd, usart_rx_buffer, PN53X_BUFF_SIZE);
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

/**
 * @brief
 *
 * @param recvBuf  pointer to recv buf.
 * @param len data len want to recv.
 * @param timeout wait time.
 */
int Pn53xUartReceive(uint8_t *recvBuf, uint16_t len, uint32_t timeout)
{
    /* wait receive finish*/
    uint32_t volatile tick = timeout;
    uint32_t totalRcvLen   = 0x00;
    while (1) {
        totalRcvLen = RingBufGetDataCount(&Pn532RingBufHnd);
        if (totalRcvLen >= len && FrameReceiveDone) {
            FrameReceiveDone = false;
            break;
        }
        tick--;
        if (tick <= 0) {
            /*Timeout.maybe some useless data in the ringbuf, clear it*/
            if (totalRcvLen > 0) {
                RingbufClear(&Pn532RingBufHnd);
            }
            return NFC_ETIMEOUT;
        }
        SysTickDelayUs(10);
    }

    /*we must check and get the newest len data.*/
    if (totalRcvLen > len) {
        /*flush out the old data.*/
        for (uint32_t i = 0; i < (totalRcvLen - len); i++) {
            RingBufferDequeue(&Pn532RingBufHnd);
        }
    }
    for (uint32_t i = 0; i < len; i++) {
        *recvBuf++ = RingBufferDequeue(&Pn532RingBufHnd);
    }
    return NFC_SUCCESS;
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

static int HsuReadAckFrame()
{
    uint8_t PN532_ACK[]        = {0, 0, 0xFF, 0, 0xFF, 0};
    uint8_t ackBuf[6]          = {0x00};
    uint32_t volatile TimeTick = PN532_ACK_WAIT_TIME;
    int ret                    = 0x00;

    if ((ret = Pn53xUartReceive(ackBuf, 6, PN532_ACK_WAIT_TIME)) < 0) {
        return ret;
    }

    if (0x00 == memcmp(PN532_ACK, ackBuf, sizeof(PN532_ACK))) {
        return NFC_SUCCESS;
    } else {
        return NFC_EIO;
    }
}

int16_t HsuReadResponse(uint8_t command, uint8_t *pbuf, uint32_t wlen,
                        uint32_t timeoutMs)
{
    uint32_t volatile Timeout = timeoutMs * 5;
    uint32_t totalRcvLen      = 0x00;
    int16_t  result           = NFC_SUCCESS;
    bool     bFindFrameHead   = false;
    /* wait  for reveive  finish message.*/
    while (1) {
        totalRcvLen = RingBufGetDataCount(&Pn532RingBufHnd);
        if (totalRcvLen > PN532_MIN_FRAME_SIZE && FrameReceiveDone) {
            FrameReceiveDone = false;
            break;
        }
        Timeout--;
        if (Timeout <= 0) {
            if (totalRcvLen > 0) {
                RingbufClear(&Pn532RingBufHnd);
            }

            return NFC_ETIMEOUT;
        }
        SysTickDelayUs(40);
    }

    for (uint32_t i = 0x00; i < totalRcvLen; i++) {
        if (0x00 == RingBufferDequeue(&Pn532RingBufHnd)) {
            if (0x00 == RingBufferDequeue(&Pn532RingBufHnd)) {
                if (0xFF == RingBufferDequeue(&Pn532RingBufHnd)) {
                    bFindFrameHead = true;
                    break;
                }
            }
        }
    }

    if (!bFindFrameHead) {
        result = NFC_EINVRESPOND;
        goto end;
    }

    do {
        uint8_t lenth = RingBufferDequeue(&Pn532RingBufHnd); // len
        uint8_t lcs   = RingBufferDequeue(&Pn532RingBufHnd);
        if (0 != (uint8_t)(lenth + lcs)) {
            result = NFC_EINVRESPOND; // lcs error
            goto end;
        }
        uint8_t cmd = command + 1;                                      // response command
        if (PN532_PN532TOHOST != RingBufferDequeue(&Pn532RingBufHnd) || // TFI
            (cmd) != RingBufferDequeue(&Pn532RingBufHnd)) {             // D0 = CMD
            result = NFC_EINVRESPOND;
            goto end;
        }
        lenth = lenth - 2;

        uint8_t Remain = RingBufGetDataCount(&Pn532RingBufHnd);
        /* Ringbuf Data length check*/
        if ((lenth > wlen) || ((2 + lenth) != Remain)) {
            result = NFC_EINVRESPOND; // no space for data

            goto end;
        }
        uint8_t sum = PN532_PN532TOHOST + cmd;

        for (uint8_t i = 0; i < lenth; i++) {
            pbuf[i] = RingBufferDequeue(&Pn532RingBufHnd);
            sum += pbuf[i];
        }
        uint8_t checksum = RingBufferDequeue(&Pn532RingBufHnd);
        if (0 != (uint8_t)(sum + checksum)) {
            result = NFC_EINVRESPOND; // data checksum error

            goto end;
        }
        uint8_t dmuuy = RingBufferDequeue(&Pn532RingBufHnd); // POSTAMBLE
        result        = lenth;
    } while (0);

end:
    return result;
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
