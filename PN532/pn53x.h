#ifndef __PN53x_H__
#define __PN53x_H__
#include "Pn53xInterface.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *uid;
    uint8_t  uidlen;
    uint8_t *keydata;
    uint32_t keynum;
} mifare_t;

typedef enum {
    PSM_NORMAL       = 0x01,
    PSM_VIRTUAL_CARD = 0x02,
    PSM_WIRED_CARD   = 0x03,
    PSM_DUAL_CARD    = 0x04
} pn532_sam_mode;

typedef struct {
    uint8_t *uid;
    uint8_t  uidlen;
    uint8_t *keydata;
    uint32_t keynum;
    /** Is the CRC automaticly added, checked and removed from the frames */
    bool bCrc;
    /** Does the chip handle parity bits, all parities are handled as data */
    bool bPar;
    /** Should the chip handle frames encapsulation and chaining */
    bool bEasyFraming;
    /** Should the chip try forever on select? */
    bool bInfiniteSelect;
    /** Should the chip switch automatically activate ISO14443-4 when
        selecting tags supporting it? */
    bool bAutoIso14443_4;
    /** Supported modulation encoded in a byte */
    uint8_t btSupportByte;
    /** Last reported error */
    int              last_error;
    uint8_t          ui8Parameters;
    pn532_sam_mode   sammode;
    PN53x_Interface *_interface;
} nfc;

typedef struct {
    uint16_t SensRes; /*ATQA*/
    uint8_t  SelRes;  /*SAK*/
    uint8_t  uidlen;
    uint8_t  NfcId[16];
} NfcTarget;

static struct {
    uint16_t ATQA;
    uint8_t  SAK;
    uint8_t  UID[10];
    enum {
        UIDSize_No_UID = 0,
        UIDSize_Single = 4,
        UIDSize_Double = 7,
        UIDSize_Triple = 10
    } UIDSize;
} CardCharacteristics = {0};

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

typedef enum {
    CardType_NXP_MIFARE_Mini =
        0, // do NOT assign another CardType item with a specific value since
           // there are loops over this type
    CardType_NXP_MIFARE_Classic_1k,
    CardType_NXP_MIFARE_Classic_4k,
    CardType_NXP_MIFARE_Ultralight,
    //	CardType_NXP_MIFARE_Ultralight_C,
    //	CardType_NXP_MIFARE_Ultralight_EV1,
    CardType_NXP_MIFARE_DESFire,
    CardType_NXP_MIFARE_DESFire_EV1,
    CardType_IBM_JCOP31,
    CardType_IBM_JCOP31_v241,
    CardType_IBM_JCOP41_v22,
    CardType_IBM_JCOP41_v231,
    CardType_Infineon_MIFARE_Classic_1k,
    CardType_Gemplus_MPCOS,
    CardType_Innovision_Jewel,
    CardType_Nokia_MIFARE_Classic_4k_emulated_6212,
    CardType_Nokia_MIFARE_Classic_4k_emulated_6131
} CardType;

/**
 * Properties
 */
typedef enum {
    /**
     * Default command processing timeout
     * Property value's (duration) unit is ms and 0 means no timeout (infinite).
     * Default value is set by driver layer
     */
    NP_TIMEOUT_COMMAND,
    /**
     * Timeout between ATR_REQ and ATR_RES
     * When the device is in initiator mode, a target is considered as mute if no
     * valid ATR_RES is received within this timeout value.
     * Default value for this property is 103 ms on PN53x based devices.
     */
    NP_TIMEOUT_ATR,
    /**
     * Timeout value to give up reception from the target in case of no answer.
     * Default value for this property is 52 ms).
     */
    NP_TIMEOUT_COM,
    /** Let the PN53X chip handle the CRC bytes. This means that the chip appends
     * the CRC bytes to the frames that are transmitted. It will parse the last
     * bytes from received frames as incoming CRC bytes. They will be verified
     * against the used modulation and protocol. If an frame is expected with
     * incorrect CRC bytes this option should be disabled. Example frames where
     * this is useful are the ATQA and UID+BCC that are transmitted without CRC
     * bytes during the anti-collision phase of the ISO14443-A protocol. */
    NP_HANDLE_CRC,
    /** Parity bits in the network layer of ISO14443-A are by default generated
     * and validated in the PN53X chip. This is a very convenient feature. On
     * certain times though it is useful to get full control of the transmitted
     * data. The proprietary MIFARE Classic protocol uses for example custom
     * (encrypted) parity bits. For interoperability it is required to be
     * completely compatible, including the arbitrary parity bits. When this
     * option is disabled, the functions to communicating bits should be used. */
    NP_HANDLE_PARITY,
    /** This option can be used to enable or disable the electronic field of the
     * NFC device. */
    NP_ACTIVATE_FIELD,
    /** The internal CRYPTO1 co-processor can be used to transmit messages
     * encrypted. This option is automatically activated after a successful MIFARE
     * Classic authentication. */
    NP_ACTIVATE_CRYPTO1,
    /** The default configuration defines that the PN53X chip will try
     * indefinitely to invite a tag in the field to respond. This could be desired
     * when it is certain a tag will enter the field. On the other hand, when this
     * is uncertain, it will block the application. This option could best be
     * compared to the (NON)BLOCKING option used by (socket)network programming.
     */
    NP_INFINITE_SELECT,
    /** If this option is enabled, frames that carry less than 4 bits are allowed.
     * According to the standards these frames should normally be handles as
     * invalid frames. */
    NP_ACCEPT_INVALID_FRAMES,
    /** If the NFC device should only listen to frames, it could be useful to let
     * it gather multiple frames in a sequence. They will be stored in the
     * internal FIFO of the PN53X chip. This could be retrieved by using the
     * receive data functions. Note that if the chip runs out of bytes (FIFO = 64
     * bytes long), it will overwrite the first received frames, so quick
     * retrieving of the received data is desirable. */
    NP_ACCEPT_MULTIPLE_FRAMES,
    /** This option can be used to enable or disable the auto-switching mode to
     * ISO14443-4 is device is compliant.
     * In initiator mode, it means that NFC chip will send RATS automatically when
     * select and it will automatically poll for ISO14443-4 card when ISO14443A is
     * requested.
     * In target mode, with a NFC chip compliant (ie. PN532), the chip will
     * emulate a 14443-4 PICC using hardware capability */
    NP_AUTO_ISO14443_4,
    /** Use automatic frames encapsulation and chaining. */
    NP_EASY_FRAMING,
    /** Force the chip to switch in ISO14443-A */
    NP_FORCE_ISO14443_A,
    /** Force the chip to switch in ISO14443-B */
    NP_FORCE_ISO14443_B,
    /** Force the chip to run at 106 kbps */
    NP_FORCE_SPEED_106,
} nfc_property;

#define NFC_SUCCESS      0   /*Success (no error)*/
#define NFC_EIO          -1  /* Input / output error*/
#define NFC_EINVARG      -2  /*Invalid argument(s)*/
#define NFC_EDEVNOTSUPP  -3  /*Operation not supported by device*/
#define NFC_ENOTSUCHDEV  -4  /*No such device*/
#define NFC_EOVFLOW      -5  /*Buffer overflow*/
#define NFC_ETIMEOUT     -6  /*Operation timed out*/
#define NFC_EOPABORTED   -7  /*Operation aborted (by user)*/
#define NFC_ENOTIMPL     -8  /*Not (yet) implemented*/
#define NFC_EINVRESPOND  -9  /*Input / output  error respond*/
#define NFC_ETGRELEASED  -10 /*Target released*/
#define NFC_ERFTRANS     -20 /*Error while RF transmission*/
#define NFC_EMFCAUTHFAIL -30 /*MIFARE Classic: authentication failed*/
#define NFC_ESOFT        -80 /*Software error (allocation, file/pipe creation, \
                                etc.)*/
#define NFC_WAIT      -88    /*Wait for data frame.*/
#define NFC_ECHIP     -90    /*Device's internal chip error*/
#define NFC_EPROTOCOL -100   /*Error protocol.*/

// Registers and symbols masks used to covers parts within a register
//   PN53X_REG_CIU_TxMode
#define SYMBOL_TX_CRC_ENABLE 0x80
#define SYMBOL_TX_SPEED      0x70
// TX_FRAMING bits explanation:
//   00 : ISO/IEC 14443A/MIFARE and Passive Communication mode 106 kbit/s
//   01 : Active Communication mode
//   10 : FeliCa and Passive Communication mode at 212 kbit/s and 424 kbit/s
//   11 : ISO/IEC 14443B
#define SYMBOL_TX_FRAMING 0x03

//   PN53X_REG_Control_switch_rng
#define SYMBOL_CURLIMOFF \
    0x08 /* When set to 1, the 100 mA current limitations is desactivated. */
#define SYMBOL_SIC_SWITCH_EN \
    0x10 /* When set to logic 1, the SVDD switch is enabled and the SVDD output \
            delivers power to secure IC and internal pads (SIGIN, SIGOUT and \
            P34). */
#define SYMBOL_RANDOM_DATAREADY \
    0x02 /* When set to logic 1, a new random number is available. */

//   PN53X_REG_CIU_RxMode
#define SYMBOL_RX_CRC_ENABLE 0x80
#define SYMBOL_RX_SPEED      0x70
#define SYMBOL_RX_NO_ERROR   0x08
#define SYMBOL_RX_MULTIPLE   0x04
// RX_FRAMING follow same scheme than TX_FRAMING
#define SYMBOL_RX_FRAMING 0x03

//   PN53X_REG_CIU_TxAuto
#define SYMBOL_FORCE_100_ASK 0x40
#define SYMBOL_AUTO_WAKE_UP  0x20
#define SYMBOL_INITIAL_RF_ON 0x04

//   PN53X_REG_CIU_ManualRCV
#define SYMBOL_PARITY_DISABLE 0x10

//   PN53X_REG_CIU_TMode
#define SYMBOL_TAUTO        0x80
#define SYMBOL_TPRESCALERHI 0x0F

//   PN53X_REG_CIU_TPrescaler
#define SYMBOL_TPRESCALERLO 0xFF

//   PN53X_REG_CIU_Command
#define SYMBOL_COMMAND            0x0F
#define SYMBOL_COMMAND_TRANSCEIVE 0xC

//   PN53X_REG_CIU_Status2
#define SYMBOL_MF_CRYPTO1_ON 0x08

//   PN53X_REG_CIU_FIFOLevel
#define SYMBOL_FLUSH_BUFFER 0x80
#define SYMBOL_FIFO_LEVEL   0x7F

//   PN53X_REG_CIU_Control
#define SYMBOL_INITIATOR    0x10
#define SYMBOL_RX_LAST_BITS 0x07

//   PN53X_REG_CIU_BitFraming
#define SYMBOL_START_SEND   0x80
#define SYMBOL_RX_ALIGN     0x70
#define SYMBOL_TX_LAST_BITS 0x07

// PN53X Support Byte flags
#define SUPPORT_ISO14443A 0x01
#define SUPPORT_ISO14443B 0x02
#define SUPPORT_ISO18092  0x04

// Internal parameters flags
#define PARAM_NONE         0x00
#define PARAM_NAD_USED     0x01
#define PARAM_DID_USED     0x02
#define PARAM_AUTO_ATR_RES 0x04
#define PARAM_AUTO_RATS    0x10
#define PARAM_14443_4_PICC 0x20 /* Only for PN532 */
#define PARAM_NFC_SECURE   0x20 /* Only for PN533 */
#define PARAM_NO_AMBLE     0x40 /* Only for PN532 */

// Radio Field Configure Items           // Configuration Data length
#define RFCI_FIELD                 0x01 //  1
#define RFCI_TIMING                0x02 //  3
#define RFCI_RETRY_DATA            0x04 //  1
#define RFCI_RETRY_SELECT          0x05 //  3
#define RFCI_ANALOG_TYPE_A_106     0x0A // 11
#define RFCI_ANALOG_TYPE_A_212_424 0x0B //  8
#define RFCI_ANALOG_TYPE_B         0x0C //  3
#define RFCI_ANALOG_TYPE_14443_4   0x0D //  9

#define RF_VARIOUS_TIMINGS_100US   0X01
#define RF_VARIOUS_TIMINGS_200US   0X02
#define RF_VARIOUS_TIMINGS_400US   0X03
#define RF_VARIOUS_TIMINGS_800US   0X04
#define RF_VARIOUS_TIMINGS_1_6MS   0X05
#define RF_VARIOUS_TIMINGS_3_2MS   0X06
#define RF_VARIOUS_TIMINGS_6_4MS   0X07
#define RF_VARIOUS_TIMINGS_12_8MS  0X08
#define RF_VARIOUS_TIMINGS_25_6MS  0X09
#define RF_VARIOUS_TIMINGS_51_2MS  0X0A
#define RF_VARIOUS_TIMINGS_102_4MS 0X0B
#define RF_VARIOUS_TIMINGS_204_8MS 0X0C
#define RF_VARIOUS_TIMINGS_409_6MS 0X0D
#define RF_VARIOUS_TIMINGS_819_2MS 0X0E

/* PN53x specific errors */
#define ETIMEOUT     0x01
#define ECRC         0x02
#define EPARITY      0x03
#define EBITCOUNT    0x04
#define EFRAMING     0x05
#define EBITCOLL     0x06
#define ESMALLBUF    0x07
#define EBUFOVF      0x09
#define ERFTIMEOUT   0x0a
#define ERFPROTO     0x0b
#define EOVHEAT      0x0d
#define EINBUFOVF    0x0e
#define EINVPARAM    0x10
#define EDEPUNKCMD   0x12
#define EINVRXFRAM   0x13
#define EMFAUTH      0x14
#define ENSECNOTSUPP 0x18 // PN533 only
#define EBCC         0x23
#define EDEPINVSTATE 0x25
#define EOPNOTALL    0x26
#define ECMD         0x27
#define ETGREL       0x29
#define ECID         0x2a
#define ECDISCARDED  0x2b
#define ENFCID3      0x2c
#define EOVCURRENT   0x2d
#define ENAD         0x2e

#define PN532_FRAME_DEFAULT_WAIT_TIME  5000 // default is 60ms
#define PN532_FRAME_SAMCFG_WAIT_TIME   1500
#define PN532_FRAME_INLIST_TARGET_TIME 150

typedef struct {
    uint16_t ATQA;
    bool     ATQARelevant;

    uint8_t SAK;
    bool    SAKRelevant;

    uint8_t ATS[16];
    uint8_t ATSSize;
    bool    ATSRelevant;

    char Manufacturer[16];
    char Type[64];
} CardIdentificationType;

typedef NfcTarget NfcTarget_t;

#define PN53X_BAUDRATE 460800

void nfc_ctor(nfc *const me, PN53x_Interface *interface);
void WakeUp(nfc *const me);
// bool InListPassiveTarget(nfc *const me,uint8_t chMaxTg,uint8_t
// chBrty,uint32_t wLen);
int  mifare_write_block(nfc *const me, uint8_t blocknum, uint8_t *pData);
int  mifare_read_block(nfc *const me, uint8_t blocknum, uint8_t *pData);
int  mifare_authenticate_block(nfc *const me, mifare_t *authenticate_t,
                               uint8_t block_num);
int  inDataExchange(nfc *const me, uint8_t *pBuf, uint32_t wLen);
int  InJumpForDEP(nfc *const me);
int  tgInitAsTarget(nfc *const me);
int  SAMConfig(nfc *const me, uint8_t mode);
bool I2cSAMConfig(nfc *const me, uint8_t mode);
int  readRegister(nfc *const me, uint16_t reg, uint8_t *data);
int  writeRegister(nfc *const me, uint16_t reg, uint8_t val);
int  get_firmware_version(nfc *const me, uint32_t *version);
int  InListPassiveTarget(nfc *const me);
int  readPassiveTargetID(nfc *const me, uint8_t cardbaudrate,
                         NfcTarget_t *target, uint16_t timeout);
int  setPassiveActivationRetries(nfc *const me, uint8_t MxRtyATR,
                                 uint8_t MxRtyPSL, uint8_t maxRetries);
bool Pn53xQuickSetWiInterface(void);
bool PN53xQuickSetBaudrate(uint32_t newBaudRate);
void PN53xInit(void);
bool CheckRfStatus(void);
bool Pn53xReadCardID(NfcTarget_t *target);
bool PN53xPowerOff(void);
void PwrMonitorInit(void);
int  DiagnoseNfcIc(void);
void Pn53xWakeUp(void);
int  Pn53xGetFirmVersion(uint32_t *version);
int  Pn53xGetRssiValue(uint8_t *rssi);
bool SAMConfigNormalMode(void);
bool Pn53xTransmitBits(uint8_t ByteNumb, uint8_t *data, uint8_t LastBitsNum);
void PN53xNestAttackInit(void);
bool Pn53xAntennaOff(void);
bool Pn53xAntennaOn(void);
bool Pn53xListTarget(NfcTarget_t *target);
bool Pn53xTransceiveBits(uint8_t ByteNumb, uint8_t *data, uint8_t LastBitsNum,
                         uint8_t *Rcv, uint32_t Rxlen);
int  InCommunicateThru(nfc *const me, uint8_t *pBuf, uint32_t wLen,
                       uint8_t *Rxbuf, uint32_t Rxlen);
int  Pn53xInitorTransceiveBits(nfc *const me, const uint8_t *pTx,
                               const uint16_t TxBits, const uint8_t *pTxPar,
                               uint8_t *pRx, uint8_t *pRxPar);
int  Pn53xSetParityAuto(nfc *const me, bool bAuto);
int  MifareAuthenticateBlock(nfc *const me, uint8_t keytype, uint8_t *key,
                             uint8_t *uid, uint8_t uidlen, uint8_t block_num);
int  Pn53xSetPropertyBool(nfc *const pnd, const nfc_property property,
                          const bool bEnable);
int  Pn53xResetSettings(nfc *const pnd);
int  Pn53xInitiatorInit(nfc *const pnd);
int  Pn53xSetParamter(nfc *const pnd, uint8_t flag);
void Pn53xInitorResetTag(void);
int  Pn53xWriteRegisterGroup(nfc *const me, uint16_t *reg, uint8_t *val,
                             uint8_t regcount);
int  Pn53xConfigurationTimeout(nfc *const me, uint8_t fATR_RES_Timeout,
                               uint8_t fRetryTimeout);
bool Pn53xInitorRegisterResetTag(void);
bool Pn53xPeriodReadCardID(NfcTarget_t *target);
int  MifareReplaceId(uint8_t *id);
int  MifareReadBlock(nfc *const me, uint8_t blocknum, uint8_t *pData);
int  MifareWriteBlockEx(nfc *const me, uint8_t blocknum, uint8_t *pData);
bool Pn53xTurnOnTxRxCrc(void);
int  P2PInitiatorInit(nfc *const me);
int  P2PTargetInit(nfc *const me);
int  P2PTargetTxRx(nfc *const me, uint8_t *tx, uint32_t txLen, uint8_t *rx, uint8_t *rxLen);
int  P2PInitiatorTxRx(nfc *const me, uint8_t *tx, uint32_t txLen, uint8_t *rx, uint8_t *rxLen);
int  tgSetData(nfc *const me, uint8_t *pData, uint32_t wLen);
int  tgGetData(nfc *const me, uint8_t *pBuf);
#endif
