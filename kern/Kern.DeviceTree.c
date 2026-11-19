/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kern/Kern.Boot.h"
#include "kern/Kern.DeviceTree.h"

#define KERN_DEVICE_TREE_MAGIC      0xD00DFEEDU
#define KERN_DEVICE_TREE_VERSION    17U
#define KERN_DEVICE_TREE_BEGIN_NODE 1U
#define KERN_DEVICE_TREE_END_NODE   2U
#define KERN_DEVICE_TREE_PROPERTY   3U
#define KERN_DEVICE_TREE_NOP        4U
#define KERN_DEVICE_TREE_END        9U
/** The largest blob the kernel reads: far over what a loader passes, under what could wrap. */
#define KERN_DEVICE_TREE_MAX_SIZE (16U * 1024U * 1024U)
#define KERN_DEVICE_TREE_HEADER   40U

/** The blob and its two blocks, checked to lie inside it. */
typedef struct
{
        const uint8_t* pBlob;
        uint32_t       size;
        uint32_t       structOffset;
        uint32_t       structSize;
        uint32_t       stringsOffset;
        uint32_t       stringsSize;
} Tree;

/** Where the walk of the structure block is. */
typedef struct
{
        uint32_t offset;
        uint32_t depth;
        bool     isMemory;
        bool     isChosen;
        uint32_t addressCells;
        uint32_t sizeCells;
} Walk;

static uint32_t
Be32(const uint8_t* aIn_pBytes)
{
        return ((uint32_t) aIn_pBytes[ 0 ] << 24) | ((uint32_t) aIn_pBytes[ 1 ] << 16)
                | ((uint32_t) aIn_pBytes[ 2 ] << 8) | aIn_pBytes[ 3 ];
}

static uint64_t
Be64(const uint8_t* aIn_pBytes)
{
        return ((uint64_t) Be32(aIn_pBytes) << 32) | Be32(aIn_pBytes + 4);
}

static uint32_t
Align4(uint32_t aOffset)
{
        return (aOffset + 3U) & ~3U;
}

/** Whether [aOffset, aOffset + aLength) lies inside a block of aSize bytes. */
static bool
IsInside(uint32_t aOffset, uint32_t aLength, uint32_t aSize)
{
        return (aOffset <= aSize && aLength <= aSize - aOffset);
}

static bool
ReadHeader(const uint8_t* aIn_pBlob, Tree* aOut_pTree)
{
        const uint32_t size = Be32(aIn_pBlob + 4);

        if(Be32(aIn_pBlob) != KERN_DEVICE_TREE_MAGIC || size < KERN_DEVICE_TREE_HEADER
           || size > KERN_DEVICE_TREE_MAX_SIZE || Be32(aIn_pBlob + 20) < KERN_DEVICE_TREE_VERSION
           || Be32(aIn_pBlob + 24) > KERN_DEVICE_TREE_VERSION)
                return (false);
        aOut_pTree->pBlob         = aIn_pBlob;
        aOut_pTree->size          = size;
        aOut_pTree->structOffset  = Be32(aIn_pBlob + 8);
        aOut_pTree->stringsOffset = Be32(aIn_pBlob + 12);
        aOut_pTree->stringsSize   = Be32(aIn_pBlob + 32);
        aOut_pTree->structSize    = Be32(aIn_pBlob + 36);
        return (aOut_pTree->structOffset % 4 == 0 && Be32(aIn_pBlob + 16) % 8 == 0
                && IsInside(aOut_pTree->structOffset, aOut_pTree->structSize, size)
                && IsInside(aOut_pTree->stringsOffset, aOut_pTree->stringsSize, size)
                && IsInside(Be32(aIn_pBlob + 16), 16, size));
}

/** The memory reservation block: pairs of 64-bit address and size, ended by a pair of zeros. */
static bool
ReadReservations(const Tree* aIn_pTree, Kern_Boot_tInformation* aInOut_pInformation)
{
        for(uint32_t offset = Be32(aIn_pTree->pBlob + 16); IsInside(offset, 16, aIn_pTree->size);
            offset         += 16)
        {
                const uint64_t start  = Be64(aIn_pTree->pBlob + offset);
                const uint64_t length = Be64(aIn_pTree->pBlob + offset + 8);

                if(start == 0 && length == 0)
                        return (true);
                Kern_Boot_AddRange(aInOut_pInformation->reserved,
                                   &aInOut_pInformation->reservedCount,
                                   start,
                                   length);
        }
        return (false);
}

/** A string that ends inside its block, or NULL. */
static const char*
StringAt(const uint8_t* aIn_pBlock, uint32_t aOffset, uint32_t aSize)
{
        for(uint32_t index = aOffset; index < aSize; index++)
                if(aIn_pBlock[ index ] == '\0')
                        return ((const char*) aIn_pBlock + aOffset);
        return (NULL);
}

static bool
IsSame(const char* aIn_pLeft, const char* aIn_pRight)
{
        while(*aIn_pLeft != '\0' && *aIn_pLeft == *aIn_pRight)
        {
                aIn_pLeft++;
                aIn_pRight++;
        }
        return (*aIn_pLeft == *aIn_pRight);
}

/** Whether a node name is aIn_pName, with or without a unit address. */
static bool
IsNode(const char* aIn_pNode, const char* aIn_pName)
{
        while(*aIn_pName != '\0' && *aIn_pNode == *aIn_pName)
        {
                aIn_pNode++;
                aIn_pName++;
        }
        return (*aIn_pName == '\0' && (*aIn_pNode == '\0' || *aIn_pNode == '@'));
}

/** One cell count, 1 or 2, of the root's #address-cells or #size-cells. */
static bool
ReadCells(const uint8_t* aIn_pValue, uint32_t aLength, uint32_t* aOut_pCells)
{
        if(aLength != 4 || Be32(aIn_pValue) < 1 || Be32(aIn_pValue) > 2)
                return (false);
        *aOut_pCells = Be32(aIn_pValue);
        return (true);
}

/** /memory's reg: pairs of an address and a size, each of the root's cell counts. */
static bool
ReadRegions(const Walk*             aIn_pWalk,
            const uint8_t*          aIn_pValue,
            uint32_t                aLength,
            Kern_Boot_tInformation* aInOut_pInformation)
{
        const uint32_t entry = 4 * (aIn_pWalk->addressCells + aIn_pWalk->sizeCells);

        if(aLength % entry != 0)
                return (false);
        for(uint32_t offset = 0; offset < aLength; offset += entry)
        {
                const uint8_t* pAddress = aIn_pValue + offset;
                const uint8_t* pSize    = pAddress + 4 * aIn_pWalk->addressCells;

                Kern_Boot_AddRange(aInOut_pInformation->memory,
                                   &aInOut_pInformation->memoryCount,
                                   aIn_pWalk->addressCells == 2 ? Be64(pAddress) : Be32(pAddress),
                                   aIn_pWalk->sizeCells == 2 ? Be64(pSize) : Be32(pSize));
        }
        return (true);
}

/** /chosen's bootargs, used in place, so it must end inside the property. */
static bool
ReadCommandLine(const uint8_t*          aIn_pValue,
                uint32_t                aLength,
                Kern_Boot_tInformation* aInOut_pInformation)
{
        if(aLength == 0 || aIn_pValue[ aLength - 1 ] != '\0')
                return (false);
        aInOut_pInformation->pCommandLine = (const char*) aIn_pValue;
        return (true);
}

/** What one property of the root says: its cell counts. */
static bool
ReadRootProperty(Walk*          aInOut_pWalk,
                 const char*    aIn_pName,
                 const uint8_t* aIn_pValue,
                 uint32_t       aLength)
{
        if(IsSame(aIn_pName, "#address-cells"))
                return (ReadCells(aIn_pValue, aLength, &aInOut_pWalk->addressCells));
        if(IsSame(aIn_pName, "#size-cells"))
                return (ReadCells(aIn_pValue, aLength, &aInOut_pWalk->sizeCells));
        return (true);
}

/** What one property says, for the root, /memory and /chosen. */
static bool
ReadProperty(Walk*                   aInOut_pWalk,
             const char*             aIn_pName,
             const uint8_t*          aIn_pValue,
             uint32_t                aLength,
             Kern_Boot_tInformation* aInOut_pInformation)
{
        if(aInOut_pWalk->depth == 1)
                return (ReadRootProperty(aInOut_pWalk, aIn_pName, aIn_pValue, aLength));
        if(aInOut_pWalk->depth != 2)
                return (true);
        if(aInOut_pWalk->isMemory && IsSame(aIn_pName, "reg"))
                return (ReadRegions(aInOut_pWalk, aIn_pValue, aLength, aInOut_pInformation));
        if(aInOut_pWalk->isChosen && IsSame(aIn_pName, "bootargs"))
                return (ReadCommandLine(aIn_pValue, aLength, aInOut_pInformation));
        return (true);
}

/** Reads a property token at the walk's offset and moves past it. */
static bool
StepProperty(const Tree* aIn_pTree, Walk* aInOut_pWalk, Kern_Boot_tInformation* aInOut_pInformation)
{
        const uint8_t* pBlock = aIn_pTree->pBlob + aIn_pTree->structOffset;

        if(!IsInside(aInOut_pWalk->offset, 12, aIn_pTree->structSize))
                return (false);
        const uint32_t length = Be32(pBlock + aInOut_pWalk->offset + 4);
        const uint32_t value  = aInOut_pWalk->offset + 12;
        const char*    pName  = StringAt(aIn_pTree->pBlob + aIn_pTree->stringsOffset,
                                         Be32(pBlock + aInOut_pWalk->offset + 8),
                                         aIn_pTree->stringsSize);

        if(pName == NULL || !IsInside(value, length, aIn_pTree->structSize))
                return (false);
        aInOut_pWalk->offset = Align4(value + length);
        return (ReadProperty(aInOut_pWalk, pName, pBlock + value, length, aInOut_pInformation));
}

/** Reads a node's start at the walk's offset and moves past its name. */
static bool
StepNode(const Tree* aIn_pTree, Walk* aInOut_pWalk)
{
        const uint8_t* pBlock = aIn_pTree->pBlob + aIn_pTree->structOffset;
        const char*    pName  = StringAt(pBlock, aInOut_pWalk->offset + 4, aIn_pTree->structSize);
        uint32_t       length = 0;

        if(pName == NULL)
                return (false);
        while(pName[ length ] != '\0')
                length++;
        aInOut_pWalk->offset = Align4(aInOut_pWalk->offset + 4 + length + 1);
        aInOut_pWalk->depth++;
        if(aInOut_pWalk->depth == 2)
        {
                aInOut_pWalk->isMemory = IsNode(pName, "memory");
                aInOut_pWalk->isChosen = IsNode(pName, "chosen");
        }
        return (true);
}

/** Walks the structure block to its end token. */
static bool
ReadStructure(const Tree* aIn_pTree, Kern_Boot_tInformation* aInOut_pInformation)
{
        Walk walk = { 0, 0, false, false, 2, 1 };

        while(IsInside(walk.offset, 4, aIn_pTree->structSize))
        {
                const uint32_t token  = Be32(aIn_pTree->pBlob + aIn_pTree->structOffset
                                             + walk.offset);
                bool           isGood = true;

                if(token == KERN_DEVICE_TREE_BEGIN_NODE)
                        isGood = StepNode(aIn_pTree, &walk);
                else if(token == KERN_DEVICE_TREE_PROPERTY)
                        isGood = StepProperty(aIn_pTree, &walk, aInOut_pInformation);
                else if(token == KERN_DEVICE_TREE_END_NODE && walk.depth > 0)
                {
                        walk.depth--;
                        walk.offset += 4;
                } else if(token == KERN_DEVICE_TREE_NOP)
                        walk.offset += 4;
                else
                        return (token == KERN_DEVICE_TREE_END && walk.depth == 0);
                if(!isGood)
                        return (false);
        }
        return (false);
}

bool
Kern_DeviceTree_Read(const void* aIn_pBlob, Kern_Boot_tInformation* aInOut_pInformation)
{
        Tree tree;

        if(!ReadHeader((const uint8_t*) aIn_pBlob, &tree))
                return (false);
        Kern_Boot_AddRange(aInOut_pInformation->reserved,
                           &aInOut_pInformation->reservedCount,
                           (uintptr_t) aIn_pBlob,
                           tree.size);
        return (ReadReservations(&tree, aInOut_pInformation)
                && ReadStructure(&tree, aInOut_pInformation));
}
