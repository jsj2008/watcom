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
* Description:  far and based heap walk function implementation
*               (16-bit code only)
*
****************************************************************************/


#include "dll.h"        // needs to be first
#include "variety.h"
#include <stddef.h>
#include <malloc.h>
#include "heap.h"


#define HEAP(s)     ((heapblkp __based(s) *)0)
#define FRLPTR(s)   freelistp __based(s) *

static int verifyHeapList( __segment start )
{
    /* make sure list of heaps is a doubly-linked NULL terminated list */
    __segment   seg;
    __segment   next_seg;
    __segment   prev_seg;

    /* check previous heaps end in NULL */
    for( seg = start; ; seg = prev_seg ) {
        prev_seg = HEAP( seg )->prevseg;
        if( prev_seg == start ) {
            return( _HEAPBADBEGIN );
        }
        if( prev_seg == _NULLSEG )
            break;
        if( HEAP( prev_seg )->nextseg != seg ) {
            return( _HEAPBADBEGIN );
        }
    }
    /* check next heaps end in NULL */
    for( ; ; seg = next_seg ) {
        next_seg = HEAP( seg )->nextseg;
        if( next_seg == start ) {
            return( _HEAPBADBEGIN );
        }
        if( next_seg == _NULLSEG )
            break;
        if( HEAP( next_seg )->prevseg != seg ) {
            return( _HEAPBADBEGIN );
        }
    }
    return( _HEAPOK );
}

int __HeapWalk( struct _heapinfo *entry, __segment seg, __segment one_heap )
{
    __segment       next_seg;
    __segment       prev_seg;
    FRLPTR( seg )   frl;
    FRLPTR( seg )   frl_next;

    if( seg == _NULLSEG )
        return( _HEAPEMPTY );
    frl = entry->_pentry;
    if( frl != NULL ) {
        seg = FP_SEG( entry->_pentry );
    } else if( one_heap == _NULLSEG ) {
        /* we are starting a multi-heap walk */
        if( verifyHeapList( seg ) != _HEAPOK ) {
            return( _HEAPBADBEGIN );
        }
    }
    for( ; ; seg = next_seg ) {
        prev_seg = HEAP( seg )->prevseg;
        next_seg = HEAP( seg )->nextseg;
        if( prev_seg != _NULLSEG ) {
            if( HEAP( prev_seg )->nextseg != seg || prev_seg == next_seg ) {
                return( _HEAPBADBEGIN );
            }
        }
        if( next_seg != _NULLSEG ) {
            if( HEAP( next_seg )->prevseg != seg ) {
                return( _HEAPBADBEGIN );
            }
        }
        if( frl == NULL ) {
            if( HEAP( seg )->freehead.len != 0 )
                return( _HEAPBADBEGIN );
            frl = (FRLPTR( seg ))sizeof( heapblk );
        } else {    /* advance to next entry */
            frl_next = (FRLPTR( seg ))NEXT_BLK_A( frl );
            if( frl_next <= frl )
                return( _HEAPBADNODE );
            frl = frl_next;
            if( HEAP( seg )->heaplen != 0 && (tag)frl > HEAP( seg )->heaplen ) {
                return( _HEAPBADNODE );
            }
        }
        if( !IS_BLK_END( frl ) )
            break;
        if( next_seg == _NULLSEG || one_heap != _NULLSEG ) {
            entry->_useflag = _USEDENTRY;
            entry->_size    = 0;
            entry->_pentry  = NULL;
            return( _HEAPEND );
        }
        frl = NULL;
    }
    entry->_pentry  = frl;
    entry->_useflag = _FREEENTRY;
    entry->_size    = GET_BLK_SIZE( frl );
    if( IS_BLK_INUSE( frl ) ) {
        entry->_useflag = _USEDENTRY;
    }
    return( _HEAPOK );
}
