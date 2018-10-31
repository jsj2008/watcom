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
* Description:  clib internal heap expansion routine.
*
****************************************************************************/


#include "dll.h"        // needs to be first
#include "variety.h"
#include <stddef.h>
#include <malloc.h>
#include "heap.h"
#include "heapacc.h"


#ifdef _M_I86
#define HEAP(s)             miniheapblkp __based(s) *
#define FRLPTR(s)           freelistp __based(s) *
#define FRLPTRADD(s,p,o)    (freelistp __based(s) *)((PTR)(p)+(o))
#else
#define HEAP(s)             miniheapblkp _WCNEAR *
#define FRLPTR(s)           freelistp _WCNEAR *
#define FRLPTRADD(s,p,o)    (freelistp _WCNEAR *)((PTR)(p)+(o))
#endif

int __HeapManager_expand( __segment seg, void_bptr cstg, size_t req_size, size_t *growth_size )
{
    HEAP( seg )     heap;
    FRLPTR( seg )   p1;
    FRLPTR( seg )   p2;
    FRLPTR( seg )   pnext;
    FRLPTR( seg )   pprev;
    size_t          new_size;
    size_t          old_size;
    size_t          free_size;

    /* round (new_size + tag) to multiple of pointer size */
    new_size = __ROUND_UP_SIZE_HEAP( req_size );
    if( new_size < req_size )
        new_size = /*0x....ffff*/ ~0U;  //go for max
    if( new_size < FRL_SIZE ) {
        new_size = FRL_SIZE;
    }
    p1 = (FRLPTR( seg ))CPTR2BLK( cstg );
    old_size = GET_BLK_SIZE( p1 );
    if( new_size > old_size ) {
        /* enlarging the current allocation */
        p2 = FRLPTRADD( seg, p1, old_size );
        *growth_size = new_size - old_size;
        for( ;; ) {
            if( IS_BLK_END( p2 ) )
                return( __HM_TRYGROW );
            if( IS_BLK_INUSE( p2 ) )    /* next piece is allocated */
                break;
            heap = 0;                   // for based heap
            free_size = p2->len;
            pnext = p2->next;
            pprev = p2->prev;
            if( seg == _DGroup() ) {    // near heap
                for( heap = __nheapbeg; heap->next != NULL; heap = heap->next ) {
                    if( IS_IN_HEAP( p1, heap ) ) {
                        break;
                    }
                }
            }
            if( heap->rover == p2 ) {
                heap->rover = p2->prev;
            }
            if( free_size < *growth_size || free_size - *growth_size < FRL_SIZE ) {
                /* unlink small free block */
                pprev->next = pnext;
                pnext->prev = pprev;
                p1->len += free_size;
                heap->numfree--;
                if( free_size >= *growth_size ) {
                    return( __HM_SUCCESS );
                }
                *growth_size -= free_size;
                p2 = FRLPTRADD( seg, p2, free_size );
            } else {
                p2 = FRLPTRADD( seg, p2, *growth_size );
                p2->len = free_size - *growth_size;
                p2->prev = pprev;
                p2->next = pnext;
                pprev->next = p2;
                pnext->prev = p2;
                p1->len += *growth_size;
                return( __HM_SUCCESS );
            }
        }
        /* no suitable free blocks behind, have to move block */
        return( __HM_FAIL );
    }
    /* shrinking the current allocation */
    if( old_size - new_size >= FRL_SIZE ) {
        heap = 0;                   // for based heap
        /* block big enough to split */
        SET_BLK_SIZE_INUSE( p1, new_size );
        p2 = FRLPTRADD( seg, p1, new_size );
        SET_BLK_SIZE_INUSE( p2, old_size - new_size );
        if( seg == _DGroup() ) {    // near heap
            for( heap = __nheapbeg; heap->next != NULL; heap = heap->next ) {
                if( IS_IN_HEAP( p1, heap ) ) {
                    break;
                }
            }
        }
        /* ...free functions will decrement 'numalloc' */
        heap->numalloc++;
#if defined( _M_I86 )
        _bfree( seg, (void_bptr)BLK2CPTR( p2 ) );
        /* free the top portion */
#else
        _nfree( (void_nptr)BLK2CPTR( p2 ) );
#endif
    }
    return( __HM_SUCCESS );
}
