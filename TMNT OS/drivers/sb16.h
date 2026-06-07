// drivers/sb16.h
#ifndef SB16_H
#define SB16_H
#include "../system/types.h"

void sb16_init(void);
void sb16_play_pcm(uint8_t* data, uint32_t size, uint16_t sample_rate);
void sb16_play_pcm_single(uint8_t* data, uint32_t size, uint16_t sample_rate);
void sb16_stop(void);
int  sb16_is_playing(void);

#endif