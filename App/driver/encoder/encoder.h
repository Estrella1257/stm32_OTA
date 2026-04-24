#ifndef ENCODER_H
#define ENCODER_H

#include "stm32f4xx.h"

void Encoder_Init(void);
int32_t Encoder_Get_RawCount(void);
float Encoder_Get_Velocity_RPM(float dt);

#endif