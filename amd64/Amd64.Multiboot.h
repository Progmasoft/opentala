/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/** The value a Multiboot loader leaves in EAX. */
#define AMD64_MULTIBOOT_LOADER_MAGIC 0x2BADB002U

/** Flags of the information structure: which of its fields are valid. */
#define AMD64_MULTIBOOT_HAS_COMMAND_LINE 0x004U
#define AMD64_MULTIBOOT_HAS_MEMORY_MAP   0x040U

/** The type of a memory map entry that is free RAM. */
#define AMD64_MULTIBOOT_MEMORY_AVAILABLE 1

/** The start of the information structure a Multiboot loader passes in EBX. */
typedef struct __attribute__((packed))
{
        uint32_t flags;
        uint32_t memoryLowKiB;
        uint32_t memoryHighKiB;
        uint32_t bootDevice;
        uint32_t commandLine;
        uint32_t moduleCount;
        uint32_t moduleAddress;
        uint32_t symbols[ 4 ];
        uint32_t memoryMapLength;
        uint32_t memoryMapAddress;
} Amd64_Multiboot_Information;

/**
 * @brief One entry of the memory map. size is the size of the rest of the entry, so the next one
 * starts size + 4 bytes after this one.
 */
typedef struct __attribute__((packed))
{
        uint32_t size;
        uint64_t address;
        uint64_t length;
        uint32_t type;
} Amd64_Multiboot_MemoryRange;
