/*
 * Copyright (C) 2023 Lance Corporation
 */

#ifndef __PRIV_MALLOC_H__
#define __PRIV_MALLOC_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <stdio.h>

/****************************************************************************
 * Other conditional compilation options
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

void *priv_HeapMalloc(size_t xWantedSize);
void  priv_HeapFree(void *pxAllocBlock);

#define debug_info printf
#define malloc     priv_HeapMalloc
#define free       priv_HeapFree

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __PRIV_MALLOC_H__ */
