/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/** How many times a second the timer interrupts. */
#define AMD64_TIMER_HZ 100

/**
 * @brief Starts the programmable interval timer (8253/8254), channel 0, at AMD64_TIMER_HZ on IRQ 0.
 *
 * @pre Amd64_Pic_Init() and Amd64_Cpu_Init() have run; interrupts are turned on by the caller.
 */
void
Amd64_Timer_Init(void);

/**
 * @brief How many times the timer interrupted since Amd64_Timer_Init().
 *
 * @return The ticks.
 */
uint64_t
Amd64_Timer_Ticks(void);
