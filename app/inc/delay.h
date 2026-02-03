#ifndef __DELAY_H
#define __DELAY_H

static void delay(void)
{
    for (unsigned int a=0;a<1000;a++)
    {
        for (unsigned int b=0;b<1000;b++)
        {
            __NOP();
            __NOP();
            __NOP();
            __NOP();
        }
    }
}

#endif 
