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
* Description:  Heart of the heap manager. Do not break
*               unless you have a death wish.
*
****************************************************************************/


#include "variety.h"
#include <limits.h>
#include <malloc.h>
#include "heap.h"


#ifdef _M_I86
#define HEAP(s)     ((heapblkp __based(s) *)(heap))
#define FRLPTR(s)   freelistp __based(s) *
#else
#define HEAP(s)     ((heapblkp _WCNEAR *)(heap))
#define FRLPTR(s)   freelistp _WCNEAR *
#endif

//
// input:
//      size    - #bytes to allocate
//      seg     - 16bit Intel data selector containing heap
//      heap    - address of heap control block
//                if 16bit Intel -> offset within segment
//                else           -> absolute pointer value
//
// output:
//      cstg    - "C" address of allocated storage or zero on failure
//                if 16bit Intel -> offset within segment
//                else           -> absolute pointer value
//
void_bptr __MemAllocator( unsigned req_size, __segment seg, void_bptr heap )
{
    void_bptr   cstg;

    cstg = NULL;                                        // assume the worst
    if( req_size != 0 ) {                               // quit if size is zero
        unsigned    size;

        size = __ROUND_UP_SIZE_HEAP( req_size );        // round up size
        if( size >= req_size ) {                        // quit if overflowed
            unsigned    largest;

            largest = HEAP( seg )->largest_blk;
            if( size < FRL_SIZE ) {
                size = FRL_SIZE;
            }
            if( size <= largest ) {                     // quit if size too big
                FRLPTR( seg )   pcur;
                unsigned        len;

                pcur = HEAP( seg )->rover;              // start at rover
                largest = HEAP( seg )->b4rover;
                if( size <= largest ) {                 // check size with rover
                    pcur = HEAP( seg )->freehead.next;  // start at beginning
                    largest = 0;                        // reset largest block size
                }
                for( ;; ) {                             // search free list
                    len = pcur->len;
                    if( size <= len ) {                 // found one
                        break;
                    }
                    if( largest < len ) {               // update largest block size
                        largest = len;
                    }
                    pcur = pcur->next;                  // advance to next entry
                                                        // if back at start
                    if( pcur == (FRLPTR( seg ))&(HEAP( seg )->freehead) ) {
                        HEAP( seg )->largest_blk = largest; // update largest
                        return( cstg );                 // return 0
                    }
                }
                HEAP( seg )->b4rover = largest;         // update rover size
                HEAP( seg )->numalloc++;                // udpate allocation count
                len -= size;                            // compute leftover size
                if( len >= FRL_SIZE ) {                 // if leftover big enough, split into two chunks
                    FRLPTR( seg )   pprev;              // before current
                    FRLPTR( seg )   pnext;              // after current
                    FRLPTR( seg )   pnew;               // start of new piece

                    pnew = (FRLPTR( seg ))((PTR)pcur + size);
                    HEAP( seg )->rover = pnew;          // update rover
                    pnew->len = len;                    // set new size
                    pcur->len = size;                   // reset current size
                    pprev = pcur->prev;                 // update next/prev links
                    pnew->prev = pprev;
                    pnext = pcur->next;
                    pnew->next = pnext;
                    pprev->next = pnew;
                    pnext->prev = pnew;
                } else {                                // just use this chunk
                    FRLPTR( seg )   pprev;              // before current
                    FRLPTR( seg )   pnext;              // after current

                    HEAP( seg )->numfree--;             // 1 fewer entries in free list
                    pprev = pcur->prev;
                    HEAP( seg )->rover = pprev;         // update rover
                    pnext = pcur->next;                 // update next/prev links
                    pprev->next = pnext;
                    pnext->prev = pprev;
                }
                SET_BLK_INUSE( pcur );                  // mark as allocated
                                                        // get pointer to user area
                cstg = (void_bptr)BLK2CPTR( pcur );
            }
        }
    }
    return( cstg );
}

//
// input:
//      cstg    - "C" address of block to free
//                if 16bit Intel -> offset within segment
//                else           -> absolute pointer value
//      seg     - 16bit Intel data selector containing heap
//      heap    - address of heap control block
//                if 16bit Intel -> offset within segment
//                else           -> absolute pointer value
//
// output:
//      none
//
void __MemFree( void_bptr cstg, __segment seg, void_bptr heap )
{
    if( cstg != NULL ) {                                // quit if pointer is zero
        FRLPTR( seg )   pfree;

        pfree = (FRLPTR( seg ))CPTR2BLK( cstg );
        if( IS_BLK_INUSE( pfree ) ) {                   // quit if storage is free
            FRLPTR( seg )   pnext;
            FRLPTR( seg )   pprev;
            FRLPTR( seg )   ptr;
            unsigned        len;

            do {                                        // this allows break statement
                unsigned    average;
                unsigned    numfree;

                // look at next block to try and coalesce
                len = GET_BLK_SIZE( pfree );            // get next block
                pnext = (FRLPTR( seg ))((PTR)pfree + len);
                if( !IS_BLK_INUSE( pnext ) ) {          // if it is free
                    len += pnext->len;                  // include the length
                    pfree->len = len;                   // update pfree length
                    if( pnext == HEAP( seg )->rover ) { // check for rover
                        HEAP( seg )->rover = pfree;     // update rover
                    }
                    pprev = pnext->prev;                // fixup next/prev links
                    pnext = pnext->next;
                    pprev->next = pnext;
                    pnext->prev = pprev;
                    HEAP( seg )->numfree--;             // reduce numfree
                    break;                              // proceed to coalesce code
                }

                // following block is not free
                // we must now try to figure out where pfree
                // is in relation to the entries in the free list
                pfree->len = len;                       // remove allocated marker

                // check a few special places
                // see if pfree is:
                // - just before or just after the rover
                // - at the very beginning or very end of the heap
                pnext = HEAP( seg )->rover;             // get rover
                if( pfree < pnext ) {                   // where is pfree?
                                                        // pfree is before rover
                    if( pfree > pnext->prev ) {         // where is pfree?
                                                        // pfree is next to rover
                        break;                          // proceed to coalesce code
                    }
                    pnext = HEAP( seg )->freehead.next; // get start of free list
                    if( pfree < pnext ) {               // where is pfree?
                                                        // pfree is at start of list
                        break;                          // proceed to coalesce code
                    }
                } else {                                // pfree is after rover
                    pnext = pnext->next;                // pnext is after rover
                    if( pfree < pnext ) {               // where is pfree?
                                                        // pfree is just after rover
                        break;                          // proceed to coalesce code
                    }
                                                        // get end of free list
                    pnext = (FRLPTR( seg ))&(HEAP( seg )->freehead);
                    pprev = pnext->prev;
                    if( pfree > pprev ) {               // where is pfree?
                                                        // pfree is at end of list
                        break;                          // proceed to coalesce code
                    }
                }

                // Calculate the average number of allocated blocks we may
                // have to skip until we expect to find a free block.  If
                // this number is less than the total number of free blocks,
                // chances are that we can find the correct position in the
                // free list by scanning ahead for a free block and linking
                // this free block before the found free block.  We protect
                // ourself against the degenerate case where there is an
                // extremely long string of allocated blocks by limiting the
                // number of blocks we will search to twice the calculated
                // average.

                numfree = HEAP( seg )->numfree;
                average = HEAP( seg )->numalloc / ( numfree + 1 );
                if( average < numfree ) {
                    unsigned    worst;

                    // There are lots of allocated blocks and lots of free
                    // blocks.  On average we should find a free block
                    // quickly by following the allocated blocks, but the
                    // worst case can be very bad.  So, we try scanning the
                    // allocated blocks and give up once we have looked at
                    // twice the average.

                    worst = HEAP( seg )->numalloc - numfree;
                    average *= 2;                       // give up after this many
                    if( worst <= numfree ) {
                        average = UINT_MAX;             // we won't give up loop
                    }
                                                        // point at next allocated
                    pnext = (FRLPTR( seg ))NEXT_BLK( pfree );
                    for( ; !IS_BLK_END( pnext ); ) {    // check for end TAG, stop at end tag
                        if( !IS_BLK_INUSE( pnext ) )    // pnext is free then break twice!
                            goto found_it;              // we have the spot
                        pnext = (FRLPTR( seg ))NEXT_BLK_A( pnext );
                        average--;
                        if( average == 0 ) {            // give up search
                            break;
                        }
                    }
                }

                // when all else fails, search the free list
                pnext = HEAP( seg )->rover;             // begin at rover
                if( pfree < pnext ) {                   // is pfree before rover? then begin at start
                    pnext = HEAP( seg )->freehead.next;
                }
                for( ;; ) {
                    if( pfree < pnext ) {               // if pfree before pnext
                        break;                          // we found it
                    }
                    pnext = pnext->next;                // advance pnext

                    if( pfree < pnext ) {               // if pfree before pnext
                        break;                          // we found it
                    }
                    pnext = pnext->next;                // advance pnext

                    if( pfree < pnext ) {               // if pfree before pnext
                        break;                          // we found it
                    }
                    pnext = pnext->next;                // advance pnext
                }
            } while( 0 );                               // only do once

found_it:
            // if we are here, then we found the spot
            pprev = pnext->prev;                        // setup pprev

            // pprev, pfree, pnext are all setup
            len = pfree->len;
                                                        // check pprev and pfree
            ptr = (FRLPTR( seg ))NEXT_BLK( pprev );
            if( ptr == pfree ) {                        // are they adjacent?
                                                        // coalesce pprev and pfree
                len += pprev->len;                      // udpate len
                pprev->len = len;
                if( HEAP( seg )->rover == pfree ) {     // check rover impact
                    HEAP( seg )->rover = pprev;         // update rover
                }
                pfree = pprev;                          // now work with coalesced blk
            } else {
                HEAP( seg )->numfree++;                 // one more free entry
                pfree->next = pnext;                    // update next/prev entries
                pfree->prev = pprev;
                pprev->next = pfree;
                pnext->prev = pfree;
            }
            HEAP( seg )->numalloc--;                    // one fewer allocated

            if( pfree < HEAP( seg )->rover ) {          // check rover impact
                if( HEAP( seg )->b4rover < len ) {      // is len bigger than b4rover
                    HEAP( seg )->b4rover = len;         // then update b4rover
                }
            }
            if( HEAP( seg )->largest_blk < len ) {      // check largest block
                HEAP( seg )->largest_blk = len;
            }
        }
    }
}
