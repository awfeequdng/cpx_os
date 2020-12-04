#ifndef __KERNEL_DRIVER_PIC_IRQ_H__
#define __KERNEL_DRIVER_PIC_IRQ_H__

void pic_init(void);
void pic_enable(unsigned int irq);

#endif // __KERNEL_DRIVER_PIC_IRQ_H__
