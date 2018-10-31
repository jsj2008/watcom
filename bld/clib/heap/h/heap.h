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
* Description:  Heap library configuration for various platforms.
*
****************************************************************************/


/*
 * Comments for heap implementation.
 */

#ifdef _M_IX86
#include <i86.h>
#include "extender.h"
#endif


#define BLK2CPTR(f)     ((unsigned)((unsigned)(f) + TAG_SIZE))
#define CPTR2BLK(p)     ((unsigned)((unsigned)(p) - TAG_SIZE))

#if defined( __DOS_EXT__ )
#define DPMI2BLK(h)    ((mheapptr)(h + 1))
#define BLK2DPMI(h)    (((dpmi_hdr *)h) - 1)
#endif

#define TAG_SIZE        (sizeof( tag ))
#if defined( _M_I86 )
    #define HEAP_ROUND_SIZE (TAG_SIZE)
#else
    #define HEAP_ROUND_SIZE (TAG_SIZE + TAG_SIZE)
#endif
#define __ROUND_UP_SIZE_HEAP(s)     __ROUND_UP_SIZE( s + TAG_SIZE, HEAP_ROUND_SIZE )
#define __ROUND_DOWN_SIZE_HEAP(s)   __ROUND_DOWN_SIZE( s - TAG_SIZE, HEAP_ROUND_SIZE )
#define FRL_SIZE                    __ROUND_UP_SIZE( sizeof( freelistp ), HEAP_ROUND_SIZE )

#if defined( _M_IX86 )
 #define _DGroup()      FP_SEG((&__nheapbeg))
#else
 #define _DGroup()      0
#endif

#define __HM_SUCCESS    0
#define __HM_FAIL       1
#define __HM_TRYGROW    2

#define PARAS_IN_64K    (0x1000)
#define END_TAG         (/*0x....ffff*/ ~0U)

#define GET_BLK_SIZE(p)             ((p)->len & ~1U)
#define IS_BLK_INUSE(p)             (((p)->len & 1) != 0)
#define SET_BLK_SIZE_INUSE(p,s)     (p)->len = ((s) | 1)
#define SET_BLK_INUSE(p)            (p)->len |= 1
#define IS_BLK_END(p)               ((p)->len == END_TAG)
#define SET_BLK_END(p)              (p)->len = END_TAG

#define NEXT_BLK(p)                 ((unsigned)(p) + (p)->len)
#define NEXT_BLK_A(p)               ((unsigned)(p) + GET_BLK_SIZE(p))

#define IS_IN_HEAP(m,h)     ((unsigned)(h) <= (unsigned)(m) && (unsigned)(m) < (unsigned)NEXT_BLK((h)))

#define memcpy_i86      "shr cx,1"  "rep movsw" "adc cx,cx"   "rep movsb"
#define memcpy_386      "shr ecx,1" "rep movsw" "adc ecx,ecx" "rep movsb"

#define memset_i86      "mov ah,al" "shr cx,1"  "rep stosw" "adc cx,cx"   "rep stosb"
#define memset_386      "mov ah,al" "shr ecx,1" "rep stosw" "adc ecx,ecx" "rep stosb"

#ifdef _M_I86
typedef void            __based(void) *void_bptr;
#else
typedef void            _WCNEAR *void_bptr;
#endif
typedef void            _WCNEAR *void_nptr;
typedef void            _WCFAR *void_fptr;
typedef void            _WCHUGE *void_hptr;

typedef unsigned int    tag;
typedef unsigned char   _WCNEAR *PTR;
typedef unsigned char   _WCFAR *FARPTR;

/*
** NOTE: the size of these data structures is critical to the alignemnt
**       of the pointers returned by malloc().
*/
typedef struct freelist {
    tag                 len;    /* length of block in free list */
    unsigned int        prev;   /* offset of previous block in free list */
    unsigned int        next;   /* offset of next block in free list */
} freelist;

typedef freelist        _WCFAR *farfrlptr;

typedef struct heapblk {
    tag                 heaplen;        /* size of heap (0 = 64K) */
#if defined( _M_I86 )
    __segment           prevseg;        /* segment selector for previous heap */
    __segment           nextseg;        /* segment selector for next heap */
#elif defined( _M_IX86 )
    __segment           prevseg;        /* segment selector for previous heap */
    __segment           dummy1;         /* not used, match miniheapblkp size */
    __segment           nextseg;        /* segment selector for next heap */
    __segment           dummy2;         /* not used, match miniheapblkp size */
#else
    unsigned int        dummy1;         /* not used, match miniheapblkp size */
    unsigned int        dummy2;         /* not used, match miniheapblkp size */
#endif
    unsigned int        rover;          /* roving pointer into free list */
    unsigned int        b4rover;        /* largest block before rover */
    unsigned int        largest_blk;    /* largest block in the heap  */
    unsigned int        numalloc;       /* number of allocated blocks in heap */
    unsigned int        numfree;        /* number of free blocks in the heap */
    freelist            freehead;       /* listhead of free blocks in heap */
#if defined( __WARP__ )
    unsigned int        spare;          /* not used, match miniheapblkp size */
#endif
} heapblk;

typedef struct freelistp {
    tag                 len;
    struct freelistp    _WCNEAR *prev;
    struct freelistp    _WCNEAR *next;
} freelistp;

typedef freelistp       _WCNEAR *frlptr;

typedef struct heapblkp {
    tag                 heaplen;
#if defined( _M_I86 )
    __segment           prevseg;        /* segment selector for previous heap */
    __segment           nextseg;        /* segment selector for next heap */
#elif defined( _M_IX86 )
    __segment           prevseg;        /* segment selector for previous heap */
    __segment           dummy1;         /* not used, match miniheapblkp size */
    __segment           nextseg;        /* segment selector for next heap */
    __segment           dummy2;         /* not used, match miniheapblkp size */
#else
    unsigned int        dummy1;         /* not used, match miniheapblkp size */
    unsigned int        dummy2;         /* not used, match miniheapblkp size */
#endif
    frlptr              rover;
    unsigned int        b4rover;
    unsigned int        largest_blk;
    unsigned int        numalloc;
    unsigned int        numfree;
    freelistp           freehead;
#if defined( __WARP__ )
    unsigned int        spare;          /* not used, match miniheapblkp size */
#endif
} heapblkp;

typedef struct miniheapblkp {
    tag                 len;
    struct miniheapblkp _WCNEAR *prev;
    struct miniheapblkp _WCNEAR *next;
    frlptr              rover;
    unsigned int        b4rover;
    unsigned int        largest_blk;
    unsigned int        numalloc;
    unsigned int        numfree;
    freelistp           freehead;
#if defined( __WARP__ )
    unsigned int        used_obj_any    :1; /* allocated with OBJ_ANY - block may be in high memory */
#endif
} miniheapblkp;

typedef miniheapblkp    _WCNEAR *mheapptr;

typedef struct heapstart {
    heapblk             h;
    freelist            first;
} heapstart;

typedef struct heapend {
    tag                 last_tag;
    freelist            last;
} heapend;

#ifdef __DOS_EXT__
typedef struct dpmi_hdr {
    unsigned long       dpmi_handle;
    tag                 dos_seg_value;  // 0 => DPMI block, else DOS segment
} dpmi_hdr;
#endif

extern mheapptr         _WCNEAR __nheapbeg;
#if defined( _M_I86 )
extern __segment        __fheapbeg;
extern __segment        __bheapbeg;
extern __segment        __fheapRover;
#endif
extern int              __heap_enabled;
extern unsigned int     __LargestSizeB4Rover;
extern mheapptr         __MiniHeapRover;
extern unsigned int     __LargestSizeB4MiniHeapRover;
extern mheapptr         __MiniHeapFreeRover;

#if defined( __WARP__ )
extern unsigned char    _os2_use_obj_any;           // Prefer high memory heap block
extern unsigned char    _os2_obj_any_supported;     // DosAllocMem supports OBJ_ANY
#endif

extern size_t           __LastFree( void );
extern int              __NHeapWalk( struct _heapinfo *entry, mheapptr start );
extern int              __ExpandDGROUP( unsigned int __amt );
extern int              __HeapManager_expand( __segment seg, void_bptr cstg, size_t req_size, size_t *growth_size );
extern void             __UnlinkNHeap( mheapptr heap, mheapptr prev_heap, mheapptr next_heap );

extern  void_bptr       __MemAllocator( unsigned __size, __segment __seg, void_bptr __heap );
extern  void            __MemFree( void_bptr __cstg, __segment __seg, void_bptr __heap );
#if defined( _M_I86 )
  #pragma aux __MemAllocator "*" parm [ax] [dx] [bx]
  #pragma aux __MemFree      "*" parm [ax] [dx] [bx]
#elif defined( _M_IX86 )
  #pragma aux __MemAllocator "*" parm [eax] [dx] [ebx]
  #pragma aux __MemFree      "*" parm [eax] [dx] [ebx]
#endif

#if defined( _M_I86 )
extern __segment        __AllocSeg( unsigned int __amt );
extern int              __GrowSeg( __segment __seg, unsigned int __amt );
extern int              __FreeSeg( __segment __seg );
extern int              __HeapWalk( struct _heapinfo *entry, __segment seg, __segment one_heap );
extern int              __HeapMin( __segment __seg, __segment one_heap );
extern int              __HeapSet( __segment __seg, unsigned fill );
extern void             _WCFAR __HeapInit( mheapptr start, unsigned int amount );
#endif

#if defined( __DOS_EXT__ )
extern void             *__ReAllocDPMIBlock( frlptr p1, unsigned req_size );
extern void             *__ExpandDPMIBlock( frlptr, unsigned );
#endif

#if defined(__WARP__) || defined(__WINDOWS__) || defined(__NT__) || \
    defined(__CALL21__) || defined(__RDOS__) || defined(__DOS_EXT__)
extern int              __nheapshrink( void );
#endif

#if defined( __QNX__ )
extern void             __setcbrk( unsigned offset );
#endif
