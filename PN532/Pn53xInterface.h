#ifndef __PN53xInterface_h__
#define __PN53xInterface_h__
#include <stdbool.h>
#include <stdint.h>

#define PN532_PREAMBLE    (0x00)
#define PN532_STARTCODE1  (0x00)
#define PN532_STARTCODE2  (0xFF)
#define PN532_HOSTTOPN532 (0xD4)
#define PN532_PN532TOHOST (0xD5)
#define PN532_POSTAMBLE   (0x00)

#define PN532_COMMAND_DIAGNOSE              (0x00)
#define PN532_COMMAND_GETFIRMWAREVERSION    (0x02)
#define PN532_COMMAND_GETGENERALSTATUS      (0x04)
#define PN532_COMMAND_READREGISTER          (0x06)
#define PN532_COMMAND_WRITEREGISTER         (0x08)
#define PN532_COMMAND_READGPIO              (0x0C)
#define PN532_COMMAND_WRITEGPIO             (0x0E)
#define PN532_COMMAND_SETSERIALBAUDRATE     (0x10)
#define PN532_COMMAND_SETPARAMETERS         (0x12)
#define PN532_COMMAND_SAMCONFIGURATION      (0x14)
#define PN532_COMMAND_POWERDOWN             (0x16)
#define PN532_COMMAND_RFCONFIGURATION       (0x32)
#define PN532_COMMAND_RFREGULATIONTEST      (0x58)
#define PN532_COMMAND_INJUMPFORDEP          (0x56)
#define PN532_COMMAND_INJUMPFORPSL          (0x46)
#define PN532_COMMAND_INLISTPASSIVETARGET   (0x4A)
#define PN532_COMMAND_INATR                 (0x50)
#define PN532_COMMAND_INPSL                 (0x4E)
#define PN532_COMMAND_INDATAEXCHANGE        (0x40)
#define PN532_COMMAND_INCOMMUNICATETHRU     (0x42)
#define PN532_COMMAND_INDESELECT            (0x44)
#define PN532_COMMAND_INRELEASE             (0x52)
#define PN532_COMMAND_INSELECT              (0x54)
#define PN532_COMMAND_INAUTOPOLL            (0x60)
#define PN532_COMMAND_TGINITASTARGET        (0x8C)
#define PN532_COMMAND_TGSETGENERALBYTES     (0x92)
#define PN532_COMMAND_TGGETDATA             (0x86)
#define PN532_COMMAND_TGSETDATA             (0x8E)
#define PN532_COMMAND_TGSETMETADATA         (0x94)
#define PN532_COMMAND_TGGETINITIATORCOMMAND (0x88)
#define PN532_COMMAND_TGRESPONSETOINITIATOR (0x90)
#define PN532_COMMAND_TGGETTARGETSTATUS     (0x8A)

#define PN532_RESPONSE_INDATAEXCHANGE      (0x41)
#define PN532_RESPONSE_INLISTPASSIVETARGET (0x4B)

#define PN532_ACK_WAIT_TIME        (10000)  //  timeout of waiting for ACK 10us*1500=15ms
#define PN532_DATA_FRAME_WAIT_TIME (20000) //  timeout of waiting for ACK 10us*20000=200ms
#define PN532_INVALID_ACK          (-1)
#define PN532_TIMEOUT              (-2)
#define PN532_INVALID_FRAME        (-3)
#define PN532_NO_SPACE             (-4)

#define PN532_MIN_FRAME_SIZE (7)

#define PN532_MIFARE_ISO14443A (0x00)

// Mifare Commands
#define MIFARE_CMD_AUTH_A           (0x60)
#define MIFARE_CMD_AUTH_B           (0x61)
#define MIFARE_CMD_READ             (0x30)
#define MIFARE_CMD_WRITE            (0xA0)
#define MIFARE_CMD_WRITE_ULTRALIGHT (0xA2)
#define MIFARE_CMD_TRANSFER         (0xB0)
#define MIFARE_CMD_DECREMENT        (0xC0)
#define MIFARE_CMD_INCREMENT        (0xC1)
#define MIFARE_CMD_STORE            (0xC2)

// FeliCa Commands
#define FELICA_CMD_POLLING                  (0x00)
#define FELICA_CMD_REQUEST_SERVICE          (0x02)
#define FELICA_CMD_REQUEST_RESPONSE         (0x04)
#define FELICA_CMD_READ_WITHOUT_ENCRYPTION  (0x06)
#define FELICA_CMD_WRITE_WITHOUT_ENCRYPTION (0x08)
#define FELICA_CMD_REQUEST_SYSTEM_CODE      (0x0C)

// Prefixes for NDEF Records (to identify record type)
#define NDEF_URIPREFIX_NONE         (0x00)
#define NDEF_URIPREFIX_HTTP_WWWDOT  (0x01)
#define NDEF_URIPREFIX_HTTPS_WWWDOT (0x02)
#define NDEF_URIPREFIX_HTTP         (0x03)
#define NDEF_URIPREFIX_HTTPS        (0x04)
#define NDEF_URIPREFIX_TEL          (0x05)
#define NDEF_URIPREFIX_MAILTO       (0x06)
#define NDEF_URIPREFIX_FTP_ANONAT   (0x07)
#define NDEF_URIPREFIX_FTP_FTPDOT   (0x08)
#define NDEF_URIPREFIX_FTPS         (0x09)
#define NDEF_URIPREFIX_SFTP         (0x0A)
#define NDEF_URIPREFIX_SMB          (0x0B)
#define NDEF_URIPREFIX_NFS          (0x0C)
#define NDEF_URIPREFIX_FTP          (0x0D)
#define NDEF_URIPREFIX_DAV          (0x0E)
#define NDEF_URIPREFIX_NEWS         (0x0F)
#define NDEF_URIPREFIX_TELNET       (0x10)
#define NDEF_URIPREFIX_IMAP         (0x11)
#define NDEF_URIPREFIX_RTSP         (0x12)
#define NDEF_URIPREFIX_URN          (0x13)
#define NDEF_URIPREFIX_POP          (0x14)
#define NDEF_URIPREFIX_SIP          (0x15)
#define NDEF_URIPREFIX_SIPS         (0x16)
#define NDEF_URIPREFIX_TFTP         (0x17)
#define NDEF_URIPREFIX_BTSPP        (0x18)
#define NDEF_URIPREFIX_BTL2CAP      (0x19)
#define NDEF_URIPREFIX_BTGOEP       (0x1A)
#define NDEF_URIPREFIX_TCPOBEX      (0x1B)
#define NDEF_URIPREFIX_IRDAOBEX     (0x1C)
#define NDEF_URIPREFIX_FILE         (0x1D)
#define NDEF_URIPREFIX_URN_EPC_ID   (0x1E)
#define NDEF_URIPREFIX_URN_EPC_TAG  (0x1F)
#define NDEF_URIPREFIX_URN_EPC_PAT  (0x20)
#define NDEF_URIPREFIX_URN_EPC_RAW  (0x21)
#define NDEF_URIPREFIX_URN_EPC      (0x22)
#define NDEF_URIPREFIX_URN_NFC      (0x23)

#define PN532_GPIO_VALIDATIONBIT (0x80)
#define PN532_GPIO_P30           (0)
#define PN532_GPIO_P31           (1)
#define PN532_GPIO_P32           (2)
#define PN532_GPIO_P33           (3)
#define PN532_GPIO_P34           (4)
#define PN532_GPIO_P35           (5)

// FeliCa consts
#define FELICA_READ_MAX_SERVICE_NUM     16
#define FELICA_READ_MAX_BLOCK_NUM       12 // for typical FeliCa card
#define FELICA_WRITE_MAX_SERVICE_NUM    16
#define FELICA_WRITE_MAX_BLOCK_NUM      10 // for typical FeliCa card
#define FELICA_REQ_SERVICE_MAX_NODE_NUM 32

#define SAM_NORMAL_MODE       (0X01)
#define SAM_VIRTUAL_CARD_MODE (0X02)
#define SAM_WIRED_CARD_MODE   (0X03)
#define SAM_DUAL_CARD_MODE    (0X04)

// Register addresses
#define PN53X_REG_Control_switch_rng 0x6106
#define PN53X_REG_CIU_Mode           0x6301
#define PN53X_REG_CIU_TxMode         0x6302
#define PN53X_REG_CIU_RxMode         0x6303
#define PN53X_REG_CIU_TxControl      0x6304
#define PN53X_REG_CIU_TxAuto         0x6305
#define PN53X_REG_CIU_TxSel          0x6306
#define PN53X_REG_CIU_RxSel          0x6307
#define PN53X_REG_CIU_RxThreshold    0x6308
#define PN53X_REG_CIU_Demod          0x6309
#define PN53X_REG_CIU_FelNFC1        0x630A
#define PN53X_REG_CIU_FelNFC2        0x630B
#define PN53X_REG_CIU_MifNFC         0x630C
#define PN53X_REG_CIU_ManualRCV      0x630D
#define PN53X_REG_CIU_TypeB          0x630E
// #define PN53X_REG_- 0x630F
// #define PN53X_REG_- 0x6310
#define PN53X_REG_CIU_CRCResultMSB   0x6311
#define PN53X_REG_CIU_CRCResultLSB   0x6312
#define PN53X_REG_CIU_GsNOFF         0x6313
#define PN53X_REG_CIU_ModWidth       0x6314
#define PN53X_REG_CIU_TxBitPhase     0x6315
#define PN53X_REG_CIU_RFCfg          0x6316
#define PN53X_REG_CIU_GsNOn          0x6317
#define PN53X_REG_CIU_CWGsP          0x6318
#define PN53X_REG_CIU_ModGsP         0x6319
#define PN53X_REG_CIU_TMode          0x631A
#define PN53X_REG_CIU_TPrescaler     0x631B
#define PN53X_REG_CIU_TReloadVal_hi  0x631C
#define PN53X_REG_CIU_TReloadVal_lo  0x631D
#define PN53X_REG_CIU_TCounterVal_hi 0x631E
#define PN53X_REG_CIU_TCounterVal_lo 0x631F
// #define PN53X_REG_- 0x6320
#define PN53X_REG_CIU_TestSel1     0x6321
#define PN53X_REG_CIU_TestSel2     0x6322
#define PN53X_REG_CIU_TestPinEn    0x6323
#define PN53X_REG_CIU_TestPinValue 0x6324
#define PN53X_REG_CIU_TestBus      0x6325
#define PN53X_REG_CIU_AutoTest     0x6326
#define PN53X_REG_CIU_Version      0x6327
#define PN53X_REG_CIU_AnalogTest   0x6328
#define PN53X_REG_CIU_TestDAC1     0x6329
#define PN53X_REG_CIU_TestDAC2     0x632A
#define PN53X_REG_CIU_TestADC      0x632B
// #define PN53X_REG_- 0x632C
// #define PN53X_REG_- 0x632D
// #define PN53X_REG_- 0x632E
#define PN53X_REG_CIU_RFlevelDet 0x632F
#define PN53X_REG_CIU_SIC_CLK_en 0x6330
#define PN53X_REG_CIU_Command    0x6331
#define PN53X_REG_CIU_CommIEn    0x6332
#define PN53X_REG_CIU_DivIEn     0x6333
#define PN53X_REG_CIU_CommIrq    0x6334
#define PN53X_REG_CIU_DivIrq     0x6335
#define PN53X_REG_CIU_Error      0x6336
#define PN53X_REG_CIU_Status1    0x6337
#define PN53X_REG_CIU_Status2    0x6338
#define PN53X_REG_CIU_FIFOData   0x6339
#define PN53X_REG_CIU_FIFOLevel  0x633A
#define PN53X_REG_CIU_WaterLevel 0x633B
#define PN53X_REG_CIU_Control    0x633C
#define PN53X_REG_CIU_BitFraming 0x633D
#define PN53X_REG_CIU_Coll       0x633E

/* defines */
#define PN53X_CACHE_REGISTER_MIN_ADDRESS PN53X_REG_CIU_Mode
#define PN53X_CACHE_REGISTER_MAX_ADDRESS PN53X_REG_CIU_Coll
#define PN53X_CACHE_REGISTER_SIZE        ((PN53X_CACHE_REGISTER_MAX_ADDRESS - PN53X_CACHE_REGISTER_MIN_ADDRESS) + 1)

#define PN53X_SFR_P3 0xFFB0

#define PN53X_SFR_P3CFGA 0xFFFC
#define PN53X_SFR_P3CFGB 0xFFFD
#define PN53X_SFR_P7CFGA 0xFFF4
#define PN53X_SFR_P7CFGB 0xFFF5
#define PN53X_SFR_P7     0xFFF7

#define PN53X_DIAGNOSE_TEST_ROM 0X01
#define SYMBOL_RX_LAST_BITS     0x07
#define PN532_RF_ON_MASK        (1 << 2)

// Radio Field Configure Items           // Configuration Data length
#define RFCI_FIELD                 0x01 //  1
#define RFCI_TIMING                0x02 //  3
#define RFCI_RETRY_DATA            0x04 //  1
#define RFCI_RETRY_SELECT          0x05 //  3
#define RFCI_ANALOG_TYPE_A_106     0x0A // 11
#define RFCI_ANALOG_TYPE_A_212_424 0x0B //  8
#define RFCI_ANALOG_TYPE_B         0x0C //  3
#define RFCI_ANALOG_TYPE_14443_4   0x0D //  9

typedef struct {
    struct InterfaceVtable const *vptr;
    uint32_t                      rtycnt;
    bool                          UseP70Irq;
} PN53x_Interface;

struct InterfaceVtable {
    int16_t (*PN53x_read_response_vcall)(uint8_t command, uint8_t *pbuf, uint32_t wlen, uint32_t timeoutMs);
    int (*write_command)(uint8_t *pBuf, uint32_t wLen);
    bool (*wakeup)(void);
    void (*sendack)(void);
};

void PN53x_Interface_ctor(PN53x_Interface *const me, uint32_t rtycnt);

static inline int16_t PN53x_read_response_vcall(PN53x_Interface const *me,
                                                uint8_t                command,
                                                uint8_t               *pbuf,
                                                uint32_t               wlen,
                                                uint32_t               timeoutMs)
{
    return (*me->vptr->PN53x_read_response_vcall)(command, pbuf, wlen, timeoutMs);
}

static inline bool PN53x_wakeup_vcall(PN53x_Interface const *me)
{
    return (*me->vptr->wakeup)();
}

static inline int PN53x_write_command_vcall(PN53x_Interface const *me,
                                            uint8_t               *pBuf,
                                            uint32_t               wLen)
{
    return (*me->vptr->write_command)(pBuf, wLen);
}

static inline void PN53x_SendAck_vcall(PN53x_Interface const *me)
{
   (*me->vptr->sendack)();
}
#endif
