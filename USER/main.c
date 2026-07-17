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

#define P2P_LINK_POLICY_AUTO                  0
#define P2P_LINK_POLICY_FORCE_PASSIVE         1
#define P2P_LINK_POLICY_FORCE_ACTIVE          2
#define P2P_LINK_POLICY_PASSIVE_RETRY_RECOVER 3

#ifndef P2P_LINK_POLICY
#define P2P_LINK_POLICY P2P_LINK_POLICY_FORCE_PASSIVE
#endif

#if P2P_LINK_POLICY < P2P_LINK_POLICY_AUTO || \
    P2P_LINK_POLICY > P2P_LINK_POLICY_PASSIVE_RETRY_RECOVER
#error "Invalid P2P_LINK_POLICY"
#endif

#define LED_INFO_PORT                  GPIOC
#define LED_INFO_PIN                   GPIO_Pin_2
#define P2P_ACTIVATION_WAIT_LIMIT      8
#define P2P_STATS_PRINT_INTERVAL       100
#define PN532_ACTIVATED_FRAMING_MASK   0x03
#define PN532_ACTIVATED_FRAMING_ACTIVE 0x01
#define LED_INFO_TROGGLE() \
    do { \
        LED_INFO_PORT->ODR &= ~LED_INFO_PIN; \
    } while (0)

typedef struct {
    uint32_t session_count;
    uint32_t complete_session_ok;
    uint32_t passive_activate_ok;
    uint32_t passive_activate_fail;
    uint32_t passive_exchange_ok;
    uint32_t passive_exchange_fail;
    uint32_t active_activate_ok;
    uint32_t active_activate_fail;
    uint32_t active_exchange_ok;
    uint32_t active_exchange_fail;
    uint32_t abort_count;
    uint32_t release_ok;
    uint32_t release_fail;
    uint32_t raw_status_01;
    uint32_t raw_status_0A;
    uint32_t raw_status_25;
    uint32_t raw_status_29;
    uint32_t host_timeout;
    uint32_t host_invalid_response;
    uint32_t host_io_error;
    uint32_t auto_passive_direct_ok;
    uint32_t auto_passive_active_ok;
    uint32_t auto_passive_active_fail;
} P2PTestStats;

static P2PTestStats P2pStats;
static uint32_t     sessionId;
static uint32_t     lastStatsSession;

static const char *P2PPolicyName(void)
{
#if P2P_LINK_POLICY == P2P_LINK_POLICY_FORCE_PASSIVE
    return "force_passive";
#elif P2P_LINK_POLICY == P2P_LINK_POLICY_FORCE_ACTIVE
    return "force_active";
#elif P2P_LINK_POLICY == P2P_LINK_POLICY_PASSIVE_RETRY_RECOVER
    return "passive_retry_recover";
#else
    return "auto";
#endif
}

static const char *P2PModeName(uint8_t mode)
{
    return mode == P2P_ACTIVE_MODE ? "active" : "passive";
}

static bool P2PNeedsAbort(int status)
{
    return UartNfc.last_error == 0 &&
           (status == NFC_ETIMEOUT || status == NFC_EINVRESPOND ||
            status == NFC_EIO);
}

static void P2PRecordError(int status)
{
    switch (UartNfc.last_error & 0x3F) {
    case ETIMEOUT:
        P2pStats.raw_status_01++;
        break;
    case ERFTIMEOUT:
        P2pStats.raw_status_0A++;
        break;
    case EDEPINVSTATE:
        P2pStats.raw_status_25++;
        break;
    case ETGREL:
        P2pStats.raw_status_29++;
        break;
    default:
        if (status == NFC_ETIMEOUT) {
            P2pStats.host_timeout++;
        } else if (status == NFC_EINVRESPOND) {
            P2pStats.host_invalid_response++;
        } else if (status == NFC_EIO) {
            P2pStats.host_io_error++;
        }
        break;
    }
}

static void P2PAbort(void)
{
    SEGGER_RTT_printf(0, "[P2P]: abort pending PN532 command\n");
    P2PAbortCurrentCommand(&UartNfc);
    P2pStats.abort_count++;
    SEGGER_RTT_printf(0, "[P2P][%lu] recover action=abort\n",
                      (unsigned long)sessionId);
}

static uint8_t P2PFirstMode(void)
{
#if P2P_LINK_POLICY == P2P_LINK_POLICY_FORCE_ACTIVE
    return P2P_ACTIVE_MODE;
#else
    return P2P_PASSIVE_MODE;
#endif
}

static uint8_t P2PNextMode(uint8_t mode)
{
#if P2P_LINK_POLICY == P2P_LINK_POLICY_AUTO
    return mode == P2P_PASSIVE_MODE ? P2P_ACTIVE_MODE : P2P_PASSIVE_MODE;
#elif P2P_LINK_POLICY == P2P_LINK_POLICY_FORCE_ACTIVE
    return P2P_ACTIVE_MODE;
#else
    return P2P_PASSIVE_MODE;
#endif
}

static const char *P2PFailureReason(uint8_t mode, int status, bool activation)
{
#if P2P_LINK_POLICY == P2P_LINK_POLICY_PASSIVE_RETRY_RECOVER
    return "passive_recover_retry";
#elif P2P_LINK_POLICY != P2P_LINK_POLICY_AUTO
    return "forced_policy";
#else
    if (mode == P2P_ACTIVE_MODE) {
        return "active_failed_return_passive";
    }
    if (activation) {
        return status == NFC_ETIMEOUT ? "passive_activate_timeout" :
                                        "passive_activate_rf_error";
    }
    return status == NFC_ETIMEOUT ? "passive_exchange_timeout" :
                                    "passive_rf_error";
#endif
}

static void P2PPrintStats(void)
{
    uint32_t passiveActivateTotal;
    uint32_t passiveExchangeTotal;
    uint32_t activeActivateTotal;
    uint32_t activeExchangeTotal;

    if (P2pStats.session_count == 0 ||
        P2pStats.session_count % P2P_STATS_PRINT_INTERVAL != 0 ||
        lastStatsSession == P2pStats.session_count) {
        return;
    }
    lastStatsSession      = P2pStats.session_count;
    passiveActivateTotal = P2pStats.passive_activate_ok + P2pStats.passive_activate_fail;
    passiveExchangeTotal = P2pStats.passive_exchange_ok + P2pStats.passive_exchange_fail;
    activeActivateTotal  = P2pStats.active_activate_ok + P2pStats.active_activate_fail;
    activeExchangeTotal  = P2pStats.active_exchange_ok + P2pStats.active_exchange_fail;

    SEGGER_RTT_printf(0, "[P2P][STATS]\npolicy=%s\nsessions=%lu complete_ok=%lu\n",
                      P2PPolicyName(), (unsigned long)P2pStats.session_count,
                      (unsigned long)P2pStats.complete_session_ok);
    SEGGER_RTT_printf(0, "passive_activate=%lu/%lu passive_exchange=%lu/%lu\n",
                      (unsigned long)P2pStats.passive_activate_ok,
                      (unsigned long)passiveActivateTotal,
                      (unsigned long)P2pStats.passive_exchange_ok,
                      (unsigned long)passiveExchangeTotal);
    SEGGER_RTT_printf(0, "active_activate=%lu/%lu active_exchange=%lu/%lu\n",
                      (unsigned long)P2pStats.active_activate_ok,
                      (unsigned long)activeActivateTotal,
                      (unsigned long)P2pStats.active_exchange_ok,
                      (unsigned long)activeExchangeTotal);
    SEGGER_RTT_printf(0, "abort=%lu release=%lu/%lu raw01=%lu raw0A=%lu raw25=%lu raw29=%lu\n",
                      (unsigned long)P2pStats.abort_count,
                      (unsigned long)P2pStats.release_ok,
                      (unsigned long)(P2pStats.release_ok + P2pStats.release_fail),
                      (unsigned long)P2pStats.raw_status_01,
                      (unsigned long)P2pStats.raw_status_0A,
                      (unsigned long)P2pStats.raw_status_25,
                      (unsigned long)P2pStats.raw_status_29);
    SEGGER_RTT_printf(0, "host_timeout=%lu host_invalid=%lu host_io=%lu auto_passive_ok=%lu auto_active=%lu/%lu\n",
                      (unsigned long)P2pStats.host_timeout,
                      (unsigned long)P2pStats.host_invalid_response,
                      (unsigned long)P2pStats.host_io_error,
                      (unsigned long)P2pStats.auto_passive_direct_ok,
                      (unsigned long)P2pStats.auto_passive_active_ok,
                      (unsigned long)(P2pStats.auto_passive_active_ok +
                                      P2pStats.auto_passive_active_fail));
}

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

void P2pTargetFsm(void)
{
    static enum {
        WAIT_TARGET = 0,
        TRANS,
    } State = WAIT_TARGET;
    int         status;
    const char *mode;
    bool        active;
    bool        starting;

    switch (State) {
    case WAIT_TARGET:
        starting = !UartNfc.target_init_pending;
        if (starting) {
            sessionId++;
            P2pStats.session_count++;
            SEGGER_RTT_printf(0, "[P2P][%lu] activate start mode=listen policy=target reason=startup\n",
                              (unsigned long)sessionId);
        }
        status = P2PTargetInit(&UartNfc);
        if (status == NFC_SUCCESS) {
            active = (UartNfc.activated_mode & PN532_ACTIVATED_FRAMING_MASK) ==
                     PN532_ACTIVATED_FRAMING_ACTIVE;
            mode = active ? "active" : "passive";
            if (active) {
                P2pStats.active_activate_ok++;
            } else {
                P2pStats.passive_activate_ok++;
            }
            SEGGER_RTT_printf(0, "[P2P][%lu] activate ok result=%d raw=0x%02x mode=%s\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error, mode);
            State = TRANS;
        } else if (status != NFC_WAIT) {
            P2PRecordError(status);
            SEGGER_RTT_printf(0, "[P2P][%lu] activate fail result=%d raw=0x%02x mode=listen\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error);
            if (P2PNeedsAbort(status)) {
                P2PAbort();
            } else {
                P2PClearLocalState(&UartNfc);
                SEGGER_RTT_printf(0, "[P2P][%lu] recover action=clear_local\n",
                                  (unsigned long)sessionId);
            }
            P2PPrintStats();
        }
        break;
    case TRANS:
        active = (UartNfc.activated_mode & PN532_ACTIVATED_FRAMING_MASK) ==
                 PN532_ACTIVATED_FRAMING_ACTIVE;
        mode   = active ? "active" : "passive";
        status = P2PTargetTxRx(&UartNfc, tx, sizeof(tx), rx, sizeof(rx),
                               &rxLen);
        if (status == NFC_SUCCESS) {
            if (active) {
                P2pStats.active_exchange_ok++;
            } else {
                P2pStats.passive_exchange_ok++;
            }
            SEGGER_RTT_printf(0, "[P2P][%lu] exchange ok result=%d raw=0x%02x mode=%s\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error, mode);
            if (rxLen) {
                SEGGER_RTT_printf(0, "[P2P]:get initiator message:\n");
                LED_INFO_TROGGLE();
                printfBuffer(rx, rxLen);
            }
        } else if (status == NFC_ETGRELEASED) {
            P2PRecordError(status);
            SEGGER_RTT_printf(0, "[P2P][%lu] exchange end result=%d raw=0x%02x mode=%s recover=clear_local\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error, mode);
            P2PClearLocalState(&UartNfc);
            P2pStats.complete_session_ok++;
            State = WAIT_TARGET;
        } else if (status != NFC_WAIT) {
            if (active) {
                P2pStats.active_exchange_fail++;
            } else {
                P2pStats.passive_exchange_fail++;
            }
            P2PRecordError(status);
            SEGGER_RTT_printf(0, "[P2P][%lu] exchange fail result=%d raw=0x%02x mode=%s\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error, mode);
            if (P2PNeedsAbort(status)) {
                P2PAbort();
            } else {
                P2PClearLocalState(&UartNfc);
                SEGGER_RTT_printf(0, "[P2P][%lu] recover action=clear_local\n",
                                  (unsigned long)sessionId);
            }
            State = WAIT_TARGET;
        }
        if (State == WAIT_TARGET) {
            P2PPrintStats();
        }
        break;
    }
}

void P2pInitiatorFsm(void)
{
    static enum {
        WAIT_TARGET = 0,
        TRANS,
        RELEASE,
        RECOVER,
    } State = WAIT_TARGET;
    static uint8_t  activeMode;
    static uint16_t activationWaitCount;
    static int      exchangeError;
    static const char *nextReason = "startup";
    static bool     modeInitialized;
    static bool     abortRecoveryPending;
    static bool     passiveFailed;
    int             status;
    int             recoveryStatus;
    bool            starting;

    switch (State) {
    case WAIT_TARGET:
        if (!modeInitialized) {
            activeMode      = P2PFirstMode();
            modeInitialized = true;
        }
        starting = !UartNfc.initiator_init_pending;
        if (starting) {
            sessionId++;
            P2pStats.session_count++;
            SEGGER_RTT_printf(0, "[P2P][%lu] activate start mode=%s policy=%s reason=%s\n",
                              (unsigned long)sessionId,
                              P2PModeName(activeMode), P2PPolicyName(),
                              nextReason);
        }
        status = P2PInitiatorInit(&UartNfc, activeMode, P2P_BAUD_106);
        if (status == NFC_SUCCESS) {
            activationWaitCount = 0;
            abortRecoveryPending = false;
            if (activeMode == P2P_ACTIVE_MODE) {
                P2pStats.active_activate_ok++;
            } else {
                P2pStats.passive_activate_ok++;
            }
            SEGGER_RTT_printf(0, "[P2P][%lu] activate ok result=%d raw=0x%02x mode=%s\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error, P2PModeName(activeMode));
            State = TRANS;
            break;
        }
        if (status == NFC_WAIT) {
            if (starting) {
                abortRecoveryPending = false;
                break;
            }
            activationWaitCount++;
            if (activationWaitCount < P2P_ACTIVATION_WAIT_LIMIT) {
                break;
            }
            status = NFC_ETIMEOUT;
        }
        activationWaitCount = 0;
        if (activeMode == P2P_ACTIVE_MODE) {
            P2pStats.active_activate_fail++;
        } else {
            P2pStats.passive_activate_fail++;
            passiveFailed = true;
        }
        P2PRecordError(status);
        SEGGER_RTT_printf(0, "[P2P][%lu] activate fail result=%d raw=0x%02x mode=%s\n",
                          (unsigned long)sessionId, status,
                          UartNfc.last_error, P2PModeName(activeMode));
        nextReason = P2PFailureReason(activeMode, status, true);
        if (P2PNeedsAbort(status)) {
            bool reinitialize = starting && abortRecoveryPending;

            P2PAbort();
            abortRecoveryPending = true;
            if (reinitialize) {
                Pn53xWakeUp();
                recoveryStatus = SAMConfig(&UartNfc, SAM_NORMAL_MODE);
                abortRecoveryPending = false;
                SEGGER_RTT_printf(0, "[P2P][%lu] recover action=reinitialize result=%d raw=0x%02x\n",
                                  (unsigned long)sessionId, recoveryStatus,
                                  UartNfc.last_error);
            }
        } else {
            P2PClearLocalState(&UartNfc);
            SEGGER_RTT_printf(0, "[P2P][%lu] recover action=clear_local\n",
                              (unsigned long)sessionId);
        }
        if (activeMode == P2P_ACTIVE_MODE && passiveFailed) {
            P2pStats.auto_passive_active_fail++;
            passiveFailed = false;
        }
        activeMode = P2PNextMode(activeMode);
        P2PPrintStats();
        break;
    case TRANS:
        status = P2PInitiatorTxRx(&UartNfc, tx, sizeof(tx), rx, sizeof(rx),
                                  &rxLen);
        if (status == NFC_SUCCESS) {
            if (activeMode == P2P_ACTIVE_MODE) {
                P2pStats.active_exchange_ok++;
            } else {
                P2pStats.passive_exchange_ok++;
            }
            SEGGER_RTT_printf(0, "[P2P][%lu] exchange ok result=%d raw=0x%02x mode=%s\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error, P2PModeName(activeMode));
            if (rxLen) {
                LED_INFO_TROGGLE();
                SEGGER_RTT_printf(0, "[P2P]:get target message:\n");
                printfBuffer(rx, rxLen);
            }
            State = RELEASE;
        } else {
            if (activeMode == P2P_ACTIVE_MODE) {
                P2pStats.active_exchange_fail++;
            } else {
                P2pStats.passive_exchange_fail++;
            }
            P2PRecordError(status);
            SEGGER_RTT_printf(0, "[P2P][%lu] exchange fail result=%d raw=0x%02x mode=%s\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error, P2PModeName(activeMode));
            exchangeError = status;
            State         = RECOVER;
        }
        break;
    case RELEASE:
        status = P2PInitiatorRelease(&UartNfc);
        if (status == NFC_SUCCESS) {
            P2pStats.release_ok++;
            P2pStats.complete_session_ok++;
            if (activeMode == P2P_ACTIVE_MODE && passiveFailed) {
                P2pStats.auto_passive_active_ok++;
            } else if (P2P_LINK_POLICY == P2P_LINK_POLICY_AUTO &&
                       activeMode == P2P_PASSIVE_MODE) {
                P2pStats.auto_passive_direct_ok++;
            }
            SEGGER_RTT_printf(0, "[P2P][%lu] release ok result=%d raw=0x%02x mode=%s\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error, P2PModeName(activeMode));
            P2PClearLocalState(&UartNfc);
        } else {
            P2pStats.release_fail++;
            if (activeMode == P2P_ACTIVE_MODE && passiveFailed) {
                P2pStats.auto_passive_active_fail++;
            }
            P2PRecordError(status);
            SEGGER_RTT_printf(0, "[P2P][%lu] release fail result=%d raw=0x%02x mode=%s\n",
                              (unsigned long)sessionId, status,
                              UartNfc.last_error, P2PModeName(activeMode));
            if (P2PNeedsAbort(status)) {
                P2PAbort();
                abortRecoveryPending = true;
            } else {
                P2PClearLocalState(&UartNfc);
                SEGGER_RTT_printf(0, "[P2P][%lu] recover action=clear_local\n",
                                  (unsigned long)sessionId);
            }
        }
        passiveFailed       = false;
        activeMode          = P2PFirstMode();
        nextReason          = "startup";
        activationWaitCount = 0;
        State               = WAIT_TARGET;
        P2PPrintStats();
        break;
    case RECOVER:
        nextReason = P2PFailureReason(activeMode, exchangeError, false);
        if (activeMode == P2P_PASSIVE_MODE) {
            passiveFailed = true;
        }
        if (P2PNeedsAbort(exchangeError)) {
            P2PAbort();
            abortRecoveryPending = true;
        } else {
            P2PClearLocalState(&UartNfc);
            SEGGER_RTT_printf(0, "[P2P][%lu] recover action=clear_local\n",
                              (unsigned long)sessionId);
        }
        if (activeMode == P2P_ACTIVE_MODE && passiveFailed) {
            P2pStats.auto_passive_active_fail++;
            passiveFailed = false;
        }
        activeMode          = P2PNextMode(activeMode);
        activationWaitCount = 0;
        State               = WAIT_TARGET;
        P2PPrintStats();
        break;
    }
}
