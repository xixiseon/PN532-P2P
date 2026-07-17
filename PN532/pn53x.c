#include "PN53x.h"
#include "PN53xInterface.h"
#include "pn53x_hsu.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static Hsu_Interface uart;
nfc                  UartNfc;

static uint8_t pn532_packetbuf[64];
static uint8_t pn532_recvbuf[64];
static uint8_t pn532_bodybuf[64];

#define PN532_ACTIVATED_MODE_DEP 0x04

uint8_t inListedTag = 0x00;

static void Delay(uint32_t tick)
{
    for (uint32_t i = 0; i < tick; i++) {
        for (uint32_t i = 0; i < 1000; i++) {
            /* prevent compiler optimizations.*/
            __asm volatile("MOV R0,R0");
        };
    }
}

static const uint8_t ByteMirror[256] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0,
    0x30, 0xb0, 0x70, 0xf0, 0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 0x04, 0x84, 0x44, 0xc4,
    0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc,
    0x3c, 0xbc, 0x7c, 0xfc, 0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 0x0a, 0x8a, 0x4a, 0xca,
    0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6,
    0x36, 0xb6, 0x76, 0xf6, 0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 0x01, 0x81, 0x41, 0xc1,
    0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9,
    0x39, 0xb9, 0x79, 0xf9, 0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 0x0d, 0x8d, 0x4d, 0xcd,
    0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3,
    0x33, 0xb3, 0x73, 0xf3, 0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 0x07, 0x87, 0x47, 0xc7,
    0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf,
    0x3f, 0xbf, 0x7f, 0xff};

uint8_t mirror(uint8_t bt)
{
    return ByteMirror[bt];
}

static void mirror_bytes(uint8_t *pbts, uint16_t szLen)
{
    uint16_t szByteNr;

    for (szByteNr = 0; szByteNr < szLen; szByteNr++) {
        *pbts = ByteMirror[*pbts];
        pbts++;
    }
}

uint32_t mirror32(uint32_t ui32Bits)
{
    mirror_bytes((uint8_t *)&ui32Bits, 4);
    return ui32Bits;
}

uint64_t mirror64(uint64_t ui64Bits)
{
    mirror_bytes((uint8_t *)&ui64Bits, 8);
    return ui64Bits;
}

void nfc_ctor(nfc *const me, PN53x_Interface *interface)
{
    me->_interface             = interface;
    me->bPar                   = true;
    me->last_error             = 0;
    me->activated_mode         = 0;
    me->initiator_init_pending = false;
    me->target_init_pending    = false;
}

bool data_packing(uint8_t *pHeader, uint32_t wLen, uint8_t *pBody)
{
    uint32_t wlcs = 0;
    uint8_t  wCheckSum;
    uint32_t wIndex;
    if (NULL == pHeader || NULL == pBody) {
        return false;
    }
    *pHeader++ = PN532_PREAMBLE;   // preamble 0x00
    *pHeader++ = PN532_STARTCODE1; // start code 0x00
    *pHeader++ = PN532_STARTCODE2; // shart code 0xff
    *pHeader++ = wLen;
    wlcs       = ~wLen + 1;
    *pHeader++ = wlcs;
    *pHeader++ = PN532_HOSTTOPN532;
    wCheckSum  = PN532_HOSTTOPN532; // TFI+DATA=CHECKSUM
    for (wIndex = 0; wIndex < wLen - 1; wIndex++) {
        wCheckSum  = wCheckSum + pBody[wIndex];
        *pHeader++ = pBody[wIndex];
    }
    wCheckSum  = ~wCheckSum + 1;
    *pHeader++ = wCheckSum;
    *pHeader   = PN532_POSTAMBLE;
    return true;
}

void WakeUp(nfc *const me)
{
    me->activated_mode         = 0;
    me->initiator_init_pending = false;
    me->target_init_pending    = false;
    PN53x_wakeup_vcall(me->_interface);
    Delay(15);
}

void PN53xInit(void)
{
    Hsu_Interface_ctor(&uart, 10, 115200);
    nfc_ctor(&UartNfc, (PN53x_Interface *)&uart);
}

int SAMConfig(nfc *const me, uint8_t mode)
{
    uint8_t *pframe;
    int16_t  status;
    pframe    = pn532_bodybuf;
    pframe[0] = PN532_COMMAND_SAMCONFIGURATION;
    pframe[1] = mode; // normal mode
    pframe[2] = 0x00; // timeout no timeout
    pframe[3] = 0x00; // use irq pin
    if (!data_packing(pn532_packetbuf, 5, pframe)) {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            5 + 7)) < 0) {
        return status;
    }

    if ((status = PN53x_read_response_vcall(me->_interface,
                                            PN532_COMMAND_SAMCONFIGURATION,
                                            pn532_recvbuf, sizeof(pn532_recvbuf),
                                            PN532_FRAME_SAMCFG_WAIT_TIME)) < 0) {
        return status;
    }

    return NFC_SUCCESS;
}

/**************************************************************************/
/*!
    @brief  Read a PN532 register.

    @param  reg  the 16-bit register address.

    @returns  The register value.
*/
/**************************************************************************/
int readRegister(nfc *const me, uint16_t reg, uint8_t *data)
{
    uint8_t *pframe;
    int16_t  status;
    pframe    = pn532_bodybuf;
    pframe[0] = PN532_COMMAND_READREGISTER;
    pframe[1] = (reg >> 8) & 0xFF;
    pframe[2] = reg & 0xFF;
    if (!data_packing(pn532_packetbuf, 4, pframe)) {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            4 + 7)) < 0) {
        return status;
    }
    Delay(20);
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_READREGISTER, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }
    *data = pn532_recvbuf[0];
    return NFC_SUCCESS;
}

int PN53xDiagnose(nfc *const me, uint16_t NumTst, uint8_t *inPara,
                  uint32_t inParamLen)
{
    uint8_t *pframe;
    int16_t  status;
    uint32_t totalLen;

    pframe    = pn532_bodybuf;
    pframe[0] = PN532_COMMAND_DIAGNOSE;
    pframe[1] = NumTst;
    if (inPara && inParamLen > 0) {
        for (uint32_t i = 0; i < inParamLen; i++) {
            pframe[i + 2] = *inPara++;
        }
    }
    totalLen = inParamLen + 2 + 1;
    if (!data_packing(pn532_packetbuf, totalLen, pframe)) {
        return NFC_ESOFT;
    }

    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            totalLen + 7)) < 0) {
        return status;
    }
    // SysTickDelayMs(1800);
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_DIAGNOSE, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }
    uint8_t result = pn532_recvbuf[0];
    if (result == 0xff) {
        return NFC_SUCCESS;
    }
    return NFC_ESOFT;
}
int Pn53xWriteRegisterGroup(nfc *const me, uint16_t *reg, uint8_t *val,
                            uint8_t regcount)
{
    uint8_t *pframe;
    int      status;
    pframe    = pn532_bodybuf;
    pframe[0] = PN532_COMMAND_WRITEREGISTER;

    uint16_t *regPointer = reg;
    uint8_t  *valPointer = val;

    for (uint8_t i = 0x00; i < regcount; i++) {
        pframe[3 * i + 1] = (*regPointer >> 8) & 0xFF;
        pframe[3 * i + 2] = *regPointer & 0xFF;
        pframe[3 * i + 3] = *valPointer;
        regPointer++;
        valPointer++;
    }

    if (!data_packing(pn532_packetbuf, 1 + regcount * 3 + 1, pframe)) {
        return NFC_ESOFT;
    }

    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            1 + regcount * 3 + 1 + 7)) < 0) {
        return status;
    }

    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_WRITEREGISTER, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }

    return NFC_SUCCESS;
}

/**************************************************************************/
/*!
    @brief  Write to a PN532 register.
    @param  reg  the 16-bit register address.
    @param  val  the 8-bit value to write.
    @returns  0 for failure, 1 for success.
*/
/**************************************************************************/
int writeRegister(nfc *const me, uint16_t reg, uint8_t val)
{
    uint8_t *pframe;
    int      status;
    pframe = pn532_bodybuf;

    pframe[0] = PN532_COMMAND_WRITEREGISTER;
    pframe[1] = (reg >> 8) & 0xFF;
    pframe[2] = reg & 0xFF;
    pframe[3] = val;

    if (!data_packing(pn532_packetbuf, 5, pframe)) {
        return NFC_ESOFT;
    }

    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            5 + 7)) < 0) {
        return status;
    }
    // read data packet
    // Delay(20);
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_WRITEREGISTER, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }

    return NFC_SUCCESS;
}

int Pn53xWriteRegisterWithMask(nfc *const     pnd,
                               const uint16_t ui16RegisterAddress,
                               const uint8_t  ui8SymbolMask,
                               const uint8_t  ui8Value)
{
    if ((ui16RegisterAddress < PN53X_CACHE_REGISTER_MIN_ADDRESS) ||
        (ui16RegisterAddress > PN53X_CACHE_REGISTER_MAX_ADDRESS)) {
        // Direct write
        if (ui8SymbolMask != 0xff) {
            int     res = 0;
            uint8_t ui8CurrentValue;
            if ((res = readRegister(pnd, ui16RegisterAddress, &ui8CurrentValue)) <
                0) {
                return res;
            }
            uint8_t ui8NewValue =
                ((ui8Value & ui8SymbolMask) | (ui8CurrentValue & (~ui8SymbolMask)));
            if (ui8NewValue != ui8CurrentValue) {
                return writeRegister(pnd, ui16RegisterAddress, ui8NewValue);
            }
        } else {
            return writeRegister(pnd, ui16RegisterAddress, ui8Value);
        }
    } else {
        // Write-back cache area
        // const int internal_address = ui16RegisterAddress -
        // PN53X_CACHE_REGISTER_MIN_ADDRESS;
    }
    return NFC_SUCCESS;
}

int powerDown(nfc *const me, uint8_t wakeUpParam, uint8_t GenerateIrqParam)
{
    uint8_t *pframe;
    int16_t  status;
    pframe = pn532_bodybuf;

    pframe[0] = PN532_COMMAND_POWERDOWN;
    pframe[1] = wakeUpParam;
    pframe[2] = GenerateIrqParam;

    if (!data_packing(pn532_packetbuf, 4, pframe)) {
        return NFC_ESOFT;
    }

    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            4 + 7)) < 0) {
        return status;
    }
    // read data packet
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_POWERDOWN, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }
    return NFC_SUCCESS;
}

/***** ISO14443A Commands ******/

/**************************************************************************/
/*!
    Waits for an ISO14443A target to enter the field

    @param  cardBaudRate  Baud rate of the card
    @param  uid           Pointer to the array that will be populated
                          with the card's UID (up to 7 bytes)
    @param  uidLength     Pointer to the variable that will hold the
                          length of the card's UID.

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
int readPassiveTargetID(nfc *const me, uint8_t cardbaudrate,
                        NfcTarget_t *target, uint16_t timeout)
{
    uint8_t *pframe;
    int16_t  status;
    pframe    = pn532_bodybuf;
    pframe[0] = PN532_COMMAND_INLISTPASSIVETARGET;
    pframe[1] = 1;
    pframe[2] = cardbaudrate; /* we only support 106kbps.*/
    if (!data_packing(pn532_packetbuf, 4, pframe)) {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            4 + 7)) < 0) {
        return status;
    }
    Delay(300);
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_INLISTPASSIVETARGET, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }
    // check some basic stuff
    /* ISO14443A card response should be in the following format:

      byte            Description
      -------------   ------------------------------------------
      b0              Tags Found
      b1              Tag Number (only one used in this example)
      b2..3           SENS_RES
      b4              SEL_RES
      b5              NFCID Length
      b6..NFCIDLen    NFCID
    */
    if (pn532_recvbuf[0] != 1) {
        return NFC_EPROTOCOL;
    }
    target->SensRes = pn532_recvbuf[2];
    target->SensRes <<= 8;
    target->SensRes |= pn532_recvbuf[3];
    target->SelRes = pn532_recvbuf[4];
    target->uidlen = pn532_recvbuf[5];
    for (uint8_t i = 0; i < pn532_recvbuf[5]; i++) {
        target->NfcId[i] = pn532_recvbuf[6 + i];
    }
    return NFC_SUCCESS;
}

int SetBaudRate(nfc *const me, uint32_t baudrate)
{
    uint8_t  baudParam = 0x00;
    int16_t  status;
    uint8_t *pframe = pn532_bodybuf;
    uint8_t  ack[6] = {0, 0, 0xFF, 0, 0xFF, 0};
    if (baudrate < 9600 || baudrate > 1288000) {
        return NFC_EINVARG;
    }
    /* by default 115200k*/
    switch (baudrate) {
    case 9600:
        baudParam = 0x00;
        break;
    case 19200:
        baudParam = 0x01;
        break;
    case 38400:
        baudParam = 0x02;
        break;
    case 57600:
        baudParam = 0x03;
        break;
    case 115200:
        baudParam = 0x04;
        break;
    case 230400:
        baudParam = 0x05;
        break;
    case 460800:
        baudParam = 0x06;
        break;
    case 921600:
        baudParam = 0x07;
        break;
    case 1288000:
        baudParam = 0x08;
        break;
    default:
        baudParam = 0x04;
        break;
    }

    pframe[0] = PN532_COMMAND_SETSERIALBAUDRATE;

    pframe[1] = baudParam;

    if (!data_packing(pn532_packetbuf, 3, pframe)) {
        return NFC_ESOFT;
    }

    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            3 + 7)) < 0) {
        return status;
    }

    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_SETSERIALBAUDRATE, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }
    /* This ACK is mandatory*/
    PN53x_write_command_vcall(me->_interface, ack, sizeof(ack));

    return NFC_SUCCESS;
}

int DiagnoseNfcIc(void)
{
    int ret = 0x55;

    WakeUp(&UartNfc);
    Delay(30);
    ret = PN53xDiagnose(&UartNfc, PN53X_DIAGNOSE_TEST_ROM, 0, 0);

    return ret;
}

bool PN53xQuickSetBaudrate(uint32_t newBaudRate)
{
    bool ret = false;

    WakeUp(&UartNfc);
    WakeUp(&UartNfc);
    /*Check Securt status.*/
    SAMConfig(&UartNfc, 1);

    if (SetBaudRate(&UartNfc, newBaudRate) < 0) {
        /* TODO: ErrorHandle.*/
        /* PN532 Init Fail. Reset system.prevent off-board debug.*/

        // NVIC_SystemReset();
        while (1)
            ;
    } else {
        PN53xUartSetBaudrate(newBaudRate);
        ret = true;
    }
    PN53xUartSetBaudrate(newBaudRate);

    return ret;
}

bool SAMConfigNormalMode(void)
{

    if (SAMConfig(&UartNfc, SAM_NORMAL_MODE) < 0) {

        return false;
    }

    return true;
}

int Pn53xWriteCardID(uint8_t *id)
{
    //唤醒
    int         ret = 0;
    NfcTarget_t target;
    uint8_t     dftKey[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    WakeUp(&UartNfc);
    Delay(30);
    //
    do {
        //配置模式
        if (SAMConfig(&UartNfc, SAM_NORMAL_MODE) < 0) {
            ret = -1;
            break;
        }
        //选卡
        if (readPassiveTargetID(&UartNfc, 0x00, &target, 0x00) < 0) {
            ret = -2;
            break;
        }
        //验证A扇区秘钥0x00 或者0xff?
        if (MifareAuthenticateBlock(&UartNfc, MIFARE_CMD_AUTH_A, dftKey,
                                    target.NfcId, target.uidlen, 0) <
            0) {
            ret = -3;
            break;
        }
        //将原先的数据读出修改之后重新写入。

    } while (0);

    return ret;
}

bool Pn53xReadCardID(NfcTarget_t *target)
{
    bool ret = false;

    WakeUp(&UartNfc);
    Delay(30);
    do {
        if (SAMConfig(&UartNfc, SAM_NORMAL_MODE) < 0) {
            ret = false;
            break;
        }
        if (readPassiveTargetID(&UartNfc, 0x00, target, 0x00) < 0) {
            ret = false;
            break;
        }
        ret = true;
    } while (0);
    powerDown(&UartNfc, 0x18, 0x00);

    return ret;
}

/**
 * @brief Check PN532 RF status
 *
 * @return true: RF is exist
 * @return false: RF is not exist
 */
bool CheckRfStatus(void)
{
    uint8_t Value = 0x00;
    WakeUp(&UartNfc);
    Delay(10);
    if (readRegister(&UartNfc, PN53X_REG_CIU_Status1, &Value) < 0) {
        /* read fail. PN532 not response.*/
        return false;
    }
    if ((Value & PN532_RF_ON_MASK) == PN532_RF_ON_MASK) {
        return true;
    } else {
        return false;
    }
}

bool PN53xPowerOff()
{
    WakeUp(&UartNfc);
    Delay(10);
    if (powerDown(&UartNfc, 0x18, 0x00) < 0) {
        return false;
    }
    return true;
}

/**************************************************************************/
/*!
    @brief  Checks the firmware version of the PN5xx chip

    @returns  The chip's firmware version and ID
*/
/**************************************************************************/
int get_firmware_version(nfc *const me, uint32_t *version)
{
    uint8_t *pframe;
    int16_t  status;
    uint32_t response;
    pframe    = pn532_bodybuf;
    pframe[0] = PN532_COMMAND_GETFIRMWAREVERSION;  // CMD
    if (!data_packing(pn532_packetbuf, 2, pframe)) // 2=TFI+CMD(D0)
    {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            2 + 7)) < 0) {
        return status;
    }
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_GETFIRMWAREVERSION, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }

    response = pn532_recvbuf[0];
    response <<= 8;
    response |= pn532_recvbuf[1];
    response <<= 8;
    response |= pn532_recvbuf[2];
    response <<= 8;
    response |= pn532_recvbuf[3];
    *version = response;
    return NFC_SUCCESS;
}

int InCommunicateThru(nfc *const me, uint8_t *pBuf, uint32_t wLen,
                      uint8_t *Rxbuf, uint32_t Rxlen)
{
    uint8_t *pframe;
    int16_t  status;
    pframe    = pn532_bodybuf;
    pframe[0] = PN532_COMMAND_INCOMMUNICATETHRU; // CMD
    for (uint32_t i = 0x00; i < wLen; i++) {
        pframe[i + 1] = *pBuf++;
    }
    if (!data_packing(pn532_packetbuf, wLen + 2, pframe)) // 2=TFI+CMD(D0)
    {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            wLen + 2 + 7)) < 0) {
        return status;
    }
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_INCOMMUNICATETHRU, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }
    return NFC_SUCCESS;
}

int Pn53xSetParamter(nfc *const pnd, uint8_t flag)
{
    uint8_t *pframe;
    int16_t  status;
    pframe    = pn532_bodybuf;
    pframe[0] = PN532_COMMAND_SETPARAMETERS; // CMD
    pframe[1] = flag;
    if (!data_packing(pn532_packetbuf, 3, pframe)) {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(pnd->_interface, pn532_packetbuf,
                                            3 + 7)) < 0) {
        return status;
    }
    if ((status = PN53x_read_response_vcall(
             pnd->_interface, PN532_COMMAND_SETPARAMETERS, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }
    pnd->ui8Parameters = flag;
    return NFC_SUCCESS;
}

int pn53x_set_parameters(nfc *const pnd, const uint8_t ui8Parameter,
                         const bool bEnable)
{
    uint8_t ui8Value = (bEnable) ? (pnd->ui8Parameters | ui8Parameter)
                                 : (pnd->ui8Parameters & ~(ui8Parameter));
    if (ui8Value != pnd->ui8Parameters) {
        return Pn53xSetParamter(pnd, ui8Value);
    }

    return NFC_SUCCESS;
}

void Pn53xWakeUp(void)
{
    WakeUp(&UartNfc);
    Delay(10);
}

int Pn53xGetFirmVersion(uint32_t *version)
{
    int ret = NFC_SUCCESS;

    ret = get_firmware_version(&UartNfc, version);

    return ret;
}

int Pn53xGetRssiValue(uint8_t *rssi)
{
    int ret = false;

    writeRegister(&UartNfc, PN53X_REG_CIU_RFlevelDet, 0x00);
    ret = readRegister(&UartNfc, PN53X_REG_CIU_TestADC, rssi);

    return ret;
}

bool Pn53xAntennaOff(void)
{
    uint8_t reg = 0x00;
    if (readRegister(&UartNfc, PN53X_REG_CIU_TxControl, &reg) < 0) {
        return false;
    }
    reg = reg & 0xfc;

    if (writeRegister(&UartNfc, PN53X_REG_CIU_TxControl, reg) < 0) {
        return false;
    }
    return true;
}

void Pn53xQuickAntennaOn(void) {}

void Pn53xQuickAntennaOff(void) {}

bool Pn53xAntennaOn(void)
{
    uint8_t reg = 0x00;
    if (readRegister(&UartNfc, PN53X_REG_CIU_TxControl, &reg) < 0) {
        return false;
    }

    reg = reg | 0x03;
    if (writeRegister(&UartNfc, PN53X_REG_CIU_TxControl, reg) < 0) {
        return false;
    }
    return true;
}

int Pn53xSetParityAuto(nfc *const me, bool bAuto)
{
    uint8_t regvalue = bAuto ? 0x00 : 0x10;
    int     ret      = 0x00;
    if ((ret = writeRegister(&UartNfc, PN53X_REG_CIU_ManualRCV, regvalue)) < 0) {
        return ret;
    }
    me->bPar = bAuto;
    return NFC_SUCCESS;
}

/* datasheet section 10.3*/
#define IDLE          0x00 // no action, cancels current command exectution
#define MEM           0x01 // store 25 bytes into internal buffer
#define GEN_RAND_ID   0x02 // generates 10-byte random id number
#define CALC_CRC      0x03 // activate on chip CRC calculation or perform self test
#define TRANSMIT      0x04 // transmit data from FIFO buffer
#define NO_CMD_CHANGE 0x07 // modify commandReg without affecting command
#define RECEIVE       0x08 // activate receive cicuit
#define TRANSCEIVE \
    0x0C                // transmit to antenna, then activate receiver after transmission
#define MF_AUTHENT 0x0E // perform mifare authentication on chip
#define SOFT_RESET 0x0F // reset chip

bool Pn53xTransmitBits(uint8_t ByteNumb, uint8_t *data, uint8_t LastBitsNum)
{
    writeRegister(&UartNfc, PN53X_REG_CIU_Command, IDLE); // IDLE

    writeRegister(&UartNfc, PN53X_REG_CIU_FIFOLevel, 0x80); // Flush fifo

    writeRegister(&UartNfc, PN53X_REG_CIU_FIFOLevel, 0x80); // Flush fifo

    for (uint32_t i = 0x00; i < ByteNumb; i++) {
        writeRegister(&UartNfc, PN53X_REG_CIU_FIFOData, data[i]); // Flush fifo
    }

    writeRegister(&UartNfc, PN53X_REG_CIU_BitFraming, LastBitsNum);

    writeRegister(&UartNfc, PN53X_REG_CIU_Command, TRANSMIT);
    uint8_t temp;
    readRegister(&UartNfc, PN53X_REG_CIU_BitFraming, &temp);
    writeRegister(&UartNfc, PN53X_REG_CIU_BitFraming, temp | 0x80);
    return true;
}

int pn53x_wrap_frame(const uint8_t *pbtTx, const uint16_t szTxBits,
                     const uint8_t *pbtTxPar, uint8_t *pbtFrame)
{
    uint8_t  btData;
    uint32_t uiBitPos;
    uint32_t uiDataPos   = 0;
    uint8_t  szBitsLeft  = szTxBits;
    uint8_t  szFrameBits = 0;

    // Make sure we should frame at least something
    if (szBitsLeft == 0)
        return NFC_EINVARG;

    // Handle a short response (1byte) as a special case
    if (szBitsLeft < 9) {
        *pbtFrame   = *pbtTx;
        szFrameBits = szTxBits;
        return szFrameBits;
    }
    // We start by calculating the frame length in bits
    szFrameBits = szTxBits + (szTxBits / 8);

    // Parse the data bytes and add the parity bits
    // This is really a sensitive process, mirror the frame bytes and append
    // parity bits buffer = mirror(frame-byte) + parity + mirror(frame-byte) +
    // parity + ... split "buffer" up in segments of 8 bits again and mirror them
    // air-bytes = mirror(buffer-byte) + mirror(buffer-byte) + mirror(buffer-byte)
    // + ..
    while (true) {
        // Reset the temporary frame byte;
        uint8_t btFrame = 0;

        for (uiBitPos = 0; uiBitPos < 8; uiBitPos++) {
            // Copy as much data that fits in the frame byte
            btData = mirror(pbtTx[uiDataPos]);
            btFrame |= (btData >> uiBitPos);
            // Save this frame byte
            *pbtFrame = mirror(btFrame);
            // Set the remaining bits of the date in the new frame byte and append the
            // parity bit
            btFrame = (btData << (8 - uiBitPos));
            btFrame |= ((pbtTxPar[uiDataPos] & 0x01) << (7 - uiBitPos));
            // Backup the frame bits we have so far
            pbtFrame++;
            *pbtFrame = mirror(btFrame);
            // Increase the data (without parity bit) position
            uiDataPos++;
            // Test if we are done
            if (szBitsLeft < 9)
                return szFrameBits;
            szBitsLeft -= 8;
        }
        // Every 8 data bytes we lose one frame byte to the parities
        pbtFrame++;
    }
}

int pn53x_unwrap_frame(const uint8_t *pbtFrame, const uint16_t szFrameBits,
                       uint8_t *pbtRx, uint8_t *pbtRxPar)
{
    uint8_t  btFrame;
    uint8_t  btData;
    uint8_t  uiBitPos;
    uint32_t uiDataPos   = 0;
    uint8_t *pbtFramePos = (uint8_t *)pbtFrame;
    uint16_t szBitsLeft  = szFrameBits;
    uint16_t szRxBits    = 0;

    // Make sure we should frame at least something
    if (szBitsLeft == 0)
        return NFC_EINVARG;

    // Handle a short response (1byte) as a special case
    if (szBitsLeft < 9) {
        *pbtRx   = *pbtFrame;
        szRxBits = szFrameBits;
        return szRxBits;
    }
    // Calculate the data length in bits
    szRxBits = szFrameBits - (szFrameBits / 9);

    // Parse the frame bytes, remove the parity bits and store them in the parity
    // array This process is the reverse of WrapFrame(), look there for more info
    while (true) {
        for (uiBitPos = 0; uiBitPos < 8; uiBitPos++) {
            btFrame = mirror(pbtFramePos[uiDataPos]);
            btData  = (btFrame << uiBitPos);
            btFrame = mirror(pbtFramePos[uiDataPos + 1]);
            btData |= (btFrame >> (8 - uiBitPos));
            pbtRx[uiDataPos] = mirror(btData);
            if (pbtRxPar != NULL)
                pbtRxPar[uiDataPos] = ((btFrame >> (7 - uiBitPos)) & 0x01);
            // Increase the data (without parity bit) position
            uiDataPos++;
            // Test if we are done
            if (szBitsLeft < 9)
                return szRxBits;
            szBitsLeft -= 9;
        }
        // Every 8 data bytes we lose one frame byte to the parities
        pbtFramePos++;
    }
}

bool Pn53xTransceiveBits(uint8_t ByteNumb, uint8_t *data, uint8_t LastBitsNum,
                         uint8_t *Rcv, uint32_t Rxlen)
{
    writeRegister(&UartNfc, PN53X_REG_CIU_Command, IDLE); // IDLE
    writeRegister(&UartNfc, PN53X_REG_CIU_CommIrq, 0x7f);
    writeRegister(&UartNfc, PN53X_REG_CIU_FIFOLevel, 0x80); // Flush fifo

    writeRegister(&UartNfc, PN53X_REG_CIU_FIFOLevel, 0x80); // Flush fifo

    for (uint32_t i = 0x00; i < ByteNumb; i++) {
        writeRegister(&UartNfc, PN53X_REG_CIU_FIFOData, data[i]); // Flush fifo
    }

    writeRegister(&UartNfc, PN53X_REG_CIU_BitFraming, LastBitsNum);

    writeRegister(&UartNfc, PN53X_REG_CIU_Command, TRANSCEIVE);
    uint8_t temp;
    readRegister(&UartNfc, PN53X_REG_CIU_BitFraming, &temp);
    writeRegister(&UartNfc, PN53X_REG_CIU_BitFraming, temp | 0x80);
    /*Reveive data.*/
    temp = 0x00;
    while (1) {
        readRegister(&UartNfc, PN53X_REG_CIU_CommIrq, &temp);
        if (temp & 0x30) { // idle or receive the end of rx data.
            break;
        }
    }
    uint8_t errstatus = 0x00;
    readRegister(&UartNfc, PN53X_REG_CIU_Error, &errstatus);
    if (errstatus & 0x11) {
        /*protocol err or fifo overflow.*/
        return false;
    }
    uint8_t fifodatalen = 0x00;
    readRegister(&UartNfc, PN53X_REG_CIU_FIFOLevel, &fifodatalen);
    if (Rxlen < fifodatalen) {
        /*rx buf not enough.*/
        return false;
    }
    for (uint32_t i = 0x00; i < fifodatalen; i++) {
        readRegister(&UartNfc, PN53X_REG_CIU_FIFOData, Rcv++);
    }
    return true;
}

int pn53x_set_tx_bits(nfc *const me, uint8_t BitsNum)
{
    int      status;
    uint8_t  cmdbuf[16];
    uint8_t  tempbuf[4];
    uint8_t *pframe = tempbuf;
    pframe[0]       = PN532_COMMAND_WRITEREGISTER;
    pframe[1]       = (PN53X_REG_CIU_BitFraming >> 8) & 0xFF;
    pframe[2]       = PN53X_REG_CIU_BitFraming & 0xFF;
    pframe[3]       = BitsNum;

    if (!data_packing(cmdbuf, 5, pframe)) // 2=TFI+CMD(D0)
    {
        return NFC_EOVFLOW;
    }

    if ((status = PN53x_write_command_vcall(me->_interface, cmdbuf, 5 + 7)) < 0) {
        return status;
    }
    // read data packet
    Delay(5);
    status = PN53x_read_response_vcall(
        me->_interface, PN532_COMMAND_WRITEREGISTER, pn532_recvbuf,
        sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME);
    if (status < 0) {
        return status;
    }

    return NFC_SUCCESS;
}

/**
 * @brief TransceiveBits
 *
 * @param pbtTx point to txbuf
 * @param szTxBits
 * @param pbtTxPar
 * @param pbtRx
 * @param pbtRxPar
 */
int Pn53xInitorTransceiveBits(nfc *const me, const uint8_t *pTx,
                              const uint16_t TxBits, const uint8_t *pTxPar,
                              uint8_t *pRx, uint8_t *pRxPar)
{
    uint16_t FrameBits = 0x00;
    int      ret       = 0x00;
    uint8_t  ui8Bits   = 0;

    uint8_t  pn532_rawbuf[64];
    uint8_t *pframe;
    pframe    = pn532_bodybuf;
    pframe[0] = PN532_COMMAND_INCOMMUNICATETHRU;
    /*Check if we should prepare the parity bits ourself.*/
    if (!me->bPar && (TxBits > 0)) {
        ret = pn53x_wrap_frame(pTx, TxBits, pTxPar, &pframe[1]);
        if (ret < 0) {
            return ret;
        }
        FrameBits = ret;
    } else {
        FrameBits = TxBits;
    }
    ui8Bits = FrameBits % 8;
    // Get the amount of frame bytes + optional (1 byte if there are leading bits)
    uint16_t FrameBytes = (FrameBits / 8) + ((ui8Bits == 0) ? 0 : 1);

    // When the parity is handled before us, we just copy the data
    if (me->bPar) {
        memcpy(&pframe[1], pTx, FrameBytes);
    }
    // Set the amount of transmission bits in the PN53X chip register
    if ((ret = pn53x_set_tx_bits(me, ui8Bits)) < 0) {
        return ret;
    }

    if (!data_packing(pn532_packetbuf, FrameBytes + 2, pframe)) // 2=TFI+CMD(D0)
    {
        return NFC_ESOFT;
    }
    if ((ret = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                         FrameBytes + 2 + 7)) < 0) {
        return NFC_EIO;
    }
    int16_t len;
    if ((len = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_INCOMMUNICATETHRU, pn532_recvbuf,
             sizeof(pn532_recvbuf), 1500)) < 0) {
        return len;
    }
    // Get the last bit-count that is stored in the received byte
    int sta = pn532_recvbuf[0];
    if (sta != 0x00) {
        /*There is error*/
        /*ret error code and lens.*/
        memcpy(pRx, &pn532_recvbuf[0], 1);
        len = 8;
        goto end;
    }
    for (uint32_t i = 0x00; i < len; i++) {
        pn532_rawbuf[i] = pn532_recvbuf[i];
    }
    uint8_t temp;
    if ((ret = readRegister(&UartNfc, PN53X_REG_CIU_Control, &temp)) < 0) {
        return ret;
    }

    ui8Bits = temp & SYMBOL_RX_LAST_BITS;

    // Recover the real frame length in bits
    FrameBits = ((len - 1 - ((ui8Bits == 0) ? 0 : 1)) * 8) + ui8Bits;

    if (pRx != NULL) {
        // Ignore the status byte from the PN53X here, it was checked earlier in
        // pn53x_transceive() Check if we should recover the parity bits ourself
        if (!me->bPar) {
            // Unwrap the response frame
            if ((ret = pn53x_unwrap_frame(&pn532_rawbuf[1], FrameBits, pRx, pRxPar)) <
                0) {
                return ret;
            }
            len = ret;
        } else {
            // Save the received bits
            len = FrameBits;
            // Copy the received bytes
            memcpy(pRx, &pn532_rawbuf[1], len - 1);
        }
    }
end:
    return len;
    /**/
}

bool Pn53xListTarget(NfcTarget_t *target)
{
    bool ret = false;
    do {
        if (readPassiveTargetID(&UartNfc, 0x00, target, 0x00) < 0) {
            ret = false;
            break;
        }
        ret = true;
    } while (0);
    return ret;
}

int setPassiveActivationRetries(nfc *const me, uint8_t MxRtyATR,
                                uint8_t MxRtyPSL, uint8_t maxRetries)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;
    pframe[0] = PN532_COMMAND_RFCONFIGURATION;
    pframe[1] = 5;        // Config item 5 (MaxRetries)
    pframe[2] = MxRtyATR; // MxRtyATR (default = 0xFF)
    pframe[3] = MxRtyPSL; // MxRtyPSL (default = 0x01)
    pframe[4] = maxRetries;
    if (!data_packing(pn532_packetbuf, 6, pframe)) {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            6 + 7)) < 0) {
        return NFC_EIO;
    }
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_RFCONFIGURATION, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }

    return NFC_SUCCESS;
}

int pn53x_RFConfiguration__RF_field(nfc *const me, bool bEnable)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;
    pframe[0] = PN532_COMMAND_RFCONFIGURATION;
    pframe[1] = RFCI_FIELD;
    pframe[2] = bEnable ? 0x01 : 0x00;
    if (!data_packing(pn532_packetbuf, 4, pframe)) {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            4 + 7)) < 0) {
        return NFC_EIO;
    }
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_RFCONFIGURATION, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }

    return NFC_SUCCESS;
}

int Pn53xConfigurationTimeout(nfc *const me, uint8_t fATR_RES_Timeout,
                              uint8_t fRetryTimeout)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;
    pframe[0] = PN532_COMMAND_RFCONFIGURATION;
    pframe[1] = RFCI_TIMING;
    pframe[2] = 0x00; /*RFU*/
    pframe[3] = fATR_RES_Timeout;
    pframe[4] = fRetryTimeout;

    if (!data_packing(pn532_packetbuf, 5 + 1, pframe)) {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            5 + 1 + 7)) < 0) {
        return NFC_EIO;
    }
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_RFCONFIGURATION, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }

    return NFC_SUCCESS;
}

int MifareAuthenticateBlock(nfc *const me, uint8_t keytype, uint8_t *key,
                            uint8_t *uid, uint8_t uidlen, uint8_t block_num)
{
    int      status;
    uint8_t *pframe = pn532_bodybuf; // this buf use too much
    uint8_t *pTemp  = key;
    uint8_t *pUid   = uid;
    pframe[0]       = PN532_COMMAND_INDATAEXCHANGE;
    pframe[1]       = 0x01; // tg max tg num
    pframe[2]       = (keytype) ? MIFARE_CMD_AUTH_A : MIFARE_CMD_AUTH_B;
    pframe[3]       = block_num;
    for (uint32_t i = 0; i < 6; i++) {
        pframe[i + 4] = *pTemp++; // 6 byte key
    }
    for (uint32_t i = 0; i < uidlen; i++) {
        pframe[i + 10] = *pUid++;
    }
    if (!data_packing(pn532_packetbuf, 11 + uidlen, pframe)) // 6 key+uid+7 pack
    {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            11 + 7 + uidlen)) < 0) {
        return NFC_EIO;
    }
    if ((status = PN53x_read_response_vcall(
             me->_interface, PN532_COMMAND_INDATAEXCHANGE, pn532_recvbuf,
             sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME)) < 0) {
        return status;
    }

    if (0x00 != pn532_recvbuf[0]) {
        return NFC_EMFCAUTHFAIL;
    }
    return NFC_SUCCESS;
}

int Pn53xSetPropertyBool(nfc *const pnd, const nfc_property property,
                         const bool bEnable)
{
    uint8_t btValue;
    int     res = 0;
    switch (property) {
    case NP_HANDLE_CRC:
        // Enable or disable automatic receiving/sending of CRC bytes
        if (bEnable == pnd->bCrc) {
            // Nothing to do
            return NFC_SUCCESS;
        }
        // TX and RX are both represented by the symbol 0x80
        btValue = (bEnable) ? 0x80 : 0x00;
        if ((res = Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_TxMode,
                                              SYMBOL_TX_CRC_ENABLE, btValue)) < 0)
            return res;
        if ((res = Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_RxMode,
                                              SYMBOL_RX_CRC_ENABLE, btValue)) < 0)
            return res;
        pnd->bCrc = bEnable;
        return NFC_SUCCESS;

    case NP_HANDLE_PARITY:
        // Handle parity bit by PN53X chip or parse it as data bit
        if (bEnable == pnd->bPar)
            // Nothing to do
            return NFC_SUCCESS;
        btValue = (bEnable) ? 0x00 : SYMBOL_PARITY_DISABLE;
        if ((res = Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_ManualRCV,
                                              SYMBOL_PARITY_DISABLE, btValue)) < 0)
            return res;
        pnd->bPar = bEnable;
        return NFC_SUCCESS;

    case NP_EASY_FRAMING:
        pnd->bEasyFraming = bEnable;
        return NFC_SUCCESS;

    case NP_ACTIVATE_FIELD:
        return pn53x_RFConfiguration__RF_field(pnd, bEnable);

    case NP_ACTIVATE_CRYPTO1:
        btValue = (bEnable) ? SYMBOL_MF_CRYPTO1_ON : 0x00;
        return Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_Status2,
                                          SYMBOL_MF_CRYPTO1_ON, btValue);

    case NP_INFINITE_SELECT:
        // TODO Made some research around this point:
        // timings could be tweak better than this, and maybe we can tweak timings
        // to "gain" a sort-of hardware polling (ie. like PN532 does)
        pnd->bInfiniteSelect = bEnable;
        return setPassiveActivationRetries(
            pnd,
            (bEnable) ? 0xff
                      : 0x00,        // MxRtyATR, default: active = 0xff, passive = 0x02
            (bEnable) ? 0xff : 0x01, // MxRtyPSL, default: 0x01
            (bEnable) ? 0xff : 0x02  // MxRtyPassiveActivation, default: 0xff (0x00
                                     // leads to problems with PN531)
        );

    case NP_ACCEPT_INVALID_FRAMES:
        btValue = (bEnable) ? SYMBOL_RX_NO_ERROR : 0x00;
        return Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_RxMode,
                                          SYMBOL_RX_NO_ERROR, btValue);

    case NP_ACCEPT_MULTIPLE_FRAMES:
        btValue = (bEnable) ? SYMBOL_RX_MULTIPLE : 0x00;
        return Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_RxMode,
                                          SYMBOL_RX_MULTIPLE, btValue);

    case NP_AUTO_ISO14443_4:
        if (bEnable == pnd->bAutoIso14443_4)
            // Nothing to do
            return NFC_SUCCESS;
        pnd->bAutoIso14443_4 = bEnable;
        return pn53x_set_parameters(pnd, PARAM_AUTO_RATS, bEnable);

    case NP_FORCE_ISO14443_A:
        if (!bEnable) {
            // Nothing to do
            return NFC_SUCCESS;
        }
        // Force pn53x to be in ISO14443-A mode
        if ((res = Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_TxMode,
                                              SYMBOL_TX_FRAMING, 0x00)) < 0) {
            return res;
        }
        if ((res = Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_RxMode,
                                              SYMBOL_RX_FRAMING, 0x00)) < 0) {
            return res;
        }
        // Set the PN53X to force 100% ASK Modified miller decoding (default for
        // 14443A cards)
        return Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_TxAuto,
                                          SYMBOL_FORCE_100_ASK, 0x40);

    case NP_FORCE_ISO14443_B:
        if (!bEnable) {
            // Nothing to do
            return NFC_SUCCESS;
        }
        // Force pn53x to be in ISO14443-B mode
        if ((res = Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_TxMode,
                                              SYMBOL_TX_FRAMING, 0x03)) < 0) {
            return res;
        }
        return Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_RxMode,
                                          SYMBOL_RX_FRAMING, 0x03);

    case NP_FORCE_SPEED_106:
        if (!bEnable) {
            // Nothing to do
            return NFC_SUCCESS;
        }
        // Force pn53x to be at 106 kbps
        if ((res = Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_TxMode,
                                              SYMBOL_TX_SPEED, 0x00)) < 0) {
            return res;
        }
        return Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_RxMode,
                                          SYMBOL_RX_SPEED, 0x00);
    // Following properties are invalid (not boolean)
    case NP_TIMEOUT_COMMAND:
    case NP_TIMEOUT_ATR:
    case NP_TIMEOUT_COM:
        return NFC_EINVARG;
    }

    return NFC_EINVARG;
}

int Pn53xResetSettings(nfc *const pnd)
{
    int res = 0;
    // Reset the ending transmission bits register, it is unknown what the last
    // tranmission used there

    if ((res = Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_BitFraming,
                                          SYMBOL_TX_LAST_BITS, 0x00)) < 0) {
        return res;
    }
    // Make sure we reset the CRC and parity to chip handling.
    if ((res = Pn53xSetPropertyBool(pnd, NP_HANDLE_CRC, true)) < 0)
        return res;
    if ((res = Pn53xSetPropertyBool(pnd, NP_HANDLE_PARITY, true)) < 0)
        return res;
    // Activate "easy framing" feature by default
    if ((res = Pn53xSetPropertyBool(pnd, NP_EASY_FRAMING, true)) < 0)
        return res;
    // Deactivate the CRYPTO1 cipher, it may could cause problems when still
    // active
    if ((res = Pn53xSetPropertyBool(pnd, NP_ACTIVATE_CRYPTO1, false)) < 0)
        return res;

    return NFC_SUCCESS;
}

int Pn53xInitiatorInit(nfc *const pnd)
{
    int ret = 0;

    if ((ret = Pn53xResetSettings(pnd)) < 0) {
        return ret;
    }
    if (pnd->sammode != PSM_NORMAL) {
        if ((ret = SAMConfig(pnd, PSM_NORMAL)) < 0) {
            return ret;
        }
        pnd->sammode = PSM_NORMAL;
    }
    return Pn53xWriteRegisterWithMask(pnd, PN53X_REG_CIU_Control,
                                      SYMBOL_INITIATOR, 0x10);
}

void Pn53xInitorResetTag()
{
    /*Drop 13.56M field.*/
    Pn53xQuickAntennaOff();
    Pn53xQuickAntennaOn();
}

bool Pn53xInitorRegisterResetTag(void)
{
    /*Turn off */
    if (writeRegister(&UartNfc, PN53X_REG_CIU_TxControl, 0x80) < 0) {
        return false;
    }
    Delay(500);
    /*Turn on */
    if (writeRegister(&UartNfc, PN53X_REG_CIU_TxControl, 0x83) < 0) {
        return false;
    }
    return true;
}

int MifareWriteBlockEx(nfc *const me, uint8_t blocknum, uint8_t *pData)
{
    uint8_t *pframe = pn532_bodybuf;
    uint8_t *pTemp;
    int16_t  status;
    if (NULL == pData) {
        return NFC_EINVARG;
    }
    pTemp     = pData;
    pframe[0] = PN532_COMMAND_INDATAEXCHANGE;
    pframe[1] = 0x01; // tg
    pframe[2] = MIFARE_CMD_WRITE;
    pframe[3] = blocknum;
    for (uint32_t i = 0; i < 16; i++) {
        pframe[i + 4] = *pTemp++;
    }
    if (!data_packing(pn532_packetbuf, 4 + 1 + 16, pframe)) {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            4 + 1 + 16 + 7)) < 0) {
        return NFC_EIO;
    }

    status = PN53x_read_response_vcall(
        me->_interface, PN532_COMMAND_INDATAEXCHANGE, pn532_recvbuf,
        sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME);
    if (status < 0) {
        return status;
    }
    if (0x00 != pn532_recvbuf[0]) {
        return NFC_ERFTRANS;
    }
    return NFC_SUCCESS;
}

int MifareReadBlock(nfc *const me, uint8_t blocknum, uint8_t *pData)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;
    if (NULL == pData) {
        return NFC_EINVARG;
    }
    pframe[0] = PN532_COMMAND_INDATAEXCHANGE;
    pframe[1] = 0x01; // tg
    pframe[2] = MIFARE_CMD_READ;
    pframe[3] = blocknum;
    if (!data_packing(pn532_packetbuf, 4 + 1, pframe)) {
        return NFC_ESOFT;
    }
    if ((status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                            4 + 1 + 7)) < 0) {
        return NFC_EIO;
    }

    status = PN53x_read_response_vcall(
        me->_interface, PN532_COMMAND_INDATAEXCHANGE, pn532_recvbuf,
        sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME);
    if (status < 0) {
        return status;
    }
    if (0x00 != pn532_recvbuf[0]) {
        return NFC_ERFTRANS;
    }
    memcpy(pData, &pn532_recvbuf[1], 16);
    return NFC_SUCCESS;
}

static int Pn53xDecodeStatus(nfc *const me, uint8_t status)
{
    me->last_error = status;

    switch (status & 0x3F) {
    case 0x00:
        return NFC_SUCCESS;
    case ERFTIMEOUT:
        return NFC_ETIMEOUT;
    case EDEPINVSTATE:
        return NFC_EPROTOCOL;
    case ETGREL:
        return NFC_ETGRELEASED;
    default:
        return NFC_ERFTRANS;
    }
}

int InJumpForDEP(nfc *const me)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;

    if (me == NULL) {
        return NFC_EINVARG;
    }

    me->last_error = 0;
    pframe[0] = PN532_COMMAND_INJUMPFORDEP;
    pframe[1] = P2P_PASSIVE_MODE;
    pframe[2] = P2P_BAUD_106;
    pframe[3] = 0x00; // No optional parameters follow.
    if (!data_packing(pn532_packetbuf, 5, pframe)) {
        return NFC_ESOFT;
    }
    status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf, 12);
    if (status < 0) {
        return status;
    }
    status = PN53x_read_response_vcall(me->_interface, PN532_COMMAND_INJUMPFORDEP,
                                       pn532_recvbuf,
                                       sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME);
    if (status < 0) {
        return status;
    }
    if (status < 1) {
        return NFC_EINVRESPOND;
    }
    return Pn53xDecodeStatus(me, pn532_recvbuf[0]);
}

int P2PInitiatorTxRx(nfc *const me, uint8_t *tx, uint32_t txLen,
                      uint8_t *rx, uint32_t rxCapacity, uint8_t *rxLen)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;
    uint32_t length;
    int      result;

    if (me == NULL || rx == NULL || rxLen == NULL ||
        (tx == NULL && txLen > 0)) {
        return NFC_EINVARG;
    }
    *rxLen = 0;
    if (txLen > sizeof(pn532_bodybuf) - 2 ||
        txLen + 3 > sizeof(pn532_packetbuf) - 7) {
        return NFC_EOVFLOW;
    }

    me->last_error = 0;
    pframe[0] = PN532_COMMAND_INDATAEXCHANGE;
    pframe[1] = 0x01; // Logical target number.
    if (txLen > 0) {
        memcpy(pframe + 2, tx, txLen);
    }

    if (!data_packing(pn532_packetbuf, txLen + 3, pframe)) {
        return NFC_ESOFT;
    }
    status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                       txLen + 10);
    if (status < 0) {
        return status;
    }

    status = PN53x_read_response_vcall(me->_interface, PN532_COMMAND_INDATAEXCHANGE,
                                       pn532_recvbuf,
                                       sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME);
    if (status < 0) {
        return status;
    }

    if (status < 1) {
        return NFC_EINVRESPOND;
    }
    result = Pn53xDecodeStatus(me, pn532_recvbuf[0]);
    if (result < 0) {
        return result;
    }

    length = status - 1;
    if (length > rxCapacity) {
        return NFC_EOVFLOW;
    }
    if (length > 0) {
        memcpy(rx, &pn532_recvbuf[1], length);
    }
    *rxLen = length;
    return NFC_SUCCESS;
}

int P2PTargetTxRx(nfc *const me, uint8_t *tx, uint32_t txLen,
                   uint8_t *rx, uint32_t rxCapacity, uint8_t *rxLen)
{
    int length;

    if (me == NULL || rx == NULL || rxLen == NULL ||
        (tx == NULL && txLen > 0)) {
        return NFC_EINVARG;
    }
    *rxLen = 0;
    length = tgGetData(me, rx, rxCapacity);
    if (length < 0) {
        return length;
    }
    *rxLen = length;
    if (tx == NULL || txLen == 0) {
        return NFC_SUCCESS;
    }
    return tgSetData(me, tx, txLen);
}

int P2PInitiatorInit(nfc *const me, uint8_t activeMode, uint8_t baudRate)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;

    if (me == NULL || activeMode > P2P_ACTIVE_MODE ||
        baudRate > P2P_BAUD_424) {
        return NFC_EINVARG;
    }

    pframe[0] = PN532_COMMAND_INJUMPFORDEP;
    pframe[1] = activeMode; // 0: passive, 1: active.
    pframe[2] = baudRate;   // 0/1/2: 106/212/424 kbps.
    pframe[3] = 0x00;       // No optional parameters follow.

    if (!data_packing(pn532_packetbuf, 5, pframe)) {
        return NFC_ESOFT;
    }
    if (!me->initiator_init_pending) {
        me->last_error = 0;
        status = PN53x_write_command_vcall(me->_interface,
                                           pn532_packetbuf, 12);
        if (status < 0) {
            me->target_init_pending = false;
            return status;
        }
        me->initiator_init_pending = true;
        return NFC_WAIT;
    }

    status = PN53x_read_response_vcall(me->_interface, PN532_COMMAND_INJUMPFORDEP,
                                       pn532_recvbuf,
                                       sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME);

    if (status == NFC_ETIMEOUT) {
        return NFC_WAIT;
    }
    if (status < 0) {
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        return status;
    }
    me->initiator_init_pending = false;
    if (status < 1) {
        return NFC_EINVRESPOND;
    }
    return Pn53xDecodeStatus(me, pn532_recvbuf[0]);
}

int P2PTargetInit(nfc *const me)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;

    if (me == NULL) {
        return NFC_EINVARG;
    }

    pframe[0] = PN532_COMMAND_TGINITASTARGET;
    /* DEP only; accept passive and active activation. */
    pframe[1] = 0x02;

    /** SENS_RES */
    pframe[2] = 0x04;
    pframe[3] = 0x00;

    /** NFCID1 */
    pframe[4] = 0x12;
    pframe[5] = 0x34;
    pframe[6] = 0x56;

    /** SEL_RES */
    pframe[7] = 0x40; // DEP only mode

    /**Parameters to build POL_RES (18 bytes including system code) */
    pframe[8]  = 0x01;
    pframe[9]  = 0xFE;
    pframe[10] = 0xA2;
    pframe[11] = 0xA3;
    pframe[12] = 0xA4;
    pframe[13] = 0xA5;
    pframe[14] = 0xA6;
    pframe[15] = 0xA7;
    pframe[16] = 0xC0;
    pframe[17] = 0xC1;
    pframe[18] = 0xC2;
    pframe[19] = 0xC3;
    pframe[20] = 0xC4;
    pframe[21] = 0xC5;
    pframe[22] = 0xC6;
    pframe[23] = 0xC7;
    pframe[24] = 0xFF;
    pframe[25] = 0xFF;
    /** NFCID3t */
    pframe[26] = 0xAA;
    pframe[27] = 0x99;
    pframe[28] = 0x88;
    pframe[29] = 0x77;
    pframe[30] = 0x66;
    pframe[31] = 0x55;
    pframe[32] = 0x44;
    pframe[33] = 0x33;
    pframe[34] = 0x22;
    pframe[35] = 0x11;
    /** Length of general bytes  */
    pframe[36] = 0x00;
    /** Length of historical bytes  */
    pframe[37] = 0x00;

    if (!data_packing(pn532_packetbuf, 38 + 1, pframe)) {
        return NFC_ESOFT;
    }

    if (!me->target_init_pending) {
        me->last_error     = 0;
        me->activated_mode = 0;
        status = PN53x_write_command_vcall(me->_interface,
                                           pn532_packetbuf, 46);
        if (status < 0) {
            me->initiator_init_pending = false;
            return status;
        }
        me->target_init_pending = true;
        return NFC_WAIT;
    }

    status = PN53x_read_response_vcall(me->_interface, PN532_COMMAND_TGINITASTARGET,
                                       pn532_recvbuf,
                                       sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME);

    if (status == NFC_ETIMEOUT) {
        return NFC_WAIT;
    }

    if (status < 0) {
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        me->activated_mode         = 0;
        return status;
    }
    me->target_init_pending = false;
    if (status < 1) {
        return NFC_EINVRESPOND;
    }
    me->activated_mode = pn532_recvbuf[0];
    if ((me->activated_mode & PN532_ACTIVATED_MODE_DEP) == 0) {
        return NFC_EPROTOCOL;
    }
    return NFC_SUCCESS;
}

int tgSetData(nfc *const me, uint8_t *pData, uint32_t wLen)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;
    int      result;

    if (me == NULL || pData == NULL) {
        return NFC_EINVARG;
    }
    if ((me->activated_mode & PN532_ACTIVATED_MODE_DEP) == 0) {
        return NFC_EPROTOCOL;
    }
    if (wLen > sizeof(pn532_bodybuf) - 1 ||
        wLen + 2 > sizeof(pn532_packetbuf) - 7) {
        return NFC_EOVFLOW;
    }

    me->last_error = 0;
    pframe[0] = PN532_COMMAND_TGSETDATA;
    memcpy(&pframe[1], pData, wLen);
    if (!data_packing(pn532_packetbuf, wLen + 2, pframe)) {
        return NFC_ESOFT;
    }
    status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf,
                                       wLen + 9);
    if (status < 0) {
        me->activated_mode         = 0;
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        return status;
    }
    status = PN53x_read_response_vcall(me->_interface, PN532_COMMAND_TGSETDATA,
                                       pn532_recvbuf,
                                       sizeof(pn532_recvbuf), PN532_FRAME_DEFAULT_WAIT_TIME);
    if (status < 0) {
        me->activated_mode         = 0;
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        return status;
    }
    if (status < 1) {
        me->activated_mode         = 0;
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        return NFC_EINVRESPOND;
    }

    result = Pn53xDecodeStatus(me, pn532_recvbuf[0]);
    if (result < 0) {
        me->activated_mode         = 0;
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
    }
    return result;
}

int tgGetData(nfc *const me, uint8_t *pBuf, uint32_t capacity)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;
    uint32_t length;
    int      result;

    if (me == NULL || pBuf == NULL) {
        return NFC_EINVARG;
    }
    if ((me->activated_mode & PN532_ACTIVATED_MODE_DEP) == 0) {
        return NFC_EPROTOCOL;
    }

    me->last_error = 0;
    pframe[0]       = PN532_COMMAND_TGGETDATA;
    if (!data_packing(pn532_packetbuf, 2, pframe)) {
        return NFC_ESOFT;
    }
    status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf, 9);
    if (status < 0) {
        me->activated_mode         = 0;
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        return status;
    }
    status = PN53x_read_response_vcall(me->_interface,
                                       PN532_COMMAND_TGGETDATA,
                                       pn532_recvbuf, sizeof(pn532_recvbuf),
                                       PN532_FRAME_DEFAULT_WAIT_TIME);
    if (status < 0) {
        me->activated_mode         = 0;
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        return status;
    }
    if (status < 1) {
        me->activated_mode         = 0;
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        return NFC_EINVRESPOND;
    }

    result = Pn53xDecodeStatus(me, pn532_recvbuf[0]);
    if (result < 0) {
        me->activated_mode         = 0;
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        return result;
    }

    length = status - 1;
    if (length > capacity) {
        me->activated_mode         = 0;
        me->initiator_init_pending = false;
        me->target_init_pending    = false;
        return NFC_EOVFLOW;
    }
    if (length > 0) {
        memcpy(pBuf, &pn532_recvbuf[1], length);
    }
    return length;
}

int P2PInitiatorRelease(nfc *const me)
{
    uint8_t *pframe = pn532_bodybuf;
    int16_t  status;

    if (me == NULL) {
        return NFC_EINVARG;
    }

    me->last_error = 0;
    pframe[0] = PN532_COMMAND_INRELEASE;
    pframe[1] = 0x01; // Release logical target 1.
    if (!data_packing(pn532_packetbuf, 3, pframe)) {
        return NFC_ESOFT;
    }
    status = PN53x_write_command_vcall(me->_interface, pn532_packetbuf, 10);
    if (status < 0) {
        return status;
    }

    status = PN53x_read_response_vcall(me->_interface,
                                       PN532_COMMAND_INRELEASE,
                                       pn532_recvbuf, sizeof(pn532_recvbuf),
                                       PN532_FRAME_DEFAULT_WAIT_TIME);
    if (status < 0) {
        return status;
    }
    if (status < 1) {
        return NFC_EINVRESPOND;
    }
    return Pn53xDecodeStatus(me, pn532_recvbuf[0]);
}
