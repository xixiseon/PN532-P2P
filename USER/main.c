/*
 * @Author: AniuZen zhangwuji_work@163.com
 * @Date: 2024-02-18 10:16:04
 * @LastEditors: AniuZen zhangwuji_work@163.com
 * @LastEditTime: 2024-02-20 13:20:47
 * @FilePath: \P2PTarget\USER\main.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "stm32f10x.h"
#include "pn53x.h"
#include "timer.h"
#include "SEGGER_RTT.h"
extern nfc UartNfc;

uint8_t tx[50];
uint8_t rx[50];
uint8_t rxLen;

void P2pTargetFsm(void);
void P2pInitiatorFsm(void);

#define LED_INFO_PORT GPIOC
#define LED_INFO_PIN  GPIO_Pin_2
#define LED_INFO_TROGGLE() \
    do { \
        LED_INFO_PORT->ODR &= ~LED_INFO_PIN; \
    } while (0)

void LedInit()
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = LED_INFO_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(LED_INFO_PORT, &GPIO_InitStructure);
}

int main(void)
{
    SEGGER_RTT_Init();
    SysTickDelayInit();
    PN53xInit();
    Pn53xWakeUp();
    for (uint32_t i = 0x00; i < 50; i++) {
        tx[i] = i;
    }
    int status = SAMConfig(&UartNfc, 0X01);
    if (status == 0x00) {
        // DiagnoseNfcIc();
        SEGGER_RTT_printf(0, "NFC P2P  demo!\n");
    }
    while (1) {
#if defined(P2P_TARGET_MODE)
        P2pTargetFsm();
#elif defined(P2P_INITIATOR_MODE)
        P2pInitiatorFsm();
#else
#error "Select the Initiator or Target Keil target"
#endif
    }
}

void printfBuffer(uint8_t *buffer, uint8_t len)
{
    for (uint32_t i = 0x00; i < len; i++) {
        SEGGER_RTT_printf(0, "%02x ", buffer[i]);
    }
    SEGGER_RTT_printf(0, "\n");
}

#define RESET_P2P_TARGET_FSM() \
    do { \
        State = START; \
    } while (0)
void P2pTargetFsm(void)
{
    static enum {
        START = 0,
        WAIT_TARGET,
        TRANS,
    } State = START;
    uint8_t status;

    switch (State) {
    case START:
        rxLen = 0x00;
        State = WAIT_TARGET;
        break;
    case WAIT_TARGET:
        status = P2PTargetInit(&UartNfc);
        if (status == NFC_SUCCESS) {
            SEGGER_RTT_printf(0, "[P2P]:find initiator!\n");
            State = TRANS;
        }
        break;
    case TRANS:
        if (NFC_SUCCESS == P2PTargetTxRx(&UartNfc, tx, sizeof(tx), rx, &rxLen)) {
            if (rxLen) {
                SEGGER_RTT_printf(0, "[P2P]:get initiator message:\n");
                LED_INFO_TROGGLE();
                printfBuffer(rx, rxLen);
            }
        }
        //复位状态机，开始下一次传输
        RESET_P2P_TARGET_FSM();
        break;
    }
}

//不停的发起p2p请求
#define RESET_P2P_INITIATOR_FSM() \
    do { \
        State = START; \
    } while (0)
void P2pInitiatorFsm(void)
{
    static enum {
        START = 0,
        WAIT_TARGET,
        TRANS,
    } State = START;
    uint8_t status;
    switch (State) {
    case START:
        rxLen = 0x00;
        State = WAIT_TARGET;
        break;
    case WAIT_TARGET:
        status = P2PInitiatorInit(&UartNfc);
        if (status == NFC_SUCCESS) {
            //获取了target
            SEGGER_RTT_printf(0, "[P2P]:find targert!\n");
            State = TRANS;
        }
        break;
    case TRANS:
        if (NFC_SUCCESS == P2PInitiatorTxRx(&UartNfc, tx, sizeof(tx), rx, &rxLen)) {
            SEGGER_RTT_printf(0, "[P2P]:send tx message done!\n");
            if (rxLen) {
                LED_INFO_TROGGLE();
                SEGGER_RTT_printf(0, "[P2P]:get target message:\n");
                printfBuffer(rx, rxLen);
            }
        }
        RESET_P2P_INITIATOR_FSM();
        break;
    }
}
