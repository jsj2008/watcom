/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Implementation of near malloc() and _nmalloc().
*
****************************************************************************/


#include "dll.h"        // needs to be first
#include "variety.h"
#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>
#include "extfunc.h"
#include "heapacc.h"
#include "heap.h"
#if defined(__RDOS__)
#include <rdos.h>
#endif

#if defined(_M_IX86)
    #pragma aux (__outside_CLIB) __nmemneed;
#endif

mheapptr        _WCNEAR __nheapbeg = NULL;
mheapptr        __MiniHeapRover = NULL;
unsigned int    __LargestSizeB4MiniHeapRover = 0;

#if defined(__WARP__)

/* OS/2 high memory heap support
 * malloc allocates from current heap
 * _os2halloc allocates from high memory if possible
 * falls back to lower memory if high memory not available
 * _os2lalloc always allocates from lower memory
 */

unsigned char _os2_use_obj_any = 0;

_WCRTLINK int _use_os2_high_mem( int fUseHighMem )
{
    int   prior;

    _AccessNHeap();
    prior = _os2_use_obj_any;
    _os2_use_obj_any = ( fUseHighMem != 0 );
    _ReleaseNHeap();
    return( prior );
}

/**
 * Allocate from lower heap
 */

_WCRTLINK void *_os2lmalloc( size_t amount )
{
    int         prior;
    void_nptr   cstg;

    _AccessNHeap();
    prior = _use_os2_high_mem( 0 );
    cstg = _nmalloc( amount );
    _use_os2_high_mem( prior );
    _ReleaseNHeap();
    return( cstg );
}

/**
 * Allocate from upper memory heap if possible
 */

_WCRTLINK void *_os2hmalloc( size_t amount )
{
    int         prior;
    void_nptr   cstg;

    _AccessNHeap();
    prior = _use_os2_high_mem( 1 );
    cstg = _nmalloc( amount );
    _use_os2_high_mem( prior );
    _ReleaseNHeap();
    return( cstg );
}

#endif /* __WARP__ */

#if defined(__SMALL_DATA__)

_WCRTLINK void *malloc( size_t amount )
{
    return( _nmalloc( amount ) );
}

#endif

/* By setting __ALLOC_DEBUG it is possible to spot memory allocation errors in
   RDOS target. RdosAllocateMem will here allocate whole pages regardless of actual
   request size. The kernel device-driver should also be set to not reuse pages
   until all pages have been allocated for this to work properly.  */

#if defined( __RDOS__ ) && defined( __ALLOC_DEBUG )

_WCRTLINK void_nptr _nmalloc( size_t amt )
{
    void *cstg;

    cstg = RdosAllocateDebugMem( amt );

    return( (void_nptr)cstg );
}

#else

_WCRTLINK void_nptr _nmalloc( size_t amt )
{
    unsigned        largest;
    unsigned        size;
    void_bptr       cstg;
    unsigned char   expanded;
    mheapptr        heap;

#if defined(__WARP__)
    unsigned char   use_obj_any;
#endif // __WARP__

    if( (amt == 0) || (amt > -sizeof( heapblk )) ) {
        return( NULL );
    }

    // Try to determine which miniheap to begin allocating from.
    // first, round up the amount
    size = __ROUND_UP_SIZE_HEAP( amt );
    if( size < FRL_SIZE ) {
        size = FRL_SIZE;
    }

    _AccessNHeap();
    cstg = NULL;
    expanded = 0;
    for( ;; ) {
#if defined(__WARP__)
        // Need to update each pass in case 1st DosAllocMem determines OBJ_ANY not supported
        use_obj_any = ( _os2_obj_any_supported && _os2_use_obj_any );
#endif
        // Figure out where to start looking for free blocks
        if( size > __LargestSizeB4MiniHeapRover ) {
            heap = __MiniHeapRover;
            if( heap == NULL ) {
                __LargestSizeB4MiniHeapRover = 0;   // force to be updated
                heap = __nheapbeg;
            }
        } else {
            __LargestSizeB4MiniHeapRover = 0;   // force to be updated
            heap = __nheapbeg;
        }
        // Search for free block
        for( ; heap != NULL; heap = heap->next ) {
            __MiniHeapRover = heap;
            largest = heap->largest_blk;
#if defined(__WARP__)
            if( use_obj_any == ( heap->used_obj_any != 0 ) ) {
#endif // __WARP__
              if( largest >= amt ) {
                  cstg = __MemAllocator( amt, _DGroup(), heap );
                  if( cstg != NULL ) {
                      goto lbl_release_heap;
                  }
              }
#if defined(__WARP__)
            }
#endif // __WARP__
            if( __LargestSizeB4MiniHeapRover < largest ) {
                __LargestSizeB4MiniHeapRover = largest;
            }
        }
        // OS/2 only - if not block of requested type, will allocate one and find in 2nd pass
        // Try to expand heap and retry
        if( expanded || !__ExpandDGROUP( amt ) ) {
            if( __nmemneed( amt ) == 0 ) {
                break;                  // give up
            }
            expanded = 0;
        } else {
            expanded = 1;
        }
    } /* forever */
lbl_release_heap:
    _ReleaseNHeap();
    return( cstg );
}

#endif
