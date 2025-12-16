#ifndef MOTORS_H
#define MOTORS_H

#include "stdint.h"
#include "stm32l4xx_hal.h"

// -----------------------------------------
// KONFIGURACJA
// -----------------------------------------

// Minimalny i maksymalny sygnał dla ESC (w mikrosekundach)
#define MOTOR_MIN_US   1000
#define MOTOR_MAX_US   2000


// -----------------------------------------
// FUNKCJE PUBLICZNE
// -----------------------------------------

// Inicjalizacja modułu silników (opcjonalnie, jeśli chcesz dodać coś na starcie)
void motors_arm(void);

// Zatrzymanie wszystkich silników
void motors_stop_all(void);

// Ustawienie konkretnego silnika w mikrosekundach (1000–2000)
void set_motor_us(uint8_t motor_id, uint16_t us);

// Miksowanie PID + throttle → wyjścia PWM na 4 silniki
void mixer_update(float u_roll, float u_pitch, float u_yaw, uint16_t throttle_us);

void esc_calibrate_all(void);


//

#endif // MOTORS_H
