/*
 * Copyright (C) 2023 Lance Corporation
 */

#include "priv_malloc.h"

#ifndef NULL
    #define NULL                 (void *)0
#endif

#ifndef size_t
    typedef unsigned int         sizet;
    #define size_t               sizet
#endif

#ifndef uint8_t
    typedef unsigned char        uint8;
    #define uint8_t              uint8
#endif

#ifndef uint16_t
    typedef unsigned short       uint16;
    #define uint16_t             uint16
#endif

#ifndef uint32_t
    typedef unsigned long        uint32;
    #define uint32_t             uint32
#endif

#ifndef uint64_t
    typedef unsigned long long   uint64;
    #define uint64_t             uint64
#endif

#ifdef  PRIVILEGED_RAM
    #define PRIVILEGED_BSS       __attribute__((section(".lmubss.heap")))
    #define PRIVILEGED_DATA      __attribute__((section(".lmudata.heap")))
    #define PRIVILEGED_TEXT      __attribute__((section(".lmutext.heap")))
#else
    #define PRIVILEGED_BSS
    #define PRIVILEGED_DATA
    #define PRIVILEGED_TEXT
#endif

typedef struct BLOCK_LINK
{
    struct BLOCK_LINK  *pxNextBlock;
    size_t              xBlockSize;
} BlockLink_t;

typedef struct MEM_MANAGE
{
    struct BLOCK_LINK   xFreeBlockHead;
    struct BLOCK_LINK   xAllocBlockHead;
    struct BLOCK_LINK  *pxUnifyBlockEnd;
    size_t              xMaxFreeBlockInBytes;
    size_t              xHeapTotalSizeInBytes;
} MemManage_t;

typedef struct MEM_STATE
{
    size_t              xNumberOfFreeBlocks;
    size_t              xMinFreeBlockInBytes;
    size_t              xMaxFreeBlockInBytes;
    size_t              xNumberOfAllocBlocks;
    size_t              xMinAllocBlockInBytes;
    size_t              xMaxAllocBlockInBytes;
    size_t              xHeapTotalSizeInBytes;
    size_t              xFreeTotalSizeInBytes;
    size_t              xAllocTotalSizeInBytes;
} MemState_t;

#define ALIGN_DOWN(x, align)  ((size_t)(x & (~(align - 1))))
#define ALIGN_UP(x, align)    ((size_t)((x + (align - 1)) & (~(align - 1))))

#define BYTE_ALIGNMENT        ((size_t)(4))
#define INVALID_VALUE         ((size_t)(-1))
#define HEAP_BUFFER_SIZE      ((size_t)(257))
#define BYTE_ALIGNMENT_MASK   ((size_t)(BYTE_ALIGNMENT - 1))
#define BLOCK_NODE_SIZE       ((size_t)(ALIGN_UP(sizeof(BlockLink_t), BYTE_ALIGNMENT)))

PRIVILEGED_TEXT static void priv_HeapMerge(BlockLink_t *pxPreviousBlock, BlockLink_t *pxFreeBlock);
PRIVILEGED_TEXT static void priv_HeapState(void);
PRIVILEGED_TEXT static void priv_HeapInit(void);

PRIVILEGED_DATA static MemManage_t gxHeapObject;
PRIVILEGED_DATA static uint8_t     gucHeap[HEAP_BUFFER_SIZE];

static void priv_HeapInit(void)
{
    MemManage_t *pxHeapObject;
    BlockLink_t *pxFirstFreeBlock;
    size_t       uxAddress1, uxAddress2, xTotalHeapSize;

    uxAddress1 = (size_t)gucHeap;
    pxHeapObject = &gxHeapObject;
    xTotalHeapSize = HEAP_BUFFER_SIZE;

    if ((uxAddress1 & BYTE_ALIGNMENT_MASK) != 0)
    {
        uxAddress2 = ALIGN_UP(uxAddress1, BYTE_ALIGNMENT);
        xTotalHeapSize -= uxAddress2 - uxAddress1;
        uxAddress1 = uxAddress2;
    }

    pxFirstFreeBlock = (void *)uxAddress1;
    uxAddress1 += xTotalHeapSize;

    if ((uxAddress1 & BYTE_ALIGNMENT_MASK) != 0)
    {
        uxAddress2 = ALIGN_DOWN(uxAddress1, BYTE_ALIGNMENT);
        xTotalHeapSize -= uxAddress1 - uxAddress2;
        uxAddress1 = uxAddress2;
    }

    uxAddress1 -= BLOCK_NODE_SIZE;
    xTotalHeapSize -= BLOCK_NODE_SIZE;

    pxHeapObject->pxUnifyBlockEnd = (void *)uxAddress1;
    pxHeapObject->pxUnifyBlockEnd->pxNextBlock = NULL;
    pxHeapObject->pxUnifyBlockEnd->xBlockSize = INVALID_VALUE;
    pxHeapObject->xAllocBlockHead.pxNextBlock = pxHeapObject->pxUnifyBlockEnd;
    pxHeapObject->xAllocBlockHead.xBlockSize = INVALID_VALUE;

    pxFirstFreeBlock->pxNextBlock = pxHeapObject->pxUnifyBlockEnd;
    pxFirstFreeBlock->xBlockSize = xTotalHeapSize - BLOCK_NODE_SIZE;
    pxHeapObject->xFreeBlockHead.pxNextBlock = pxFirstFreeBlock;
    pxHeapObject->xFreeBlockHead.xBlockSize = INVALID_VALUE;

    pxHeapObject->xHeapTotalSizeInBytes = xTotalHeapSize;
    pxHeapObject->xMaxFreeBlockInBytes = pxFirstFreeBlock->xBlockSize;
}

static void priv_HeapState(void)
{
    BlockLink_t *pxBlock;
    MemManage_t *pxHeapObject;
    MemState_t   xHeapState;
    size_t       i, xMaxValue, xMinValue;
    uint8_t     *puc;

    pxHeapObject = &gxHeapObject;
    puc = (uint8_t *)(&xHeapState);

    for (i = 0; i < sizeof(xHeapState); ++i)
    {
        puc[i] = 0;
    }

    xHeapState.xHeapTotalSizeInBytes = pxHeapObject->xHeapTotalSizeInBytes;

    for (xMaxValue = 0, xMinValue = INVALID_VALUE, pxBlock = pxHeapObject->xAllocBlockHead.pxNextBlock; \
         pxBlock != pxHeapObject->pxUnifyBlockEnd; pxBlock = pxBlock->pxNextBlock)
    {
        xHeapState.xNumberOfAllocBlocks++;
        xHeapState.xAllocTotalSizeInBytes += (BLOCK_NODE_SIZE + pxBlock->xBlockSize);

        if (pxBlock->xBlockSize > xMaxValue)
        {
            xMaxValue = pxBlock->xBlockSize;
            xHeapState.xMaxAllocBlockInBytes = xMaxValue;
        }

        if (pxBlock->xBlockSize < xMinValue)
        {
            xMinValue = pxBlock->xBlockSize;
            xHeapState.xMinAllocBlockInBytes = xMinValue;
        }
    }

    for (xMaxValue = 0, xMinValue = INVALID_VALUE, pxBlock = pxHeapObject->xFreeBlockHead.pxNextBlock; \
         pxBlock != pxHeapObject->pxUnifyBlockEnd; pxBlock = pxBlock->pxNextBlock)
    {
        xHeapState.xNumberOfFreeBlocks++;
        xHeapState.xFreeTotalSizeInBytes += (BLOCK_NODE_SIZE + pxBlock->xBlockSize);

        if (pxBlock->xBlockSize > xMaxValue)
        {
            xMaxValue = pxBlock->xBlockSize;
            xHeapState.xMaxFreeBlockInBytes = xMaxValue;
        }

        if (pxBlock->xBlockSize < xMinValue)
        {
            xMinValue = pxBlock->xBlockSize;
            xHeapState.xMinFreeBlockInBytes = xMinValue;
        }
    }

    debug_info("\r\n-------------------> begin <------------------\r\n");
    debug_info("pxHeapObject->xFreeBlockHead               = %08x \r\n", (size_t)&pxHeapObject->xFreeBlockHead);
    debug_info("pxHeapObject->xFreeBlockHead.pxNextBlock   = %08x \r\n", (size_t)pxHeapObject->xFreeBlockHead.pxNextBlock);
    debug_info("pxHeapObject->xFreeBlockHead.xBlockSize    = %08x \r\n", (size_t)pxHeapObject->xFreeBlockHead.xBlockSize);
    debug_info("pxHeapObject->xAllocBlockHead              = %08x \r\n", (size_t)&pxHeapObject->xAllocBlockHead);
    debug_info("pxHeapObject->xAllocBlockHead.pxNextBlock  = %08x \r\n", (size_t)pxHeapObject->xAllocBlockHead.pxNextBlock);
    debug_info("pxHeapObject->xAllocBlockHead.xBlockSize   = %08x \r\n", (size_t)pxHeapObject->xAllocBlockHead.xBlockSize);
    debug_info("pxHeapObject->pxUnifyBlockEnd              = %08x \r\n", (size_t)pxHeapObject->pxUnifyBlockEnd);
    debug_info("pxHeapObject->pxUnifyBlockEnd->pxNextBlock = %08x \r\n", (size_t)pxHeapObject->pxUnifyBlockEnd->pxNextBlock);
    debug_info("pxHeapObject->pxUnifyBlockEnd->xBlockSize  = %08x \r\n", (size_t)pxHeapObject->pxUnifyBlockEnd->xBlockSize);
    debug_info("pxHeapObject->xMaxFreeBlockInBytes         = %08x \r\n", (size_t)pxHeapObject->xMaxFreeBlockInBytes);
    debug_info("pxHeapObject->xHeapTotalSizeInBytes        = %08x \r\n", (size_t)pxHeapObject->xHeapTotalSizeInBytes);
    debug_info("--------------------------------------------------\r\n");
    debug_info("xHeapState.xNumberOfFreeBlocks             = %08x \r\n", (size_t)xHeapState.xNumberOfFreeBlocks);
    debug_info("xHeapState.xMinFreeBlockInBytes            = %08x \r\n", (size_t)xHeapState.xMinFreeBlockInBytes);
    debug_info("xHeapState.xMaxFreeBlockInBytes            = %08x \r\n", (size_t)xHeapState.xMaxFreeBlockInBytes);
    debug_info("xHeapState.xNumberOfAllocBlocks            = %08x \r\n", (size_t)xHeapState.xNumberOfAllocBlocks);
    debug_info("xHeapState.xMinAllocBlockInBytes           = %08x \r\n", (size_t)xHeapState.xMinAllocBlockInBytes);
    debug_info("xHeapState.xMaxAllocBlockInBytes           = %08x \r\n", (size_t)xHeapState.xMaxAllocBlockInBytes);
    debug_info("xHeapState.xHeapTotalSizeInBytes           = %08x \r\n", (size_t)xHeapState.xHeapTotalSizeInBytes);
    debug_info("xHeapState.xFreeTotalSizeInBytes           = %08x \r\n", (size_t)xHeapState.xFreeTotalSizeInBytes);
    debug_info("xHeapState.xAllocTotalSizeInBytes          = %08x \r\n", (size_t)xHeapState.xAllocTotalSizeInBytes);
    debug_info("----------------------> end <---------------------\r\n");
}

static void priv_HeapMerge(BlockLink_t *pxPreviousBlock, BlockLink_t *pxFreeBlock)
{
    MemManage_t *pxHeapObject;
    BlockLink_t *pxBlock;
    size_t       xuc1, xuc2;

    xuc1 = (size_t)(pxFreeBlock + 1) + pxFreeBlock->xBlockSize;
    xuc2 = (size_t)(pxFreeBlock->pxNextBlock);

    if ((xuc1 == xuc2) && (NULL != pxFreeBlock->pxNextBlock->pxNextBlock))
    {
        pxFreeBlock->xBlockSize += pxFreeBlock->pxNextBlock->xBlockSize + BLOCK_NODE_SIZE;
        pxFreeBlock->pxNextBlock = pxFreeBlock->pxNextBlock->pxNextBlock;
    }

    xuc1 = (size_t)(pxPreviousBlock + 1) + pxPreviousBlock->xBlockSize;
    xuc2 = (size_t)(pxFreeBlock);

    if (xuc1 == xuc2)
    {
        pxPreviousBlock->xBlockSize += pxFreeBlock->xBlockSize + BLOCK_NODE_SIZE;
        pxPreviousBlock->pxNextBlock = pxFreeBlock->pxNextBlock;
    }

    pxHeapObject = &gxHeapObject;
    pxHeapObject->xMaxFreeBlockInBytes = 0;
    pxBlock = pxHeapObject->xFreeBlockHead.pxNextBlock;

    while (NULL != pxBlock->pxNextBlock)
    {
        if (pxBlock->xBlockSize > pxHeapObject->xMaxFreeBlockInBytes)
        {
            pxHeapObject->xMaxFreeBlockInBytes = pxBlock->xBlockSize;
        }

        pxBlock = pxBlock->pxNextBlock;
    }
}

void *priv_HeapMalloc(size_t xWantedSize)
{
    BlockLink_t *pxNewAllocBlock, *pxNewFreeBlock, *pxBlock;
    BlockLink_t *pxPreviousBlock, *pxPreviousFreeBlock;
    MemManage_t *pxHeapObject;
    size_t       xDiffer1, xDiffer2;
    void        *pvReturn;

    pxHeapObject = &gxHeapObject;
    xDiffer1 = INVALID_VALUE;
    pxNewAllocBlock = NULL;
    pxNewFreeBlock = NULL;
    pvReturn = NULL;

    if (NULL == pxHeapObject->pxUnifyBlockEnd)
    {
        priv_HeapInit();
    }

    xWantedSize = ALIGN_UP(xWantedSize, BYTE_ALIGNMENT);

    if ((xWantedSize > 0) && (xWantedSize <= pxHeapObject->xMaxFreeBlockInBytes))
    {
        pxPreviousBlock = &pxHeapObject->xFreeBlockHead;
        pxBlock = pxPreviousBlock->pxNextBlock;

        while (NULL != pxBlock->pxNextBlock)
        {
            if (xWantedSize <= pxBlock->xBlockSize)
            {
                xDiffer2 = pxBlock->xBlockSize - xWantedSize;

                if (xDiffer2 < xDiffer1)
                {
                    xDiffer1 = xDiffer2;
                    pxNewAllocBlock = pxBlock;
                    pxPreviousFreeBlock = pxPreviousBlock;
                }
            }

            pxPreviousBlock = pxBlock;
            pxBlock = pxBlock->pxNextBlock;
        }

        if (NULL != pxNewAllocBlock)
        {
            pvReturn = (void *)(pxNewAllocBlock + 1);
            pxPreviousBlock = &pxHeapObject->xAllocBlockHead;

            while (pxPreviousBlock->pxNextBlock < pxNewAllocBlock)
            {
                pxPreviousBlock = pxPreviousBlock->pxNextBlock;
            }

            pxBlock = pxPreviousFreeBlock->pxNextBlock->pxNextBlock;
            pxNewAllocBlock->pxNextBlock = pxPreviousBlock->pxNextBlock;
            pxPreviousBlock->pxNextBlock = pxNewAllocBlock;

            if (xDiffer1 > BLOCK_NODE_SIZE)
            {
                pxNewAllocBlock->xBlockSize = xWantedSize;
                pxNewFreeBlock = (BlockLink_t *)((size_t)pvReturn + xWantedSize);
                pxNewFreeBlock->xBlockSize = xDiffer1 - BLOCK_NODE_SIZE;
                pxNewFreeBlock->pxNextBlock = pxBlock;
                pxPreviousFreeBlock->pxNextBlock = pxNewFreeBlock;
            }
            else
            {
                pxPreviousFreeBlock->pxNextBlock = pxBlock;
            }

            pxHeapObject->xMaxFreeBlockInBytes = 0;
            pxBlock = pxHeapObject->xFreeBlockHead.pxNextBlock;

            while (NULL != pxBlock->pxNextBlock)
            {
                if (pxBlock->xBlockSize > pxHeapObject->xMaxFreeBlockInBytes)
                {
                    pxHeapObject->xMaxFreeBlockInBytes = pxBlock->xBlockSize;
                }

                pxBlock = pxBlock->pxNextBlock;
            }
        }
    }

    return pvReturn;
}

void priv_HeapFree(void *pxAllocBlock)
{
    MemManage_t *pxHeapObject;
    BlockLink_t *pxBlock, *pxPreviousBlock;

    if (NULL != pxAllocBlock)
    {
        pxHeapObject = &gxHeapObject;
        pxPreviousBlock = &pxHeapObject->xAllocBlockHead;
        pxBlock = (BlockLink_t *)((size_t)pxAllocBlock - BLOCK_NODE_SIZE);

        while (pxPreviousBlock->pxNextBlock != pxBlock)
        {
            pxPreviousBlock = pxPreviousBlock->pxNextBlock;
        }

        pxPreviousBlock->pxNextBlock = pxBlock->pxNextBlock;
        pxPreviousBlock = &pxHeapObject->xFreeBlockHead;

        while (pxPreviousBlock->pxNextBlock < pxBlock)
        {
            pxPreviousBlock = pxPreviousBlock->pxNextBlock;
        }

        pxBlock->pxNextBlock = pxPreviousBlock->pxNextBlock;
        pxPreviousBlock->pxNextBlock = pxBlock;

        priv_HeapMerge(pxPreviousBlock, pxBlock);
    }
}

int main(int argc, char const *argv[])
{
    uint8_t *pt1, *pt2, *pt3, *pt4, *pt5;

    pt1 = priv_HeapMalloc(0x10);
    if (NULL == pt1)
    {
        debug_info("error 1\n");
    }
    else
    {
        debug_info("\r\n-----1------\r\n");
        priv_HeapState();
    }

    pt2 = priv_HeapMalloc(0x15);
    if (NULL == pt2)
    {
        debug_info("error 2\n");
    }
    else
    {
        debug_info("\r\n-----2------\r\n");
        priv_HeapState();
    }

    pt3 = priv_HeapMalloc(0x20);
    if (NULL == pt3)
    {
        debug_info("error 3\n");
    }
    else
    {
        debug_info("\r\n-----3------\r\n");
        priv_HeapState();
    }

    pt4 = priv_HeapMalloc(0x10);
    if (NULL == pt4)
    {
        debug_info("error 4\n");
    }
    else
    {
        debug_info("\r\n-----4------\r\n");
        priv_HeapState();
    }

    pt5 = priv_HeapMalloc(0x6c);
    if (NULL == pt5)
    {
        debug_info("error 5\n");
    }
    else
    {
        debug_info("\r\n-----5------\r\n");
        priv_HeapState();
    }

    debug_info("\r\n--------------------tets free---------------\r\n");
    priv_HeapFree(pt2);
    priv_HeapState();

    pt2 = priv_HeapMalloc(0x1);
    if (NULL == pt2)
    {
        debug_info("error 6\n");
    }
    else
    {
        debug_info("\r\n-----6------\r\n");
        priv_HeapState();
    }

    priv_HeapFree(pt4);
    debug_info("\r\n-----7------\r\n");
    priv_HeapState();

    priv_HeapFree(pt3);
    debug_info("\r\n-----8------\r\n");
    priv_HeapState();

    pt3 = priv_HeapMalloc(0x38);
    if (NULL == pt3)
    {
        debug_info("error 9\n");
    }
    else
    {
        debug_info("\r\n-----9------\r\n");
        priv_HeapState();
    }

    priv_HeapFree(pt1);
    debug_info("\r\n-----10------\r\n");
    priv_HeapState();

    priv_HeapFree(pt5);
    debug_info("\r\n----11------\r\n");
    priv_HeapState();

    priv_HeapFree(pt2);
    debug_info("\r\n----12------\r\n");
    priv_HeapState();
    debug_info("\r\n-----------------is end---------------\r\n");
    return 0;
}
