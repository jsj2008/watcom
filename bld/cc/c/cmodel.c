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
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include "cvars.h"
#include "scan.h"
#include "pdefn2.h"
#include <ctype.h>

extern  char    CompilerID[];


/* COMMAND LINE PARSING OF MACRO DEFINITIONS */


static size_t get_namelen( const char *start )
{
    char        c;
    const char  *src;

    for( src = start; (c = *src) != '\0'; ++src ) {
        if( EqualChar( c ) )
            break;
        if( c == ' ' )
            break;
        if( c == '-' )
            break;
        if( c == SwitchChar ) {
            break;
        }
    }
    return( src - start );
}


char *BadCmdLine( int error_code, const char *str )
{
    char        *p;
    auto char   buffer[128];

    p = buffer;
    for( ; *str != '\0'; ) {
        *p++ = *str++;
        if( *str == ' ' )
            break;
        if( *str == '-' )
            break;
        if( *str == SwitchChar ) {
            break;
        }
    }
    *p = '\0';
    CBanner();
    CErr2p( error_code, buffer );
    return( (char *)str );
}


static char *Def_Macro_Tokens( const char *str, bool multiple_tokens, macro_flags mflags )
{
    size_t      len;
    MEPTR       mentry;

    len = get_namelen( str );
    if( len == 0 ) {
        CErr1( ERR_NO_MACRO_ID_COMMAND_LINE );
        return( (char *)str );
    }
    mentry = CreateMEntry( str, len );
    str += len;
    len = 0;
    if( !EqualChar( *str ) ) {
        MTOK( TokenBuf + len ) = T_PPNUMBER;
        MTOKINC( len );
        TokenBuf[len++] = '1';
        TokenBuf[len++] = '\0';
    } else {
        bool ppscan_mode;

        ppscan_mode = InitPPScan();
        ReScanInit( ++str );
        for( ; *str != '\0'; ) {
            ReScanToken();
            if( ReScanPos() == str )
                break;
            if( CurToken == T_WHITE_SPACE )
                break;
            if( CurToken == T_BAD_CHAR && !multiple_tokens )
                break;
            MTOK( TokenBuf + len ) = CurToken;
            MTOKINC( len );
            switch( CurToken ) {
            case T_BAD_CHAR:
                TokenBuf[len++] = Buffer[0];
                break;
            case T_CONSTANT:
            case T_PPNUMBER:
            case T_ID:
            case T_LSTRING:
            case T_STRING:
                memcpy( &TokenBuf[len], &Buffer[0], TokenLen + 1 );
                len += TokenLen + 1;
                break;
            default:
                break;
            }
            str = ReScanPos();
            if( !multiple_tokens ) {
                break;
            }
        }
        FiniPPScan( ppscan_mode );
    }
    MTOK( TokenBuf + len ) = T_NULL;
    MTOKINC( len );
    if( CMPLIT( mentry->macro_name, "defined" ) != 0 ) {
        MacroAdd( mentry, TokenBuf, len, mflags );
    } else {
        CErr1( ERR_CANT_DEFINE_DEFINED );
    }
    FreeMEntry( mentry );
    return( (char *)str );
}

char *Define_Macro( const char *str )
{
    return( Def_Macro_Tokens( str, CompFlags.extended_defines, MFLAG_NONE ) );
}

char *Define_UserMacro( const char *str )
{
    return( Def_Macro_Tokens( str, CompFlags.extended_defines, MFLAG_USER_DEFINED ) );
}

void PreDefine_Macro( const char *str )
{
    undef_names     *uname;
    size_t          len;
    const char      *p;

    if( !CompFlags.undefine_all_macros ) {
        if( UndefNames != NULL ) {
            len = 0;
            for( p = str; *p != '\0'; ++p ) {
                if( EqualChar( *p ) )
                    break;
                ++len;
            }
            for( uname = UndefNames; uname != NULL; uname = uname->next ) {
                if( memcmp( uname->name, str, len ) == 0 ) {
                    if( uname->name[len] == '\0' ) {
                        return;
                    }
                }
            }
        }
        Def_Macro_Tokens( str, true, MFLAG_CAN_BE_REDEFINED );
    }
}


char *AddUndefName( const char *str )
{
    size_t          len;
    undef_names     *uname;

    len = strlen( str );
    if( len == 0 ) {
        CompFlags.undefine_all_macros = true;
    } else {
        CalcHash( str, len );
        if( !MacroDel( str ) ) {
            uname = (undef_names *)CMemAlloc( sizeof( undef_names ) );
            uname->next = UndefNames;
            uname->name = CMemAlloc( len + 1 );
            memcpy( uname->name, str, len + 1 );
            UndefNames = uname;
            str += len;
        }
    }
    return( (char *)str );
}


static void FreeUndefNames( void )
{
    undef_names     *uname;

    for( ; (uname = UndefNames) != NULL; ) {
        UndefNames = uname->next;
        CMemFree( uname->name );
        CMemFree( uname );
    }
}


static void Define_Macros_Extension( void )
{
    PreDefine_Macro( "_far16=__far16" );
    PreDefine_Macro( "near=__near" );
    PreDefine_Macro( "far=__far" );
    PreDefine_Macro( "huge=__huge" );
    PreDefine_Macro( "cdecl=__cdecl" );
    PreDefine_Macro( "pascal=__pascal" );
    PreDefine_Macro( "fortran=__fortran" );
    PreDefine_Macro( "interrupt=__interrupt" );
    PreDefine_Macro( "_near=__near" );
    PreDefine_Macro( "_far=__far" );
    PreDefine_Macro( "_huge=__huge" );
    PreDefine_Macro( "_cdecl=__cdecl" );
    PreDefine_Macro( "_pascal=__pascal" );
    PreDefine_Macro( "_fortran=__fortran" );
    PreDefine_Macro( "_inline=__inline" );
    PreDefine_Macro( "_fastcall=__fastcall" );
    PreDefine_Macro( "_interrupt=__interrupt" );
    PreDefine_Macro( "_export=__export" );
    PreDefine_Macro( "_loadds=__loadds" );
    PreDefine_Macro( "_saveregs=__saveregs" );
    PreDefine_Macro( "_stdcall=__stdcall" );
    PreDefine_Macro( "_syscall=__syscall" );
    PreDefine_Macro( "_based=__based" );
    PreDefine_Macro( "_self=__self" );
    PreDefine_Macro( "_segname=__segname" );
    PreDefine_Macro( "_segment=__segment" );
    PreDefine_Macro( "_try=__try");
    PreDefine_Macro( "_except=__except");
    PreDefine_Macro( "_finally=__finally");
    PreDefine_Macro( "_leave=__leave");
    PreDefine_Macro( "_asm=__asm");
#if _CPU == 8086
    /* SOM for Windows macros */
    PreDefine_Macro( "SOMLINK=__cdecl" );
    PreDefine_Macro( "SOMDLINK=__far" );
#else
    PreDefine_Macro( "SOMLINK=_Syscall" );
    PreDefine_Macro( "SOMDLINK=_Syscall" );
#endif
}


void MiscMacroDefs( void )
{
    if( CompFlags.inline_functions ) {
        Define_Macro( "__INLINE_FUNCTIONS__" );
    }
    if( CompFlags.oldmacros_enabled ) {
        Define_Macros_Extension();
    }
    if( !CompFlags.extensions_enabled ) {
        Define_Macro( "NO_EXT_KEYS" );
    }
    if( CompFlags.signed_char ) {
        Define_Macro( "__CHAR_SIGNED__" );
    }
    if( CompFlags.rent ) {
        Define_Macro( "__RENT__" );
    }
    PreDefine_Macro( "_PUSHPOP_SUPPORTED" );
    PreDefine_Macro( CompilerID );
    FreeUndefNames();
}

void InitModInfo( void )
{
    GenSwitches = 0;
    TargetSwitches = 0;
    Toggles = TOGGLE_CHECK_STACK
            | TOGGLE_UNREFERENCED
            | TOGGLE_REUSE_DUPLICATE_STRINGS;
    DataThreshold = 32767;
    OptSize = 50;
    UndefNames = NULL;
    WholeFName = NULL;
    ObjectFileName = NULL;
    ErrorFileName = CStrSave( "*" );
    DependFileName = NULL;
    DependForceSlash = 0;
    ModuleName = NULL;
    ErrLimit = 20;
    WngLevel = 1;
#if _CPU == 8086
    PackAmount = TARGET_INT;     /* pack structs on word boundaries */
#elif _CPU == 386
    PackAmount = 8;
#else
    CompFlags.make_enums_an_int = true;     // make enums ints
    CompFlags.original_enum_setting = true;
    PackAmount = 8;
#endif
    PreProcChar = '#';
    CompFlags.check_syntax              = false;
    CompFlags.signed_char               = false;
    CompFlags.use_full_codegen_od       = false;
    CompFlags.inline_functions          = false;
    CompFlags.dump_prototypes           = false;
    CompFlags.generate_prototypes       = false;
    CompFlags.bss_segment_used          = false;
    CompFlags.undefine_all_macros       = false;
    CompFlags.extensions_enabled        = true;
    CompFlags.oldmacros_enabled         = true;
    CompFlags.unix_ext                  = false;
    CompFlags.slack_byte_warning        = false;
    CompFlags.errfile_written           = false;
    CompFlags.zu_switch_used            = false;
    CompFlags.register_conventions      = false;
    CompFlags.pragma_library            = false;
    CompFlags.emit_all_default_libs     = false;
    CompFlags.emit_library_names        = true;
    CompFlags.emit_dependencies         = true;
    CompFlags.emit_targimp_symbols      = true;
    CompFlags.use_unicode               = true;
    CompFlags.no_debug_type_names       = false;
    CompFlags.auto_agg_inits            = false;
    CompFlags.no_check_inits            = false;
    CompFlags.no_check_qualifiers       = false;
    CompFlags.ignore_default_dirs       = false;
    CompFlags.use_stdcall_at_number     = true;
    CompFlags.rent                      = false;
    CompFlags.check_truncated_fnames    = true;

    SetAuxWatcallInfo();
}
