/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/*
 * The interrupt controller and the timer on arm64: a GICv2 as QEMU's virt machine has it with
 * gic-version=2 (distributor at 0x08000000, CPU interface at 0x08010000), and the EL1 physical
 * timer of the generic timer, private peripheral interrupt 30.
 */

/**
 * @brief Turns on the distributor and the CPU interface, and the timer's interrupt at
 * KERN_MACHINE_TIMER_HZ.
 */
void
Arm64_Gic_StartTimer(void);

/**
 * @brief Takes the pending interrupt from the CPU interface, handles it (the timer counts a tick
 * and is set for the next) and signals its end. A spurious interrupt is not acknowledged.
 */
void
Arm64_Gic_HandleInterrupt(void);

/**
 * @brief How many times the timer has ticked.
 *
 * @return The count.
 */
uint64_t
Arm64_Gic_Ticks(void);
