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
* Description:  Implementation of far _heapmin() and _fheapmin()
*                   (16-bit code only)
*
****************************************************************************/


#include "dll.h"        // needs to be first
#include "variety.h"
#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>
#include "heap.h"
#include "heapacc.h"


#define HEAP(s)     ((heapblkp __based(s) *)0)

#if defined(__BIG_DATA__)

_WCRTLINK int _heapshrink( void )
{
    return( _fheapshrink() );
}

_WCRTLINK int _heapmin( void )
{
    return( _fheapshrink() );
}

#endif

_WCRTLINK int _fheapmin( void )
{
    return( _fheapshrink() );
}

_WCRTLINK int _fheapshrink( void )
{
    __segment   seg;
    __segment   next_seg;
    __segment   prev_seg;
    int         heap_status;

    heap_status = __HeapMin( __fheapbeg, _NULLSEG );
    _AccessFHeap();
    for( seg = __fheapbeg; seg != _NULLSEG; seg = next_seg ) {
        /* we might free this segment so get the next one now */
        next_seg = HEAP( seg )->nextseg;
        if( HEAP( seg )->numalloc == 0 ) {     /* empty heap */
            /* unlink from heap list */
            prev_seg = HEAP( seg )->prevseg;
            if( next_seg != _NULLSEG ) {
                HEAP( next_seg )->prevseg = prev_seg;
            }
            if( prev_seg == _NULLSEG ) {
                __fheapbeg = next_seg;
            } else {
                HEAP( prev_seg )->nextseg = next_seg;
            }
            __fheapRover = __fheapbeg;
            heap_status = __FreeSeg( seg );
        }
    }
    _ReleaseFHeap();
    return( heap_status );
}
