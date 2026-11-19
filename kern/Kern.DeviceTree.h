/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kern/Kern.Boot.h"

/*
 * The flattened device tree (Devicetree Specification 0.4, chapter 5), as an arm64 loader passes
 * it. The kernel reads three things from it: the RAM of the /memory nodes, the memory reservation
 * block, and /chosen's bootargs. Every offset and length in the blob is checked against the blob
 * before it is read, since the loader's data is not trusted to be well formed.
 */

/**
 * @brief Reads the RAM, the reserved ranges (the blob itself among them) and the command line.
 *
 * @param[in]     aIn_pBlob          The blob, at its physical address.
 * @param[in,out] aInOut_pInformation Where the ranges and the command line go.
 * @return false when the blob is not a device tree of version 17 or is malformed; nothing is
 *         trusted from it then.
 */
bool
Kern_DeviceTree_Read(const void* aIn_pBlob, Kern_Boot_tInformation* aInOut_pInformation);
