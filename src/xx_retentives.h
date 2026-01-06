#ifndef XX_RETENTIVES_H
#define XX_RETENTIVES_H

#include <stdint.h>

extern uint32_t *xx_retentives_ptr;
void xx_retentives_init(int size);
void xx_retentives_sync();
void xx_retentives_dump();

#endif // XX_RETENTIVES_H
