#ifndef PIT_H
#define PIT_H

#include "../system/types.h"

void pit_init(void);
void pit_ticks_delay(uint32_t ticks);
void delay_us(uint32_t us);

#endif