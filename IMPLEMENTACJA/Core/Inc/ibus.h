/*
 * ibus.h
 *
 *  Created on: Dec 9, 2025
 *      Author: filip
 */

#ifndef IBUS_H
#define IBUS_H

#include "stdint.h"
#include "stdbool.h"

void ibus_init(void);

// kanały numerujemy 0..13 (CH1 = 0, CH2 = 1, ...)
uint16_t ibus_read_channel(uint8_t ch);

// czy mamy świeży sygnał z odbiornika
bool ibus_is_signal_valid(void);

#endif // IBUS_H
