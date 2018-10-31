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
* Description:  Fatal runtime error handler for Win32.
*
****************************************************************************/


#include "variety.h"
#include <stddef.h>
#include <io.h>
#include <windows.h>
#include "iomode.h"
#include "rtdata.h"
#include "exitwmsg.h"


_WCRTLINK _WCNORETURN void __exit_with_msg( char *msg, unsigned retcode )
{

    unsigned    len;
    char        *end;
    ULONG       written;
    char        newline[2];

    end = msg;
    for( len = 0; *end++ != '\0'; len++ )
        ;
    WriteFile( NT_STDERR_FILENO, msg, len, &written, NULL );
    newline[0] = '\r';
    newline[1] = '\n';
    WriteFile( NT_STDERR_FILENO, &newline, 2, &written, NULL );
    __exit( retcode );
    // never return
}

_WCRTLINK _WCNORETURN void __fatal_runtime_error( char *msg, unsigned retcode )
{
    if( __EnterWVIDEO( msg ) )
        __exit( retcode );
        // never return
    __exit_with_msg( msg, retcode );
    // never return
}