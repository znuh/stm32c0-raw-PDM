#ifndef LOWLEVEL_H
#define LOWLEVEL_H

#include <stdint.h>

/* delay_loop: 20.83ns * 3 * n */
void __attribute__((long_call)) delay_loop(uint32_t n);

#endif // LOWLEVEL_H
