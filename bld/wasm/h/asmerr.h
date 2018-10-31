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
* Description:  Assembler message output interface.
*
****************************************************************************/


#ifndef _ASMERR_H_INCLUDED
#define _ASMERR_H_INCLUDED

#if defined( _STANDALONE_ )
    #include "asminput.h"
#endif

#ifdef DEBUG_OUT
    extern void DoDebugMsg( const char *format, ... );
    #define DebugMsg( x ) DoDebugMsg x
#else
    #define DebugMsg( x )
#endif
// use DebugMsg((....)) to call it

#define         AsmWarning( errno )             AsmWarn( 0,errno )

extern void             AsmErr( int msgnum, ... );
extern void             AsmWarn( int level, int msgnum, ... );
extern void             AsmNote( int level, int msgnum, ... );
extern void             _AsmNote( int level, int msgnum, ... );

#if !defined( _STANDALONE_ )
    #define DebugCurrLine()
    #define AsmIntErr( x )
#elif DEBUG_OUT
    #define DebugCurrLine() printf( "%s\n", CurrString );
    #define AsmIntErr( x ) DebugCurrLine(); printf( "Internal error = %d\n", x )
#else
    #define DebugCurrLine()
    #define AsmIntErr( x ) printf( "Internal error = %d\n", x )
#endif

#if defined( _STANDALONE_ )

#include "asmrcmsg.h"

    extern bool MsgInit( void );
    extern bool MsgGet( int, char * );
    extern void MsgFini( void );
    extern void OpenLstFile( void );
    extern void LstMsg( const char *format, ... );
    extern void PrintfUsage( void );
    extern void MsgPrintf( int resourceid );
    extern void MsgPrintf1( int resourceid, const char *token );
    extern int  PrintBanner( void );


#if !defined( USE_TEXT_MSGS )

    extern void MsgPutUsage( void );
    extern void MsgSubStr( char *, char *, char );
    extern void MsgChgeSpec( char *strptr, char specifier );

#endif

#elif defined( _USE_RESOURCES_ )

    #define MSG_RC_BASE         15000
    #include "msg.gh"

#else
    /* set up the enum for error messages */

    #define pick(c,e,j)         asmerr(c,e),
  #ifndef asmerr
    #define asmerr(code,emsg)   code
    enum    asmerr_codes {
  #else
    static char const ASMFAR * const ASMFAR AsmErrMsgs[] = {
  #endif
        #include "asmshare.msg"
    };
    #undef pick
#endif

#endif
