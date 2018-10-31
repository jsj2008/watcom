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
* Description:  Implementation of sbrk() for DOS, OSI and Windows.
*
****************************************************************************/


#include "variety.h"
#include <stdlib.h>
#if defined(__WINDOWS_386__)
    #include "windpmi.h"
#elif defined(__WINDOWS__)
    #include <windows.h>
#else
    #include <dos.h>
    #include "tinyio.h"
#endif
#include "rtstack.h"
#include "rterrno.h"
#include "rtdata.h"
#include "heap.h"


#if defined( __DOS__ ) && !defined( __CALL21__ )

#ifdef _M_I86

extern  unsigned short SS_Reg( void );
#pragma aux SS_Reg =    \
        "mov ax,ss"     \
    parm caller value [ax]

extern  int SetBlock( unsigned short, unsigned );
#pragma aux SetBlock =      \
        "mov    ah,04ah"    \
        "int    021h"       \
        "sbb    ax,ax"      \
    parm caller [es] [bx] value [ax]

#else

extern  unsigned short  GetDS( void );
#pragma aux GetDS =     \
        "mov    ax,ds"  \
    value [ax]

extern  int SetBlock( unsigned short selector, unsigned size );
#pragma aux SetBlock =      \
        "push   es"         \
        "mov    es,ax"      \
        "mov    ah,04ah"    \
        "int    021h"       \
        "rcl    eax,1"      \
        "ror    eax,1"      \
        "pop    es"         \
    parm caller [ax] [ebx]  \
    modify [ebx] value [eax]

extern  int SegInfo( unsigned short selector );
#pragma aux SegInfo =           \
        "mov    ah,0edH"        \
        "int    021h"           \
        "shl    eax,31"         \
        "and    edi,0000FFFFh"  \
        "or     edi,eax"        \
    parm caller [ebx] value [edi] \
    modify exact [eax ecx edx esi ebx edi]

extern  int SegmentLimit( void );
#pragma aux SegmentLimit =  \
        "xor    eax,eax"    \
        "mov    ax,ds"      \
        "lsl    eax,ax"     \
        "inc    eax"        \
    value [eax] modify exact [eax]

#endif

#endif

#if defined(__WINDOWS_386__)

_WCRTLINK void_nptr sbrk( int increment )
{
    increment = __ROUND_UP_SIZE_4K( increment );
    return( (void_nptr)DPMIAlloc( increment ) );
}

#elif defined(__WINDOWS__)

_WCRTLINK void_nptr sbrk( int increment )
{
    HANDLE h;

    if( increment > 0 ) {
        h = LocalAlloc( LMEM_FIXED, increment );
        if( h != NULL ) {
            return( (void_nptr)h );
        }
        _RWD_errno = ENOMEM;
    } else {
        _RWD_errno = EINVAL;
    }
    return( (void_nptr)-1 );
}

#elif defined(__OSI__)

_WCRTLINK void_nptr sbrk( int increment )
{
    increment = __ROUND_UP_SIZE_4K( increment );
    return( (void_nptr)TinyMemAlloc( increment ) );
}

#else

_WCRTLINK void_nptr __brk( unsigned brk_value )
{
    unsigned        old_brk_value;
    unsigned short  segm;
    unsigned        segm_size;

    if( brk_value < _STACKTOP ) {
        _RWD_errno = ENOMEM;
        return( (void_nptr)-1 );
    }
    segm = _DGroup();
#ifdef _M_I86
    segm_size = __ROUND_UP_SIZE_TO_PARA( brk_value );
    if( segm_size == 0 ) {
        segm_size = PARAS_IN_64K;
    }
    /* try setting the block of memory */
    if( _RWD_osmode == DOS_MODE ) {
        segm_size += SS_Reg() - _RWD_psp;    /* add in code size (in paragraphs) */
        segm = _RWD_psp;
    }
#else
    if( _IsOS386() ) {
        int parent;

        segm_size = __ROUND_UP_SIZE_TO_PARA( brk_value );
        if( segm_size == 0 )
            segm_size = 0x0FFFFFFF;
        parent = SegInfo( segm );
        if( parent < 0 ) {
            if( SetBlock( parent & 0xffff, segm_size ) < 0 ) {
                _RWD_errno = ENOMEM;
                return( (void_nptr)-1 );
            }
        }
    } else {        /* _IsPharLap() || IsRationalNonZeroBase() */
        segm_size = __ROUND_UP_SIZE_TO_4K( brk_value );
        if( segm_size == 0 )
            segm_size = 0x000FFFFF;
        if( _IsRationalNonZeroBase() ) {
            // convert from 4k pages to paragraphs
            segm_size = segm_size * 256U;
        }
    }
#endif
    if( SetBlock( segm, segm_size ) < 0 ) {
        _RWD_errno = ENOMEM;
        return( (void_nptr)-1 );
    }
    old_brk_value = _curbrk;        /* return old value of _curbrk */
    _curbrk = brk_value;            /* set new break value */

    return( (void_nptr)old_brk_value );
}

_WCRTLINK void_nptr sbrk( int increment )
{
#ifdef __386__
    if( _IsRationalZeroBase() || _IsCodeBuilder() ) {
        void_nptr   cstg;

        if( increment > 0 ) {
            increment = __ROUND_UP_SIZE_4K( increment );
            if( _IsRational() ) {
                cstg = TinyDPMIAlloc( increment );
            } else {
                cstg = TinyCBAlloc( increment );
            }
            if( cstg == NULL ) {
                _RWD_errno = ENOMEM;
                cstg = (void_nptr)-1;
            }
        } else {
            _RWD_errno = EINVAL;
            cstg = (void_nptr)-1;
        }
        return( cstg );
    } else if( _IsPharLap() ) {
        _curbrk = SegmentLimit();
    }
#endif
    return( __brk( _curbrk + increment ) );
}

#endif
