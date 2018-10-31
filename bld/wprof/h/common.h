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
* Description:  Common includes and definitions for profiler.
*
****************************************************************************/


#ifndef COMMON_H
#define COMMON_H    1

#include <stddef.h>
#include <stdlib.h>
#include "watcom.h"
#include "bool.h"

#define STATIC          static
#define MAX_LONG_DIGITS (16)

#define NULLCHAR        0

#define ArraySize( x )  (sizeof( x ) / sizeof( (x)[0] ))

#if defined(__UNIX__)
#define ALLFILES        "All Files\0*\0"
#else
#define ALLFILES        "All Files\0*.*\0"
#endif

#if defined(__LARGE__)
#include <malloc.h>
#define _MALLOC     _fmalloc
#define _REALLOC    _frealloc
#define _FREE       _ffree
#else
#define _MALLOC     malloc
#define _REALLOC    realloc
#define _FREE       free
#endif

typedef void *          pointer;

#endif