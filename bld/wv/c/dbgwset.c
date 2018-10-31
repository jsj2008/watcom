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
* Description:  Debugger interface configuration.
*
****************************************************************************/


#include <ctype.h>
#include "dbgdefn.h"
#include "dbgdata.h"
#include "dbgwind.h"
#include "dbgerr.h"
#include "madinter.h"
#include "dbgmem.h"
#include "wndregx.h"
#include "dbgio.h"
#include "strutil.h"
#include "dbgscan.h"
#include "dbgutil.h"
#include "dbgmain.h"
#include "dbginvk.h"
#include "dbgcall.h"
#include "dbgshow.h"
#include "dbgparse.h"
#include "dbgwdlg.h"
#include "wndsys.h"
#include "dbgtrace.h"
#include "dbgupdt.h"
#include "dbglkup.h"
#include "dbgwass.h"
#include "dbgwfunc.h"
#include "dbgwglob.h"
#include "dbgwmod.h"
#include "dbgwset.h"
#include "dbgwvar.h"
#include "dbgwdisp.h"
#include "wndmenu.h"
#include "dbgsetfg.h"

#include "clibext.h"


extern void             WndUserAdd( char *, unsigned int );
extern void             WndRestoreToFront( a_window );

extern margins          SrcMar;
extern margins          AsmMar;

static unsigned         TabInterval = 8;


void DClickSet( void )
{
    unsigned    value;
    mad_radix   old_radix;

    old_radix = SetCurrRadix( 10 );
    value = ReqExpr();
    ReqEOC();
    if( value < MIN_DCLICK || value > MAX_DCLICK )
        Error( ERR_NONE, LIT_DUI( ERR_BAD_DCLICK ) );
    WndSetDClick( value );
    SetCurrRadix( old_radix );
}


void DClickConf( void )
{
    CnvULongDec( WndGetDClick(), TxtBuff, TXT_LEN );
    ConfigLine( TxtBuff );
}


void InputSet( void )
{
    wnd_class_wv    wndclass;
    a_window        wnd;

    wndclass = ReqWndName();
    ReqEOC();
    wnd = WndFindClass( NULL, wndclass );
    if( wnd == NULL ) {
        GetCmdEntry( WndNameTab, (int)wndclass, TxtBuff );
        Error( ERR_NONE, LIT_DUI( ERR_WIND_NOT_OPEN ), TxtBuff );
    }
    WndRestoreToFront( wnd );
}


void InputConf( void )
{
    a_window  wnd;

    wnd = WndFindActive();
    if( wnd != NULL && WndHasClass( wnd ) ) {
        GetCmdEntry( WndNameTab, WndClass( wnd ), TxtBuff );
        ConfigLine( TxtBuff );
    }
}

static char     *KeyNamePieces[] =
{
    #define pick( x ) #x,
    #include "keynames.h"
    #undef pick
};

typedef enum {
    #define pick( x ) x,
    #include "keynames.h"
    #undef pick
    CTRL        = 0x2000,
    SHIFT       = 0x4000,
    ALT         = 0x8000,
} key_desc;
#define   STATE  (CTRL+ALT+SHIFT)

typedef struct {
    key_desc    desc;
    gui_key     key;
} key_name;

static key_name KeyNames[] = {
    ENTER,              GUI_KEY_ENTER,
    ESCAPE,             GUI_KEY_ESCAPE,
    BACKSPACE,          GUI_KEY_BACKSPACE,
    TAB,                GUI_KEY_TAB,
    CTRL+ENTER,         GUI_KEY_CTRL_ENTER,
    CTRL+BACKSPACE,     GUI_KEY_CTRL_BACKSPACE,
    CTRL+TAB,           GUI_KEY_CTRL_TAB,
    ALT+ENTER,          GUI_KEY_ALT_ENTER,
    ALT+BACKSPACE,      GUI_KEY_ALT_BACKSPACE,
    ALT+TAB,            GUI_KEY_ALT_TAB,
    SHIFT+TAB,          GUI_KEY_SHIFT_TAB,
    INSERT,             GUI_KEY_INSERT,
    DELETE,             GUI_KEY_DELETE,
    HOME,               GUI_KEY_HOME,
    END,                GUI_KEY_END,
    PAGEUP,             GUI_KEY_PAGEUP,
    PAGEDOWN,           GUI_KEY_PAGEDOWN,
    UP,                 GUI_KEY_UP,
    DOWN,               GUI_KEY_DOWN,
    LEFT,               GUI_KEY_LEFT,
    RIGHT,              GUI_KEY_RIGHT,
    CTRL+INSERT,        GUI_KEY_CTRL_INSERT,
    CTRL+DELETE,        GUI_KEY_CTRL_DELETE,
    CTRL+HOME,          GUI_KEY_CTRL_HOME,
    CTRL+END,           GUI_KEY_CTRL_END,
    CTRL+PAGEUP,        GUI_KEY_CTRL_PAGEUP,
    CTRL+PAGEDOWN,      GUI_KEY_CTRL_PAGEDOWN,
    CTRL+UP,            GUI_KEY_CTRL_UP,
    CTRL+DOWN,          GUI_KEY_CTRL_DOWN,
    CTRL+LEFT,          GUI_KEY_CTRL_LEFT,
    CTRL+RIGHT,         GUI_KEY_CTRL_RIGHT,
    F1,                 GUI_KEY_F1,
    F2,                 GUI_KEY_F2,
    F3,                 GUI_KEY_F3,
    F4,                 GUI_KEY_F4,
    F5,                 GUI_KEY_F5,
    F6,                 GUI_KEY_F6,
    F7,                 GUI_KEY_F7,
    F8,                 GUI_KEY_F8,
    F9,                 GUI_KEY_F9,
    F10,                GUI_KEY_F10,
    F11,                GUI_KEY_F11,
    F12,                GUI_KEY_F12,
    ALT+F1,             GUI_KEY_ALT_F1,
    ALT+F2,             GUI_KEY_ALT_F2,
    ALT+F3,             GUI_KEY_ALT_F3,
    ALT+F4,             GUI_KEY_ALT_F4,
    ALT+F5,             GUI_KEY_ALT_F5,
    ALT+F6,             GUI_KEY_ALT_F6,
    ALT+F7,             GUI_KEY_ALT_F7,
    ALT+F8,             GUI_KEY_ALT_F8,
    ALT+F9,             GUI_KEY_ALT_F9,
    ALT+F10,            GUI_KEY_ALT_F10,
    ALT+F11,            GUI_KEY_ALT_F11,
    ALT+F12,            GUI_KEY_ALT_F12,
    CTRL+F1,            GUI_KEY_CTRL_F1,
    CTRL+F2,            GUI_KEY_CTRL_F2,
    CTRL+F3,            GUI_KEY_CTRL_F3,
    CTRL+F4,            GUI_KEY_CTRL_F4,
    CTRL+F5,            GUI_KEY_CTRL_F5,
    CTRL+F6,            GUI_KEY_CTRL_F6,
    CTRL+F7,            GUI_KEY_CTRL_F7,
    CTRL+F8,            GUI_KEY_CTRL_F8,
    CTRL+F9,            GUI_KEY_CTRL_F9,
    CTRL+F10,           GUI_KEY_CTRL_F10,
    CTRL+F11,           GUI_KEY_CTRL_F11,
    CTRL+F12,           GUI_KEY_CTRL_F12,
    SHIFT+F1,           GUI_KEY_SHIFT_F1,
    SHIFT+F2,           GUI_KEY_SHIFT_F2,
    SHIFT+F3,           GUI_KEY_SHIFT_F3,
    SHIFT+F4,           GUI_KEY_SHIFT_F4,
    SHIFT+F5,           GUI_KEY_SHIFT_F5,
    SHIFT+F6,           GUI_KEY_SHIFT_F6,
    SHIFT+F7,           GUI_KEY_SHIFT_F7,
    SHIFT+F8,           GUI_KEY_SHIFT_F8,
    SHIFT+F9,           GUI_KEY_SHIFT_F9,
    SHIFT+F10,          GUI_KEY_SHIFT_F10,
    SHIFT+F11,          GUI_KEY_SHIFT_F11,
    SHIFT+F12,          GUI_KEY_SHIFT_F12,
    0,          0
};

typedef struct {
    char        name;
    gui_key     key;
} alt_key_name;

static alt_key_name AltKeyNames[] = {
        '`',    GUI_KEY_ALT_BACKQUOTE,
        '1',    GUI_KEY_ALT_1,
        '2',    GUI_KEY_ALT_2,
        '3',    GUI_KEY_ALT_3,
        '4',    GUI_KEY_ALT_4,
        '5',    GUI_KEY_ALT_5,
        '6',    GUI_KEY_ALT_6,
        '7',    GUI_KEY_ALT_7,
        '8',    GUI_KEY_ALT_8,
        '9',    GUI_KEY_ALT_9,
        '0',    GUI_KEY_ALT_0,
        '-',    GUI_KEY_ALT_MINUS,
        '=',    GUI_KEY_ALT_EQUAL,
        '[',    GUI_KEY_ALT_LEFT_BRACKET,
        ']',    GUI_KEY_ALT_RIGHT_BRACKET,
        '\\',   GUI_KEY_ALT_BACKSLASH,
        ';',    GUI_KEY_ALT_SEMICOLON,
        '\'',   GUI_KEY_ALT_QUOTE,
        ',',    GUI_KEY_ALT_COMMA,
        '.',    GUI_KEY_ALT_PERIOD,
        '/',    GUI_KEY_ALT_SLASH,
        0,      0
};

static alt_key_name CtrlKeyNames[] = {
        'a',    GUI_KEY_CTRL_A,
        'b',    GUI_KEY_CTRL_B,
        'c',    GUI_KEY_CTRL_C,
        'd',    GUI_KEY_CTRL_D,
        'e',    GUI_KEY_CTRL_E,
        'f',    GUI_KEY_CTRL_F,
        'g',    GUI_KEY_CTRL_G,
        'h',    GUI_KEY_CTRL_H,
        'i',    GUI_KEY_CTRL_I,
        'j',    GUI_KEY_CTRL_J,
        'k',    GUI_KEY_CTRL_K,
        'l',    GUI_KEY_CTRL_L,
        'm',    GUI_KEY_CTRL_M,
        'n',    GUI_KEY_CTRL_N,
        'o',    GUI_KEY_CTRL_O,
        'p',    GUI_KEY_CTRL_P,
        'q',    GUI_KEY_CTRL_Q,
        'r',    GUI_KEY_CTRL_R,
        's',    GUI_KEY_CTRL_S,
        't',    GUI_KEY_CTRL_T,
        'u',    GUI_KEY_CTRL_U,
        'v',    GUI_KEY_CTRL_V,
        'w',    GUI_KEY_CTRL_W,
        'x',    GUI_KEY_CTRL_X,
        'y',    GUI_KEY_CTRL_Y,
        'z',    GUI_KEY_CTRL_Z,
        '2',    GUI_KEY_CTRL_2,
        '\\',   GUI_KEY_CTRL_BACKSLASH,
        '[',    GUI_KEY_CTRL_LEFT_BRACKET,
        ']',    GUI_KEY_CTRL_RIGHT_BRACKET,
        '6',    GUI_KEY_CTRL_6,
        '-',    GUI_KEY_CTRL_MINUS,
        0,      0
};

#define STR_CTRL        "CTRL-"
#define STR_SHIFT       "SHIFT-"
#define STR_ALT         "ALT-"

char LookUpCtrlKey( gui_key key )
{
    alt_key_name        *alt;

    for( alt = CtrlKeyNames; alt->name != 0; ++alt ) {
        if( alt->key == key ) {
            break;
        }
    }
    return( alt->name );
}

static char *AddOn( char *buff, key_desc desc )
{
    switch( desc & STATE ) {
    case CTRL:
        return( StrCopy( STR_CTRL, buff ) );
    case SHIFT:
        return( StrCopy( STR_SHIFT, buff ) );
    case ALT:
        return( StrCopy( STR_ALT, buff ) );
    default:
        return( buff );
    }
}


char *KeyName( gui_key key )
{
    static char         buff[20];
    key_name            *k;
    char                *p;
    alt_key_name        *alt;

    for( k = KeyNames; k->key != 0; ++k ) {
        if( k->key == key ) {
            p = AddOn( buff, k->desc );
            StrCopy( KeyNamePieces[k->desc & ~STATE], p );
            return( buff );
        }
    }
    if( WndKeyIsPrintChar( key ) ) {
        buff[0] = (char)key;
        buff[1] = NULLCHAR;
        return( buff );
    }
    for( alt = CtrlKeyNames; alt->name != 0; ++alt ) {
        if( alt->key == key ) {
            p = AddOn( buff, CTRL );
            *p++ = alt->name;
            *p = NULLCHAR;
            return( buff );
        }
    }
    for( alt = AltKeyNames; alt->name != 0; ++alt ) {
        if( alt->key == key ) {
            p = AddOn( buff, ALT );
            *p++ = alt->name;
            *p = NULLCHAR;
            return( buff );
        }
    }
    return( NULL );
}

static key_desc StripOff( const char **start, size_t *len,
                          const char *test, key_desc desc )
{
    size_t      tlen;

    tlen = strlen( test );
    if( strnicmp( *start, test, tlen ) == 0 ) {
        *start += tlen;
        *len -= tlen;
        return( desc );
    }
    return( 0 );
}


static gui_key MapKey( const char *start, size_t len )
{
    key_name            *k;
    key_desc            desc;
    int                 i;
    alt_key_name        *alt;
    char                ch;

    if( len == 1 ) {
        return( *start );
    }
    desc  = StripOff( &start, &len, STR_CTRL,  CTRL );
    desc |= StripOff( &start, &len, STR_SHIFT, SHIFT );
    desc |= StripOff( &start, &len, STR_ALT,   ALT );
    if( len == 1 ) {
        alt = NULL;
        if( desc == CTRL ) {
            alt = CtrlKeyNames;
        }
        if( desc == ALT ) {
            alt = AltKeyNames;
        }
        if( alt != NULL ) {
            ch = tolower( *start );
            while( alt->name != 0 ) {
                if( alt->name == ch ) {
                    return( alt->key );
                }
                ++alt;
            }
        }
    }
    for( i = 0; i < ArraySize( KeyNamePieces ); ++i ) {
        if( strnicmp( KeyNamePieces[i], start, len ) == 0 ) {
            if( strlen( KeyNamePieces[i] ) == len ) {
                desc += i;
                for( k = KeyNames; k->key != 0; ++k ) {
                    if( k->desc == desc ) {
                        return( k->key );
                    }
                }
                return( 0 );
            }
        }
    }
    return( 0 );
}


wnd_macro *MacAddDel( gui_key key, wnd_class_wv wndclass, cmd_list *cmds )
{
    wnd_macro           **owner,*curr;
    bool                is_main;

    for( owner = &WndMacroList; (curr = *owner) != NULL; owner = &curr->link ) {
        if( curr->key == key && curr->wndclass == wndclass ) {
            break;
        }
    }
    if( cmds != NULL ) {
        if( curr == NULL ) {
            _Alloc( curr, sizeof( wnd_macro ) );
            if( curr == NULL ) {
                FreeCmdList( cmds );
                Error( ERR_NONE, LIT_ENG( ERR_NO_MEMORY ) );
                return( curr );
            }
            curr->key = key;
            curr->wndclass = wndclass;
            curr->link = NULL;
            curr->menu = NULL;
            *owner = curr;
        } else {
            WndMenuSetHotKey( curr->menu, curr->type == MACRO_MAIN_MENU, LIT_ENG( Empty ) );
            FreeCmdList( curr->cmd );
        }
        curr->cmd = cmds;
        curr->menu = AddMenuAccel( KeyName( key ), cmds->buff, wndclass, &is_main );
        if( curr->menu == NULL ) {
            curr->type = MACRO_COMMAND;
        } else if( is_main ) {
            curr->type = MACRO_MAIN_MENU;
        } else {
            curr->type = MACRO_POPUP_MENU;
        }
    } else {
        if( curr != NULL ) {
            *owner = curr->link;
            WndMenuSetHotKey( curr->menu, curr->type == MACRO_MAIN_MENU, LIT_ENG( Empty ) );
            FreeCmdList( curr->cmd );
            _Free( curr );
        }
    }
    DbgUpdate( UP_MACRO_CHANGE );
    return( curr );
}


void MacroSet( void )
{
    wnd_class_wv    wndclass;
    cmd_list        *cmds;
    gui_key         key;
    const char      *start;
    size_t          len;
    bool            scanned;
    char            *p;
    const char      *q;
    int             i;


    wndclass = ReqWndName();
    key = 0;
    if( ScanItem( true, &start, &len ) ) {
        key = MapKey( start, len );
    }
    if( key == 0 )
        Error( ERR_NONE, LIT_DUI( ERR_MACRO_NOT_VALID ) );
    scanned = ScanItem( true, &start, &len );
    ReqEOC();
    if( scanned ) {
        p = TxtBuff;
        q = start;
        i = len;
        while( isspace( *q ) ) {
            ++q;
            --i;
        }
        while( --i >= 0 ) {
            if( *q != '\r' )
                *p++ = *q;
            ++q;
        }
        *p = NULLCHAR;
        cmds = AllocCmdList( TxtBuff, strlen( TxtBuff ) );
    } else {
        cmds = NULL;
    }
    MacAddDel( key, wndclass, cmds );
}

void    MacroConf( void )
{
    char        wnd_name[20];
    wnd_macro   *mac;
    char        *fmt;

    for( mac = WndMacroList; mac != NULL; mac = mac->link ) {
        GetCmdEntry( WndNameTab, mac->wndclass, wnd_name );
        if( TxtBuff[0] == NULLCHAR )
            break;
        fmt = isspace( mac->key ) ? "%s {%s} {" : "%s %s {";
        Format( TxtBuff, fmt, wnd_name, KeyName( mac->key ) );
        ConfigLine( TxtBuff );
        ConfigCmdList( ((cmd_list*)mac->cmd)->buff, 0 );
        WndDlgTxt( "}" );
    }
}

void    FiniMacros( void )
{
    wnd_macro   *mac;
    wnd_macro   *junk;

    mac = WndMacroList;
    while( mac != NULL ) {
        junk = mac;
        WndMenuSetHotKey( mac->menu, mac->type == MACRO_MAIN_MENU, LIT_ENG( Empty ) );
        mac = mac->link;
        FreeCmdList( junk->cmd );
        _Free( junk );
    }
    WndMacroList = NULL;
}


int TabIntervalGet( void )
{
    return( TabInterval );
}

void TabIntervalSet( int new )
{
    TabInterval = new;
    WndRedraw( WND_FILE );
    WndRedraw( WND_SOURCE );
}

void TabSet( void )
{
    int         value;
    mad_radix   old_radix;

    old_radix = SetCurrRadix( 10 );
    value = ReqExpr();
    ReqEOC();
    if( value < 0 || value > MAX_TAB )
        Error( ERR_NONE, LIT_DUI( ERR_BAD_TAB ) );
    TabIntervalSet( value );
    SetCurrRadix( old_radix );
}


void TabConf( void )
{
    CnvULongDec( TabInterval, TxtBuff, TXT_LEN );
    ConfigLine( TxtBuff );
}

static const char SearchSettings[] = {
    "CASEIgnore\0"
    "CASEREspect\0"
    "Rx\0"
    "NORx\0"
};

enum {
    SEARCH_IGNORE,
    SEARCH_RESPECT,
    SEARCH_RX,
    SEARCH_NORX
};


void SearchSet( void )
{
    const char  *start;
    size_t      len;

    while( CurrToken == T_DIV ) {
        Scan();
        switch( ScanCmd( SearchSettings ) ) {
        case SEARCH_IGNORE:
            WndSetSrchIgnoreCase( true );
            break;
        case SEARCH_RESPECT:
            WndSetSrchIgnoreCase( false );
            break;
        case SEARCH_RX:
            WndSetSrchRX( true );
            break;
        case SEARCH_NORX:
            WndSetSrchRX( false );
            break;
        default:
            Error( ERR_LOC, LIT_ENG( ERR_BAD_SUBCOMMAND ), GetCmdName( CMD_SET ) );
            break;
        }
    }
    if( ScanItem( true, &start, &len ) ) {
        WndSetSrchMagicChars( start, len );
    }
    ReqEOC();
}


void SearchConf( void )
{
    char        *ptr;

    ptr = TxtBuff;
    *ptr++ = '/';
    ptr = GetCmdEntry( SearchSettings, WndGetSrchIgnoreCase() ? SEARCH_IGNORE : SEARCH_RESPECT, ptr );
    *ptr++ = '/';
    ptr = GetCmdEntry( SearchSettings, WndGetSrchRX() ? SEARCH_RX : SEARCH_NORX, ptr );
    ptr = StrCopy( " {", ptr );
    ptr = StrCopy( WndGetSrchMagicChars(), ptr );
    ptr = StrCopy( "}", ptr );
    ConfigLine( TxtBuff );
}
