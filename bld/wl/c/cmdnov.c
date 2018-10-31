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
* Description:  Command line parsing for Novell Netware file formats.
*
****************************************************************************/


#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "linkstd.h"
#include "alloc.h"
#include "command.h"
#include "msg.h"
#include "exenov.h"
#include "loadnov.h"
#include "wlnkmsg.h"
#include "cmdnov.h"
#include "nwpfx.h"


static bool IsNetWarePrefix( const char *token, size_t tokenlen )
{
    if( ( token != NULL ) && ( token[0] == '(' ) && ( token[tokenlen - 1] == ')' ) )
        return( true );
    return( false );
}

/*
//  should move these somewhere more suitable
*/
#define IS_NUMBER(ptr)     ((*ptr >= '0') && (*ptr <= '9'))
#define IS_WHITESPACE(ptr) (*(ptr) == ' ' || *(ptr) =='\t' || *(ptr) == '\r')

static bool NetWareSplitSymbol( const char *token, size_t tokenlen, const char **name, size_t *namelen, const char **prefix, size_t *prefixlen )
{
    const char  *findAt = token;
    size_t      len;

    if( (NULL == token) || (0 == tokenlen) || (NULL == name) || (NULL == namelen) || (NULL == prefix) || (NULL == prefixlen) )
        return( false );

    *name = *prefix = NULL;
    *namelen = *prefixlen = 0;

    for( len = tokenlen; len; len-- ) {
        if( '@' == *findAt )
            break;
        if( '\0' == *findAt ) {
            len = 0;   /* force zero */
            break;
        }
        findAt++;
    }

    if( 0 == len ) {
        *name = token;
        *namelen = tokenlen;
        return( true );
    }

    /*
    //  findAt now points at an @ symbol. this maybe a stdcall designator or a prefixed symbol.
    //  if the following character is a number then it must be stdcall as it is illegal to start
    //  a function name with a numeric character (I believe)
    */

    if( IS_NUMBER( &findAt[ 1 ] ) ) {
        *name = token;
        *namelen = tokenlen;
        return( true );
    }

    if( findAt != token ) {
        *prefix = token;
        *prefixlen = findAt - token;
    }

    *name = findAt + 1;
    *namelen = len - 1;

    return( true );
}

/*
//  Trouble! In files, import and export specifiers may or may not have a trailing comma
//  so we look ahead to Token.next and see if there is a comma next (after whitespace)
//  and if there is then we don't set this flag else we do
//  this also affects us using
//      IMPORT x, (PREFIX), y, (PREFIX), x
*/
static bool DoWeNeedToSkipASeparator( bool CheckDirectives )
{
    char *parse;

    if( (NULL == (parse = Token.next)) || ('\0' == *parse) )
        return( false );

    while( ('\0' != *parse) && IS_WHITESPACE( parse ) )
        parse++;

    if( '\0' == *parse )
        return( false );

    if( ',' == *parse )
        return( false );

    /*
    //  skip cr-lf
    */
    if( ('\n' == *parse) || ('\r' == *parse) )
        parse++;
    if( ('\n' == *parse) || ('\r' == *parse) )
        parse++;

    /*
    //  always skip to the next token if the next available token is not a comma
    //  this will allow individual tokens without commas which isn't a big deal
    */
    if( ('\0' != *parse) && (',' != *parse) ) {
        /*
        //  If the next token is __not__ a comma then we need to check that it is not a directive
        //  before allowing the skip!
        */
        if( CheckDirectives ) {
            size_t      len = 0;
            char        *t;

            for( t = parse; !IS_WHITESPACE(t); ++t ) {
                len++;
            }

            if( MatchOne( Directives, SEP_NO, parse, len ) ) {
                return( false );
            }
        }
        return( true );
    }
    return( false );
}

static bool GetNovImport( void )
/******************************/
{
    symbol      *sym;
    const char  *name = NULL;
    const char  *prefix = NULL;
    size_t      namelen = 0;
    size_t      prefixlen = 0;
    bool        result;

    /*
    //  we need to trap import/export prefixes here. Unfortunately the prefix context
    //  is not followed by a valid seperator so the GetToken() call in ProcArgList
    //  at the end of the do...while loop terminates the loop after we return from
    //  this call (and WildCard from where we were called of course
    */
    if( IsNetWarePrefix( Token.this, Token.len ) ) {
        result = SetCurrentPrefix( Token.this, Token.len );
        if( result ) {
            Token.skipToNext = DoWeNeedToSkipASeparator( false );
#ifndef NDEBUG
            printf( "Set new prefix. Skip = %d\n", Token.skipToNext );
#endif
        }
        return( result );
    }

    if( !NetWareSplitSymbol( Token.this, Token.len, &name, &namelen, &prefix, &prefixlen ) ) {
        return( false );
    }

    sym = SymOpNWPfx( ST_DEFINE_SYM, name, namelen, prefix, prefixlen );
    if( sym == NULL || sym->p.import != NULL ) {
        return( true );
    }

#ifndef NDEBUG
    printf( "imported %s from %s\n", sym->name.u.ptr, ( sym->prefix != NULL ) ? sym->prefix : "(NONE)" );
#endif

    SET_SYM_TYPE( sym, SYM_IMPORTED );
    sym->info |= SYM_DCE_REF;   // make sure we don't try to get rid of these.
    SetNovImportSymbol( sym );

    Token.skipToNext = DoWeNeedToSkipASeparator( true );

    return( true );
}

void SetNovImportSymbol( symbol *sym )
/************************************/
{
    sym->p.import = DUMMY_IMPORT_PTR;
}

static bool GetNovExport( void )
/******************************/
{
    symbol      *sym;
    const char  *name = NULL;
    const char  *prefix = NULL;
    size_t      namelen = 0;
    size_t      prefixlen = 0;
    bool        result;

    /*
    //  we need to trap import/export prefixes here. Unfortunately the prefix context
    //  is not followed by a valid seperator so the GetToken() call in ProcArgList
    //  at the end of the do...while loop terminates the loop after we return from
    //  this call (and WildCard from where we were called of course
    */
    if( IsNetWarePrefix( Token.this, Token.len ) ) {
        result = SetCurrentPrefix( Token.this, Token.len );
        if( result )
            Token.skipToNext = DoWeNeedToSkipASeparator( false );
        return( result );
    }

    if( !NetWareSplitSymbol( Token.this, Token.len, &name, &namelen, &prefix, &prefixlen ) ) {
        return( false );
    }

    sym = SymOpNWPfx( ST_CREATE | ST_REFERENCE, name, namelen, prefix, prefixlen );

    sym->info |= SYM_DCE_REF | SYM_EXPORTED;

    AddNameTable( name, namelen, true, &FmtData.u.nov.exp.export );

    Token.skipToNext = DoWeNeedToSkipASeparator( true );

    return( true );
}

bool ProcNovImport( void )
/************************/
{
    SetCurrentPrefix( NULL, 0 );
    return( ProcArgListEx( GetNovImport, TOK_INCLUDE_DOT, CmdFile ) );
}

bool ProcNovExport( void )
/************************/
{
    SetCurrentPrefix( NULL, 0 );
    return( ProcArgListEx( GetNovExport, TOK_INCLUDE_DOT, CmdFile ) );
}

bool ProcScreenName( void )
/*************************/
{
    if( !GetToken( SEP_NO, TOK_INCLUDE_DOT ) ) {
        return( false );
    }
    if( Token.len > MAX_SCREEN_NAME_LENGTH ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_TOO_LARGE, "s", "screenname" );
    } else {
        if( FmtData.u.nov.screenname != NULL ) {
            _LnkFree( FmtData.u.nov.screenname );  // assume second is correct.
        }
        FmtData.u.nov.screenname = tostring();
    }
    return( true );
}

bool ProcCheck( void )
/********************/
{
    if( !GetToken( SEP_EQUALS, TOK_INCLUDE_DOT ) ) {
        return( false );
    }
    FmtData.u.nov.checkfn = tostring();
    return( true );
}

bool ProcMultiLoad( void )
/************************/
{
    FmtData.u.nov.exeflags |= NOV_MULTIPLE;
    return( true );
}

bool ProcAutoUnload( void )
/*************************/
{
    FmtData.u.nov.exeflags |= NOV_AUTOUNLOAD;
    return( true );
}


bool ProcReentrant( void )
/************************/
{
    FmtData.u.nov.exeflags |= NOV_REENTRANT;
    return( true );
}

bool ProcSynch( void )
/********************/
{
    FmtData.u.nov.exeflags |= NOV_SYNCHRONIZE;
    return( true );
}

bool ProcPseudoPreemption( void )
/*******************************/
{
    FmtData.u.nov.exeflags |= NOV_PSEUDOPREEMPTION;
    return( true );
}

bool ProcNLMFlags( void )
/***********************/
{
    unsigned_32 value;

    if( !GetLong( &value ) )
        return( false );
    FmtData.u.nov.exeflags |= value;
    return( true );
}

bool ProcCustom( void )
/*********************/
{
    if( !GetToken( SEP_EQUALS, TOK_INCLUDE_DOT | TOK_IS_FILENAME ) ) {
        return( false );
    }
    FmtData.u.nov.customdata = tostring();         // no default extension.
    return( true );
}

bool ProcMessages( void )
/***********************/
{
    if( !GetToken( SEP_EQUALS, TOK_INCLUDE_DOT | TOK_IS_FILENAME ) ) {
        return( false );
    }
    FmtData.u.nov.messages = tostring();           // no default extension.
    return( true );
}

bool ProcHelp( void )
/*******************/
{
    if( !GetToken( SEP_EQUALS, TOK_INCLUDE_DOT | TOK_IS_FILENAME ) ) {
        return( false );
    }
    FmtData.u.nov.help = tostring();       // no default extension.
    return( true );
}

bool ProcXDCData( void )
/**********************/
{
    if( !GetToken( SEP_EQUALS, TOK_INCLUDE_DOT | TOK_IS_FILENAME ) ) {
        return( false );
    }
    FmtData.u.nov.rpcdata = tostring();    // no default extension.
    return( true );
}

bool ProcSharelib( void )
/***********************/
{
    if( !GetToken( SEP_EQUALS, TOK_INCLUDE_DOT | TOK_IS_FILENAME ) ) {
        return( false );
    }
    FmtData.u.nov.sharednlm = FileName( Token.this, Token.len, E_NLM, false );
    return( true );
}

bool ProcExit( void )
/*******************/
{
    if( !GetToken( SEP_EQUALS, TOK_INCLUDE_DOT ) ) {
        return( false );
    }
    FmtData.u.nov.exitfn = tostring();
    return( true );
}

bool ProcThreadName( void )
/*************************/
{
    if( !GetToken( SEP_NO, TOK_INCLUDE_DOT ) ) {
        return( false );
    } else if( Token.len > MAX_THREAD_NAME_LENGTH ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_TOO_LARGE, "s", "threadname" );
    } else {
        FmtData.u.nov.threadname = tostring();
    }
    return( true );
}

#define COPYRIGHT_START     "Copyright 1991"
#define DEFAULT_COPYRIGHT COPYRIGHT_START " Novell, Inc. All rights reserved"
#define DEFAULT_COPYRIGHT_LENGTH (sizeof( DEFAULT_COPYRIGHT ) - 1)
#define YEAR_OFFSET (sizeof( COPYRIGHT_START ) - 1)

bool ProcCopyright( void )
/************************/
{
    struct tm       *currtime;
    time_t          thetime;
    unsigned        year;
    char            *copy_year;

    if( !GetToken( SEP_EQUALS, TOK_INCLUDE_DOT ) && !GetToken( SEP_NO, TOK_INCLUDE_DOT ) ) {
        if( FmtData.u.nov.copyright != NULL ) {
            _LnkFree( FmtData.u.nov.copyright );  // assume second is correct.
        }
        _ChkAlloc( FmtData.u.nov.copyright, DEFAULT_COPYRIGHT_LENGTH + 1 );
        memcpy(FmtData.u.nov.copyright,DEFAULT_COPYRIGHT,DEFAULT_COPYRIGHT_LENGTH+1);
        copy_year = FmtData.u.nov.copyright + YEAR_OFFSET;
        thetime = time( NULL );
        currtime = localtime( &thetime );
        for( year = currtime->tm_year + 1900; year > 0; year /= 10 ) {
            *copy_year = '0' + (year % 10);
            copy_year--;
        }
    } else {
        if( Token.len > MAX_COPYRIGHT_LENGTH ) {
            LnkMsg( LOC+LINE+WRN+MSG_VALUE_TOO_LARGE, "s", "copyright" );
        } else {
            if( FmtData.u.nov.copyright != NULL ) {
                _LnkFree( FmtData.u.nov.copyright );  // assume second is correct.
            }
            FmtData.u.nov.copyright = tostring();
        }
    }
    return( true );
}

static bool ProcModuleTypeN( int n )
/**********************************/
{
    Extension = E_NLM;
    FmtData.u.nov.moduletype = n;
    return( true );
}

bool ProcNovell( void )
/*********************/
{
    if( !ProcOne( NovModels, SEP_NO, false ) ) {    // get file type
        int     nType = 0;

        if( (nType = atoi( Token.this )) > 0 ) {
            GetToken( SEP_NO, TOK_INCLUDE_DOT );
            ProcModuleTypeN( nType );
        } else {
            ProcNLM();
        }
    }
    if( GetToken( SEP_QUOTE, TOK_INCLUDE_DOT ) ) {  // get description
        FmtData.description = tostring();
    }
    return( true );
}

void SetNovFmt( void )
/********************/
{
    Extension = E_NLM;
}

void FreeNovFmt( void )
/*********************/
{
    _LnkFree( FmtData.description );
    _LnkFree( FmtData.u.nov.screenname );
    _LnkFree( FmtData.u.nov.checkfn );
    _LnkFree( FmtData.u.nov.exitfn );
    _LnkFree( FmtData.u.nov.customdata );
    _LnkFree( FmtData.u.nov.threadname );
    _LnkFree( FmtData.u.nov.copyright );
    _LnkFree( FmtData.u.nov.messages );
    _LnkFree( FmtData.u.nov.help );
    _LnkFree( FmtData.u.nov.rpcdata );
    _LnkFree( FmtData.u.nov.sharednlm );
/*  FreeList( FmtData.u.nov.exp.export );
    FreeList( FmtData.u.nov.exp.module );  Permalloc'd now */
}

bool ProcNLM( void )
/******************/
{
    Extension = E_NLM;
    FmtData.u.nov.moduletype = 0;
    return( true );
}

bool ProcLAN( void )
/******************/
{
    Extension = E_LAN;
    FmtData.u.nov.moduletype = 1;
    return( true );
}

bool ProcDSK( void )
/******************/
{
    Extension = E_DSK;
    FmtData.u.nov.moduletype = 2;
    return( true );
}

bool ProcNAM( void )
/******************/
{
    Extension = E_NAM;
    FmtData.u.nov.moduletype = 3;
    return( true );
}

bool ProcModuleType4( void )
/**************************/
{
    Extension = E_NLM;
    FmtData.u.nov.moduletype = 4;
    return( true );
}

bool ProcModuleType5( void )
/**************************/
{
    Extension = E_NOV_MSL;
    FmtData.u.nov.moduletype = 5;
    return( true );
}

bool ProcModuleType6( void )
/**************************/
{
    Extension = E_NLM;
    FmtData.u.nov.moduletype = 6;
    return( true );
}

bool ProcModuleType7( void )
/**************************/
{
    Extension = E_NLM;
    FmtData.u.nov.moduletype = 7;
    return( true );
}

bool ProcModuleType8( void )
/**************************/
{
    Extension = E_NOV_HAM;
    FmtData.u.nov.moduletype = 8;
    return( true );
}

bool ProcModuleType9( void )
/**************************/
{
    Extension = E_NOV_CDM;
    FmtData.u.nov.moduletype = 9;
    return( true );
}

#if 0
/*
// as I have got tired of writing, module types 10 through 12 are reserved */
bool ProcModuleType10( void )
/***************************/
{
    Extension = ;
    FmtData.u.nov.moduletype = 10;
    return( true );
}

bool ProcModuleType11( void )
/***************************/
{
    Extension = ;
    FmtData.u.nov.moduletype = 11;
    return( true );
}

bool ProcModuleType12( void )
/***************************/
{
    Extension = ;
    FmtData.u.nov.moduletype = 12;
    return( true );
}
#endif

static bool GetNovModule( void )
/******************************/
{
    AddNameTable( Token.this, Token.len, false, &FmtData.u.nov.exp.module );
    return( true );
}

bool ProcModule( void )
/*********************/
{
    return( ProcArgList( GetNovModule, TOK_INCLUDE_DOT ) );
}

bool ProcOSDomain( void )
/***********************/
{
    FmtData.u.nov.exeflags |= NOV_OS_DOMAIN;
    return( true );
}

bool ProcNovDBIExports( void )
/****************************/
{
    FmtData.u.nov.flags |= DO_NOV_EXPORTS;
    return( true );
}

bool ProcNovDBIReferenced( void )
/*******************************/
{
    FmtData.u.nov.flags |= DO_NOV_REF_ONLY;
    return( true );
}

bool ProcNovDBI( void )
/*********************/
{
    LinkFlags |= NOVELL_DBI_FLAG;
    if( ProcOne( NovDBIOptions, SEP_NO, false ) ) {
        while( ProcOne( NovDBIOptions, SEP_COMMA, false ) ) {
            ; /*null loop*/
        }
    }
    return( true );
}

bool ProcExportsDBI( void )
/*************************/
{
    DBIFlag |= DBI_ONLY_EXPORTS;
    FmtData.u.nov.flags |= DO_WATCOM_EXPORTS;
    return( true );
}

void CmdNovFini( void )
/*********************/
{
}
