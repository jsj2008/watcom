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
* Description:  Spawn another application.
*
****************************************************************************/


#include "vi.h"
#include <dos.h>
#ifdef __WIN__
#include "wclbproc.h"
#endif


/* Local Windows CALLBACK function prototypes */
WINEXPORT BOOL CALLBACK NotifyHandler( WORD id, DWORD data );

#define MODULE_FROM_TASK( t )   (*((WORD __far *)MK_FP( (t), 0x1e )))
#define INSTANCE_FROM_TASK( t ) (*((WORD __far *)MK_FP( (t), 0x1c )))

#ifdef __WINDOWS_386__
typedef BOOL (CALLBACK *LPFNNOTIFYCALLBACKx)( WORD, DWORD );
#endif

static bool     doneExec;
static HMODULE  moduleHandle;
static HMODULE  instanceHandle;

#ifdef __WINDOWS_386__
static FARPROC MakeNotifyCallbackProcInstance( LPFNNOTIFYCALLBACKx fn, HINSTANCE instance )
{
    instance = instance;
    return( MakeProcInstance( (FARPROCx)fn, instance ) );
}
#else
#define LPFNNOTIFYCALLBACKx LPFNNOTIFYCALLBACK
#define MakeNotifyCallbackProcInstance(f,i) MakeProcInstance((FARPROC)f,i)
#endif

WINEXPORT BOOL CALLBACK NotifyHandler( WORD id, DWORD data )
{
    UINT        task;

    if( id == NFY_DELMODULE ) {
        if( (HMODULE)LOWORD( data ) == moduleHandle ) {
            doneExec = true;
        }
    } else if( id == NFY_EXITTASK ) {
        task = (UINT)GetCurrentTask();
        if( MODULE_FROM_TASK( task ) == (WORD)moduleHandle &&
            INSTANCE_FROM_TASK( task ) == (WORD)instanceHandle ) {
            doneExec = true;
        }
    }
    return( FALSE );
}

/*
 * MySpawn - spawn a windows app
 */
long MySpawn( const char *cmd )
{
    FARPROC             proc;
    HANDLE              inst;
    cmd_struct          cmds;
    char                path[_MAX_PATH];
#ifndef __WINDOWS_386__
    char                buffer[FILENAME_MAX];
#endif
    int                 rc;

    GetSpawnCommandLine( path, cmd, &cmds );
    cmds.cmd[cmds.len] = '\0';
    proc = MakeNotifyCallbackProcInstance( NotifyHandler, InstanceHandle );
    if( !NotifyRegister( (HANDLE)NULLHANDLE, (LPFNNOTIFYCALLBACK)proc, NF_NORMAL ) ) {
        FreeProcInstance( proc );
        return( -1L );
    }
    strcat( path, " " );
    strcat( path, cmds.cmd );
    inst = (HANDLE)WinExec( (LPCSTR)path, SW_SHOWNORMAL );
    if( inst > (HANDLE)32 ) {
        union REGS in_regs, out_regs;

        doneExec = false;
#ifdef __WINDOWS_386__
        moduleHandle = GetModuleHandle( PASS_WORD_AS_POINTER( inst ) );
#else
        GetModuleFileName( inst, buffer, FILENAME_MAX - 1 );
        moduleHandle = GetModuleHandle( buffer );
#endif

        // waiting doesn't work under win-os2 so don't wait!
        in_regs.h.ah = 0x30;
        in_regs.h.al = 0x0;
        intdos( &in_regs, &out_regs );
        if( out_regs.h.al == 20 ) {
            doneExec = true;
        }

        instanceHandle = inst;
        EditFlags.HoldEverything = true;
        while( !doneExec ) {
            MessageLoop( true );
            Yield();
        }
        EditFlags.HoldEverything = false;
        rc = 0;
    } else {
        rc = -1L;
    }
    NotifyUnRegister( (HANDLE)NULLHANDLE );
    FreeProcInstance( proc );
    return( rc );

} /* MySpawn */
