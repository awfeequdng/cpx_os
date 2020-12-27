#ifndef __KERNEL_DRIVER_CLOCK_H__
#define __KERNEL_DRIVER_CLOCK_H__

#include <types.h>

void set_ticks(size_t tick);
size_t get_ticks(void);
void clock_init(void);


#endif //__KERNEL_DRIVER_CLOCK_H__