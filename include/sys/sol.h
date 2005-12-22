
#ifndef _SYS_SOL_H
#define _SYS_SOL_H

#include <mach/coil.h>

typedef uint8_t solnum_t;

void sol_rtt (void);
void sol_on (solnum_t sol);
void sol_off (solnum_t sol);
void sol_pulse (solnum_t sol);
void sol_serve (void);
void sol_init (void);

void flasher_pulse (solnum_t n);
void flasher_rtt (void);
void flasher_init (void);

#endif /* _SYS_SOL_H */
