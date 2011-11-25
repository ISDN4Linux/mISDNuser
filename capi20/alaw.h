#ifndef _ALAW_H
#define _ALAW_H
#include <stdint.h>

signed char lin2alaw[65536];	// 16bit unsigned index
signed char *slin2alaw;		// 16bit signed index
uint16_t alaw2lin[256];		// alaw -> 16bit PCM, Mono, 8000 hz

void create_lin2alaw_table(void);
#endif
