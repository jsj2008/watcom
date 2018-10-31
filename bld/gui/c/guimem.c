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
* Description:  Cover routines to access the trmem memory tracker
*
****************************************************************************/


#include "guiwind.h"
#include <stdlib.h>
#include "wio.h"
#include "guimem.h"
#ifdef TRMEM
    #include "trmem.h"
    #include "clibext.h"
#endif


#ifdef TRMEM
_trmem_hdl  GUIMemHandle;
static int  GUIMemFileHandle;   /* stream to put output on */

static int  GUIMemOpened = 0;

static void GUIMemPrintLine( void *handle, const char *buff, size_t len )
/***********************************************************************/
{
    posix_write( *(int *)handle, buff, len );
}

#endif

void GUIMemRedirect( int handle )
/*******************************/
{
#ifdef TRMEM
    GUIMemFileHandle = handle;
#else
    /* unused parameters */ (void)handle;
#endif
}

void GUIMemOpen( void )
/*********************/
{
#ifdef TRMEM
    char * tmpdir;

    if( !GUIMemOpened ) {
#ifdef NLM
        GUIMemFileHandle = STDERR_HANDLE;
#else
        GUIMemFileHandle = STDERR_FILENO;
#endif
        GUIMemHandle = _trmem_open( malloc, free, realloc, NULL,
            &GUIMemFileHandle, GUIMemPrintLine,
            _TRMEM_ALLOC_SIZE_0 | _TRMEM_REALLOC_SIZE_0 |
            _TRMEM_OUT_OF_MEMORY | _TRMEM_CLOSE_CHECK_FREE );

        tmpdir = getenv( "TRMEMFILE" );
        if( tmpdir != NULL ) {
            GUIMemFileHandle = open( tmpdir, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, PMODE_RW );
        }
        GUIMemOpened = 1;
    }
#endif
}

void GUIMemClose( void )
/**********************/
{
#ifdef TRMEM
    _trmem_prt_list( GUIMemHandle );
    _trmem_close( GUIMemHandle );
#ifdef NLM
    if( GUIMemFileHandle != STDERR_HANDLE ) {
#else
    if( GUIMemFileHandle != STDERR_FILENO ) {
#endif
        close( GUIMemFileHandle );
    }
#endif
}

void GUIMemPrtUsage( void )
/*************************/
{
#ifdef TRMEM
    _trmem_prt_usage( GUIMemHandle );
#endif
}

void *GUIMemAlloc( size_t size )
/******************************/
{
#ifdef TRMEM
    return( _trmem_alloc( size, _trmem_guess_who(), GUIMemHandle ) );
#else
    return( malloc( size ) );
#endif
}

void GUIMemFree( void *ptr )
/**************************/
{
#ifdef TRMEM
    _trmem_free( ptr, _trmem_guess_who(), GUIMemHandle );
#else
    free( ptr );
#endif
}

void *GUIMemRealloc( void *ptr, size_t size )
/*******************************************/
{
#ifdef TRMEM
    return( _trmem_realloc( ptr, size, _trmem_guess_who(), GUIMemHandle ) );
#else
    return( realloc( ptr, size ) );
#endif
}
