#include <stdbool.h>
#include <stdint.h>
#include "PN53xInterface.h"

int16_t PN53x_read_response(uint8_t command, uint8_t *pbuf, uint32_t wlen, uint32_t timeoutMs);
int     PN53x_write_command(uint8_t *pBuf, uint32_t wLen);
bool    PN53x_wakeup(void);
void    PN53x_SendAck(void);

void PN53x_Interface_ctor(PN53x_Interface *const me, uint32_t cnt)
{
    static const struct InterfaceVtable Vtable = {
        &PN53x_read_response,
        &PN53x_write_command,
        &PN53x_wakeup,
        &PN53x_SendAck,
    };
    me->rtycnt = cnt;
    me->vptr   = &Vtable;
}

int16_t PN53x_read_response(uint8_t command, uint8_t *pbuf, uint32_t wlen, uint32_t timeoutMs)
{
    (void)(command);
    (void)(pbuf);
    (void)(wlen);
    (void)(timeoutMs);
    return -1;
}

int PN53x_write_command(uint8_t *pBuf, uint32_t wLen)
{
    (void)(pBuf);
    (void)(wLen);
    return -1;
}
bool PN53x_wakeup(void)
{
    return false;
}

void PN53x_SendAck(void)
{
    return;
}
