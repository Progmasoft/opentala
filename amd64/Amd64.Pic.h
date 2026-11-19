/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

/**
 * @brief Sets up the two 8259 PICs: IRQ 0 to 15 at vectors AMD64_TRAP_IRQ_BASE to +15, every IRQ
 * masked.
 *
 * Their default vectors, 8 to 15, are those of processor exceptions, so a timer interrupt would
 * look like a double fault; they are moved before interrupts are turned on.
 */
void
Amd64_Pic_Init(void);

/**
 * @brief Lets IRQ aIrq through, or masks it again.
 *
 * @param[in] aIrq      The IRQ, 0 to 15.
 * @param[in] isEnabled Whether it gets through.
 */
void
Amd64_Pic_SetEnabled(unsigned aIrq, bool isEnabled);

/**
 * @brief Whether IRQ aIrq, which just arrived, is a spurious IRQ 7 or 15: one the PIC raised
 * without a device asking, and which must not be acknowledged.
 *
 * @param[in] aIrq The IRQ.
 * @return true for a spurious IRQ.
 */
bool
Amd64_Pic_IsSpurious(unsigned aIrq);

/**
 * @brief Tells the PICs that IRQ aIrq was handled (end of interrupt).
 *
 * @param[in] aIrq The IRQ.
 */
void
Amd64_Pic_Acknowledge(unsigned aIrq);
