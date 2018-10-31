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
* Description:  WLIB DLL version mainline.
*
****************************************************************************/


#include "wlib.h"
#include "idedll.h"
#ifdef __WATCOMC__
    #include <malloc.h> // for _heapshrink()
#endif
#include "banner.h"
#include "main.h"

#include "clibext.h"


static IDECBHdl     IdeHdl;
static IDECallBacks *IdeCbs;
static IDEInitInfo  *ideInfo;

unsigned IDEAPI IDEGetVersion( void )
{
    return( IDE_CUR_DLL_VER );
}

IDEBool IDEAPI IDEInitDLL( IDECBHdl cbhdl, IDECallBacks *cb, IDEDllHdl *hdl )
{
    IdeHdl = cbhdl;
    IdeCbs = cb;
    *hdl = NULL;
    return( InitSubSystems() != 0 );
}

IDEBool IDEAPI IDEPassInitInfo( IDEDllHdl hdl, IDEInitInfo *info )
{
    /* unused parameters */ (void)hdl;

    ideInfo = info;
    return( false );
}

IDEBool IDEAPI IDERunYourSelf( IDEDllHdl hdl, const char *opts, IDEBool *fatalerr )
{
    char        *argv[3];

    /* unused parameters */ (void)hdl;

    *fatalerr = false;
    argv[0] = "";
    argv[1] = (char *)opts;
    argv[2] = NULL;
    return( WlibMainLine( argv ) != 0 );
}

IDEBool IDEAPI IDERunYourSelfArgv(// COMPILE A PROGRAM (ARGV ARGS)
    IDEDllHdl hdl,              // - handle for this instantiation
    int argc,                   // - # of arguments
    char **argv,                // - argument vector
    IDEBool* fatal_error )      // - addr[fatality indication]
{
    /* unused parameters */ (void)hdl; (void)argc;

    *fatal_error = false;
    return( WlibMainLine( argv ) != 0 );
}

void IDEAPI IDEStopRunning( void )
{
    if( ideInfo == NULL || ideInfo->ver <= 2 || ideInfo->console_output ) {
        exit( 1 );
    } else {
        longjmp( Env, 1 );
    }
}

void IDEAPI IDEFreeHeap( void )
{
#ifdef __WATCOMC__
    _heapshrink();
#endif
}

void IDEAPI IDEFiniDLL( IDEDllHdl hdl )
{
    /* unused parameters */ (void)hdl;

    FiniSubSystems();
}

char *WlibGetEnv( const char *name )
{
    char *env;

    if( !ideInfo->ignore_env && IdeCbs != NULL ) {
        if( !IdeCbs->GetInfo( IdeHdl, IDE_GET_ENV_VAR, (IDEGetInfoWParam)name, (IDEGetInfoLParam)&env ) ) {
            return( env );
        }
    }
    return( NULL );

}
void FatalResError( void )
{
    IDEMsgInfo          msg_info;

    if( IdeCbs != NULL ) {
        msg_info.severity = IDEMSGSEV_ERROR;
        msg_info.flags = 0;
        msg_info.helpfile = NULL;
        msg_info.helpid = 0;
        msg_info.msg = NO_RES_MESSAGE;
        IdeCbs->PrintWithInfo( IdeHdl, &msg_info );
    }
    longjmp( Env, 1 );
}

void FatalError( int str, ... )
{
    va_list             arglist;
    char                buff[MAX_ERROR_SIZE];
    char                msg[512];
    IDEMsgInfo          msg_info;

    va_start( arglist, str );
    MsgGet( str, buff );
    vsnprintf( msg, 512, buff, arglist );
    if( IdeCbs != NULL ) {
        IdeMsgInit( &msg_info, IDEMSGSEV_ERROR, msg );
        IdeCbs->PrintWithInfo( IdeHdl, &msg_info );
    }
    va_end( arglist );
    longjmp( Env, 1 );
}

void Warning( int str, ... )
{
    va_list             arglist;
    char                buff[MAX_ERROR_SIZE];
    char                msg[512];
    IDEMsgInfo          msg_info;

    if( Options.quiet )
        return;
    MsgGet( str, buff );
    va_start( arglist, str );
    vsnprintf( msg, 512, buff, arglist );
    if( IdeCbs != NULL ) {
        IdeMsgInit( &msg_info, IDEMSGSEV_WARNING, msg );
        IdeCbs->PrintWithInfo( IdeHdl, &msg_info );
    }
    va_end( arglist );
}

void Message( char *buff, ... )
{
    va_list             arglist;
    char                msg[512];
    IDEMsgInfo          msg_info;

    if( Options.quiet )
        return;
    va_start( arglist, buff );
    vsnprintf( msg, 512, buff, arglist );
    if( IdeCbs != NULL ) {
        IdeMsgInit( &msg_info, IDEMSGSEV_NOTE_MSG, msg );
        IdeCbs->PrintWithInfo( IdeHdl, &msg_info );
    }
    va_end( arglist );
}

#define NUM_ROWS        24

static void ConsoleMessage( const char *msg )
{
    static IDEMsgInfo   msg_info = { IDEMSGSEV_BANNER, 0, NULL, 0, NULL };
    msg_info.msg = msg;
    IdeCbs->PrintWithInfo( IdeHdl, &msg_info );
}

static bool Wait_for_return( void )
/*********************************/
// return true if we should stop printing
{
    int         c;

    ConsoleMessage( "    (Press Return to continue)" );
    c = getchar();
    return( c == 'q' || c == 'Q' );
}

void Usage( void )
/****************/
{
    char                buff[MAX_ERROR_SIZE];
    int                 str;
    int                 str_first;
    int                 str_last;
    int                 count;
    bool                console_tty;

    count = Banner();
    if( IdeCbs != NULL ) {
        console_tty = ( ideInfo != NULL && ideInfo->ver > 2 && ideInfo->console_output );
        if( console_tty && count ) {
            ConsoleMessage( "" );
            ++count;
        }
        if( Options.ar ) {
            str_first = USAGE_AR_FIRST;
            str_last = USAGE_AR_LAST;
        } else {
            str_first = USAGE_WLIB_FIRST;
            str_last = USAGE_WLIB_LAST;
        }
        for( str = str_first; str <= str_last; ++str ) {
            if( console_tty ) {
                if( count == NUM_ROWS - 2 ) {
                    if( Wait_for_return() )
                        break;
                    count = 0;
                }
                ++count;
            }
            MsgGet( str, buff );
            ConsoleMessage( buff );
        }
    }
    longjmp( Env, 1 );
}

int Banner( void )
{
    static char *bannerText[] = {
#ifndef NDEBUG
        banner1w( "Library Manager", _WLIB_VERSION_ ) " [Internal Development]",
#else
        banner1w( "Library Manager", _WLIB_VERSION_ ),
#endif
        banner2,
        banner2a( 1984 ),
        banner3,
        banner3a,
        NULL
    };
    static bool alreadyDone = false;
    char        **text;
    int         count;
    char        *p;

    count = 0;
    if( !Options.quiet && !alreadyDone && !Options.terse_listing ) {
        alreadyDone = true;
        if( IdeCbs != NULL ) {
            text = bannerText;
            while( (p = *text++) != NULL ) {
                ConsoleMessage( p );
                ++count;
            }
        }
    }
    return( count );
}
