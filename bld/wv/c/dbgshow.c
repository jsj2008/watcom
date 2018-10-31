/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2018 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  Debugger 'show' command.
*
****************************************************************************/


#include <ctype.h>
#include "dbgdefn.h"
#include "dbgdata.h"
#include "dbgerr.h"
#include "dbglit.h"
#include "dbgtback.h"
#include "dbgmem.h"
#include "dui.h"
#include "strutil.h"
#include "dbgscan.h"
#include "dbgmain.h"
#include "dbginvk.h"
#include "dbghook.h"
#include "dbgshow.h"
#include "dbgparse.h"
#include "dbgmisc.h"
#include "addarith.h"
#include "dbglist.h"
#include "dbgset.h"


/*
 * GetCmdEntry -- get an entry from a command table
 */

char *GetCmdEntry( const char *tab, int index, char *buff )
{
    while( index-- > 0 ) {
        while( *tab != NULLCHAR )
            ++tab;
        ++tab;
    }
    for( ;; ) {
        *buff = tolower( *tab );
        if( *buff == NULLCHAR )
            break;
        ++buff;
        ++tab;
    }
    return( buff );
}


#define INDENT_AMOUNT   4

void ConfigCmdList( char *cmds, int indent )
{
    char        *p;
    char        ch;
    int         i;

    indent += INDENT_AMOUNT;
again:
    while( *cmds == ' ' )
        ++cmds;
    p = TxtBuff;
    for( i = 0; i < indent; ++i ) {
        *p++ = ' ';
    }
    for( ;; ) {
        *p++ = ch = *cmds++;
        if( ch == '{' ) {
            *p = NULLCHAR;
            DUIDlgTxt( TxtBuff );
            indent += INDENT_AMOUNT;
            goto again;
        } else if( ch == '}' ) {
            *--p = NULLCHAR;
            for( ;; ) {
                if( p == TxtBuff )
                    break;
                --p;
                if( *p != ' ' ) {
                    ++p;
                    break;
                }
                *p = NULLCHAR;
            }
            if( p != TxtBuff )
                DUIDlgTxt( TxtBuff );
            p = TxtBuff;
            for( i = 0; i < indent-INDENT_AMOUNT; ++i ) {
                *p++ = ' ';
            }
            *p++ = '}';
            *p = NULLCHAR;
            DUIDlgTxt( TxtBuff );
            indent -= INDENT_AMOUNT;
            goto again;
        } else if( ch == '\r' ) {
            --p;
        } else if( ch == NULLCHAR ) {
            if( p != TxtBuff ) {
                DUIDlgTxt( TxtBuff );
            }
            break;
        }
    }
}

void ConfigLine( char *conf )
{
    char        *buff;

    _AllocA( buff, strlen( conf ) + strlen( NameBuff ) + 2 );
    Format( buff, "%s %s", NameBuff, conf );
    DUIDlgTxt( buff );
}


void DoConfig( const char *cmd, const char *name_tab, void(**jmp_tab)( void ), void(**not_all)( void ) )
{
    int         num;
    const char  *start;
    char        *ptr;
    unsigned    i;
    int         cmdx;

    ptr = StrCopy( cmd, NameBuff );
    *ptr++ = ' ';
    if( ScanEOC() ) {
        /* show configuration on everything */
        for( num = 0; jmp_tab[num] != NULL; ++num ) {
            GetCmdEntry( name_tab, num, ptr );
            for( i = 0; not_all[i] != NULL; ++i ) {
                if( jmp_tab[num] == not_all[i] ) {
                    break;
                }
            }
            if( not_all[i] == NULL ) {
                (*jmp_tab[num])();
            }
        }
    } else {
        start = ScanPos();
        do {
            cmdx = ScanCmd( name_tab );
            if( cmdx < 0 ) {
                Format( TxtBuff, "%s %s", GetCmdName( CMD_SHOW ), cmd );
                Error( ERR_LOC, LIT_ENG( ERR_BAD_SUBCOMMAND ), TxtBuff );
            }
        } while( !ScanEOC() );
        ReScan( start );
        do {
            cmdx = ScanCmd( name_tab );
            GetCmdEntry( name_tab, cmdx, ptr );
            (*jmp_tab[cmdx])();
        } while( !ScanEOC() );
    }
}


OVL_EXTERN void BadShow( void )
{
    Error( ERR_LOC, LIT_ENG( ERR_BAD_SUBCOMMAND ), GetCmdName( CMD_SHOW ) );
}


static const char ShowNameTab[] = {
    "Paint\0"
    "Display\0"
    "Font\0"
    "Set\0"
    "Flip\0"
    "Hook\0"
    "Event\0"
    "Calls\0"
    "Types\0"
};

typedef struct {
    void        (*rtn)( void );
    bool        config; /* should info be dumped for save config call */
} show_rtn;

OVL_EXTERN void ConfigTypes( void )
{
}

static  show_rtn ShowJmpTab[] = {
    &ConfigPaint,   true,
    &ConfigDisp,    true,
    &ConfigFont,    true,
    &ConfigSet,     true,
    &ConfigFlip,    true,
    &ConfigHook,    true,
    &ConfigEvent,   false,
    &ConfigCalls,   false,
    &ConfigTypes,   false,
};

void ProcShow( void )
{
    int     cmd;

    cmd = ScanCmd( ShowNameTab );
    if( cmd < 0 ) {
        BadShow();
    } else {
        ShowJmpTab[cmd].rtn();
    }
}


OVL_EXTERN void ShowAll( void )
{
    unsigned    i;

    DUIDlgTxt( GetCmdName( CMD_CONFIGFILE ) );
    for( i = 0; i < ArraySize( ShowJmpTab ); ++i ) {
        if( ShowJmpTab[i].config ) {
            ShowJmpTab[i].rtn();
        }
    }
}

void SaveConfigToFile( const char *name )
{
    CreateInvokeFile( name, ShowAll );
}

void RestoreConfigFromFile( const char *name )
{
    InvokeAFile( name );
}
