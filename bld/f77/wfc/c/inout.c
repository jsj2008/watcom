/***************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2017 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  Input/output for listing, errors and source files.
*
****************************************************************************/


#include "ftnstd.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "progsw.h"
#include "cpopt.h"
#include "errcod.h"
#include "global.h"
#include "omodes.h"
#include "cioconst.h"
#include "csetinfo.h"
#include "fmemmgr.h"
#include "ferror.h"
#include "comio.h"
#include "inout.h"
#include "banner.h"
#include "charset.h"
#include "mkname.h"
#include "filescan.h"
#include "sdcio.h"
#include "libsupp.h"
#include "wf77auxd.h"
#include "wf77aux.h"
#include "errutil.h"
#include "sdfile.h"
#include "brseinfo.h"
#include "posdat.h"

#include "clibext.h"


#if _CPU == 8086
    #define _Banner "FORTRAN 77 x86 16-bit Optimizing Compiler"
#elif _CPU == 386
    #define _Banner "FORTRAN 77 x86 32-bit Optimizing Compiler"
#elif _CPU == _AXP
    #define _Banner "FORTRAN 77 Alpha AXP Optimizing Compiler"
#elif _CPU == _PPC
    #define _Banner "FORTRAN 77 PowerPC Optimizing Compiler"
#else
    #error Unknown System
#endif

static f_attrs          DskAttr = { REC_TEXT | CARRIAGE_CONTROL };
static f_attrs          PrtAttr = { REC_TEXT | CARRIAGE_CONTROL };
static f_attrs          TrmAttr = { REC_TEXT | CARRIAGE_CONTROL };
static f_attrs          ErrAttr = { REC_TEXT };

static char             ErrExtn[] = { "err" };
static char             LstExtn[] = { "lst" };

static char             *ListBuff;      // listing file buffer
static file_handle      ListFile;       // file pointer for the listing file
static int              ListCursor;     // offset into "ListBuff"

static byte             ListCount;      // # of lines printed to listing file
static byte             ListFlag;       // flag for listing file

static char             *ErrBuff;       // error file buffer
static file_handle      ErrFile;        // file pointer for the error file
static int              ErrCursor;      // offset into "ErrBuff"

static char             *TermBuff;      // terminal file buffer
static file_handle      TermFile;       // file pointer for terminal
static int              TermCursor;     // offset into "TermBuff"

/* Forward declarations */
static  void    SendRec( void );
static  void    SetCtrlSeq( void );
static  void    PutLst( const char *string );
static  void    ChkErrErr( void );
static  void    ErrOut( const char *string );
static  void    ErrNL( void );
static  void    ChkLstErr( void );
static  void    Erase( char *extn );


//========================================================================
//
//  Initialization routines
//
//========================================================================


void    InitComIO( void ) {
//===================

    TermCursor = 0;
    ErrCursor  = 0;
    ListCursor = 0;
    // Point "terminal" buffer and ".ERR" file buffer to static area
    // so that we can report an error before memory initialization.
    TermBuff = TokenBuff;
    ErrBuff = &TokenBuff[ 256 ];
    ListBuff = NULL;
    CurrFile = NULL;
    ErrFile = NULL;
    ListFile = NULL;
    ListFlag = 0;
    ListCursor = 0;
    ListCount = 0;
    SDInitIO();
    TermFile = FStdOut;
}


void    InitMemIO( void ) {
//===================

    // We've initialized memory - now we can allocate file buffers.
    TermBuff = FMemAlloc( TERM_BUFF_SIZE );
    if( ErrFile == NULL ) {
        // we haven't opened the error file yet so set ErrBuff to NULL
        // so that when we do open the error file we can allocate the
        // buffer at that time
        ErrBuff = NULL;
    } else {
        // the error file has been opened so allocate a buffer for it
        ErrBuff = FMemAlloc( ERR_BUFF_SIZE );
    }
}


void    FiniComIO( void ) {
//===================

    if( TermBuff != TokenBuff ) {
        FMemFree( TermBuff );
    }
}


//========================================================================
//
//  Source file routines
//
//========================================================================


void    OpenSrc( void ) {
//=======================

    file_handle fp;
    char        err_msg[ERR_BUFF_SIZE+1];
    char        bld_name[_MAX_PATH];
    bool        erase_err;

    erase_err = ErrFile == NULL;
    SDInitAttr();
    MakeName( SrcName, SrcExtn, bld_name );
    fp = SDOpen( bld_name, READ_FILE );
    if( fp != NULL ) {
        SrcInclude( bld_name );
        CurrFile->fileptr = fp;
    } else {
        SDError( NULL, err_msg );
        InfoError( SM_OPENING_FILE, bld_name, err_msg );
    }
    if( erase_err ) {
        CloseErr();
        Erase( ErrExtn );
    }
}


void    IOPurge( void ) {
//=======================

// make sure all the input files are closed

    while( CurrFile != NULL ) {
        Conclude();
    }
}


static  uint    SrcRead( void ) {
//===============================

    uint        len;
    file_handle fp;
    char        msg[81];

    fp = CurrFile->fileptr;
    if( CurrFile->flags & INC_LIB_MEMBER ) {
        len = LibRead( fp );
        if( LibEof( fp ) ) {
            ProgSw |= PS_INC_EOF;
        } else if( LibError( fp, msg ) ) {
            InfoError( SM_IO_READ_ERR, CurrFile->name, msg );
            ProgSw |= PS_INC_EOF;
        }
    } else {
        len = SDRead( fp, SrcBuff, SRCLEN );
        if( SDEof( fp ) ) {
            ProgSw |= PS_INC_EOF;
        } else if( SDError( fp, msg ) ) {
            InfoError( SM_IO_READ_ERR, CurrFile->name, msg );
            ProgSw |= PS_INC_EOF;
        }
    }
    return( len );
}


void    ReadSrc( void ) {
//=======================

    uint        len;

    // If we are loading source as a result of an undefined
    // subprogram (as opposed to using an C$INCLUDE option),
    // then indicate EOF since the main source file may have
    // the C$DATA option in it in which case "CurrFile" will
    // not be NULL after calling "Conclude()".
    if( CurrFile->flags & INC_DATA_OPTION ) {
        ProgSw |= PS_SOURCE_EOF;
    } else {
        len = SrcRead();
        if( ProgSw & PS_INC_EOF ) {
            CurrFile->flags |= CONC_PENDING;
            if( CurrFile->link == NULL ) {
                ProgSw |= PS_SOURCE_EOF;
            }
        } else {
            CurrFile->rec++;
            SrcBuff[ len ] = NULLCHAR;
        }
    }
}


static bool AlreadyOpen( const char *name )
//=========================================
{
    source_t    *src;

    src = CurrFile;
    for( ;; ) {
        if( src == NULL )
            return( false );
        if( strcmp( name, src->name ) == 0 )
            break;
        src = src->link;
    }
    InfoError( CO_ALREADY_OPEN, name );
    return( true );
}


void    Include( const char *inc_name )
//=====================================
{
    file_handle fp;
    char        bld_name[_MAX_PATH];
    char        err_msg[ERR_BUFF_SIZE+1];

    SDInitAttr();
    CopyMaxStr( inc_name, bld_name, _MAX_PATH - 1 );
    MakeName( bld_name, SDSrcExtn( bld_name ), bld_name );
    if( AlreadyOpen( inc_name ) )
        return;
    if( AlreadyOpen( bld_name ) )
        return;
    // try file called <include_name>.FOR.
    fp = SDOpen( bld_name, READ_FILE );
    if( fp != NULL ) {
        SrcInclude( bld_name );
        CurrFile->fileptr = fp;
    } else {
        // get error message before next i/o
        SDError( NULL, err_msg );
        // try library
        fp = IncSearch( inc_name );
        if( fp != NULL ) {
            // SrcInclude( inc_name ) now done in LIBSUPP
            CurrFile->fileptr = fp;
            CurrFile->flags |= INC_LIB_MEMBER;
        } else {
            // could not open include file
            InfoError( SM_OPENING_FILE, bld_name, err_msg );
        }
    }
    // clear RetCode so that we don't get "file not found" returned
    // because we could not open include file
    RetCode = _SUCCESSFUL;
    AddDependencyInfo( CurrFile );
}


bool    SetLst( bool new ) {
//==========================

    bool        old;

    old = ( ListFlag & LF_QUIET ) == 0;
    if( new ) {
        ListFlag &= ~LF_QUIET;
    } else {
        ListFlag |= LF_QUIET;
    }
    return( old );
}


void    SrcInclude( const char *name )
//====================================
{
    source_t    *src;

    src = FMemAlloc( sizeof( source_t ) );
    src->name = FMemAlloc( strlen( name ) + 1 );
    strcpy( src->name, name );
    src->rec = 0;
    src->link = CurrFile;
    src->options = NewOptions;
    src->flags = 0;
    if( CurrFile != NULL ) {
        NewOptions = Options;
        if( ( Options & OPT_INCLIST ) == 0 ) {
            SetLst( false );
        }
    }
    CurrFile = src;
    if( CurrFile->link ) {
        // tell the browser which file we are going into (not for the main
        // source file since we have not yet initialized the dwarf library)
        BISetSrcFile();
    }
}


void    Conclude( void ) {
//========================

    source_t    *old;

    old = CurrFile;
    CurrFile = CurrFile->link;
    if( CurrFile != NULL ) {
        NewOptions = old->options;
        Options = NewOptions;
        if( ( ( CurrFile->link == NULL ) && ( Options & OPT_LIST ) ) ||
              ( Options & OPT_INCLIST ) ) {
            SetLst( true );
        } else {
            SetLst( false );
        }
    }
    if( old->flags & INC_LIB_MEMBER ) {
        IncMemClose( old->fileptr );
    } else {
        SDClose( old->fileptr );
    }
    FMemFree( old->name );
    FMemFree( old );
    ProgSw &= ~PS_INC_EOF;
    BISetSrcFile();             // tell browser which file we return to
}


//========================================================================
//
//  Error file routines
//
//========================================================================


static  file_handle Open( char *fn, char *extn, int mode ) {
//==========================================================

    file_handle ptr;
    char        buffer[_MAX_PATH];
    char        errmsg[81];

    MakeName( fn, extn, buffer );
    ptr = SDOpen( buffer, mode );
    if( SDError( ptr, errmsg ) ) {
        InfoError( SM_OPENING_FILE, &buffer, &errmsg );
    }
    return( ptr );
}


void    OpenErr( void ) {
//=======================

    if( ( Options & OPT_ERRFILE ) &&
        ( ( ProgSw & PS_ERR_OPEN_TRIED ) == 0 ) ) {
        ProgSw |= PS_ERR_OPEN_TRIED;
        SDSetAttr( ErrAttr );
        ErrFile = Open( SDFName( SrcName ), ErrExtn, WRITE_FILE );
        if( ErrFile != NULL ) {
            SDInitAttr();
            ErrCursor = 0;
            // ErrBuff will be non-NULL iff we have opened the error file
            // before initializing memory (i.e. a warning message during
            // options processing).
            if( ErrBuff == NULL ) {
                ErrBuff = FMemAlloc( ERR_BUFF_SIZE );
            }
        }
    }
}


void    CompErr( uint msg ) {
//===========================

    InfoError( msg );
}


void    PrintErr( const char *string )
//====================================
{
    JustErr( string );
    PrtLst( string );
}


static  bool    ErrToTerm( void ) {
//=================================

    if( ( Options & OPT_TERM ) == 0 )
        return( false );
    if( ( Options & OPT_TYPE ) && ( ListFile != NULL ) )
        return( false );
    return( true );
}


void    PrtErrNL( void ) {
//========================

    if( ErrToTerm() ) {
        TOutNL( "" );
    }
    ErrNL();
    PrtLstNL( "" );
}


void    JustErr( const char *string )
//===================================
{
    if( ErrToTerm() ) {
        TOut( string );
    }
    ErrOut( string );
}


static  void    ErrNL( void ) {
//=============================

    if( ErrFile != NULL ) {
        SDWrite( ErrFile, ErrBuff, ErrCursor );
        ChkErrErr();
    }
    ErrCursor = 0;
}


static  void    ChkErrErr( void ) {
//=================================

    char        msg[81];
    char        fnbuff[_MAX_PATH];

    if( SDError( ErrFile, msg ) ) {
        CloseErr();
        Options |= OPT_TERM;
        TermCursor = 0;
        MakeName( SDFName( SrcName ), ErrExtn, fnbuff );
        InfoError( SM_IO_WRITE_ERR, fnbuff, msg );
    }
}

void    ChkErrFile( void ) {
//==========================

// Make sure error file is opened.

    if( ErrFile == NULL ) {
        OpenErr();
    }
}


static  void    SendBuff( const char *str, char *buff, int buff_size, int *cursor,
                          file_handle fp, void (*err_rtn)( void ) ) {
//==========================================================================

    int         len;

    for( ; fp != NULL; ) {
        len = buff_size - 1 - *cursor;
        len = CharSetInfo.extract_text( str, len );
        len = CopyMaxStr( str, &buff[*cursor], len );
        *cursor += len;
        str += len;
        if( *str == NULLCHAR )
            break;
        SDWrite( fp, buff, *cursor );
        err_rtn();
        *cursor = 0;
    }
}


static  void    ErrOut( const char *string )
//==========================================
{
    if( ErrFile != NULL ) {
        SendBuff( string, ErrBuff, ERR_BUFF_SIZE, &ErrCursor, ErrFile, &ChkErrErr );
    }
}


void    CloseErr( void ) {
//========================

    if( ErrFile == NULL )
        return;
    SDClose( ErrFile );
    ErrFile = NULL;
    if( ErrBuff == NULL )
        return;
    FMemFree( ErrBuff );
    ErrBuff = NULL;
}


//========================================================================
//
//  Terminal output routines
//
//========================================================================


static  void    ChkTermErr( void ) {
//==================================
}


void    TOutNL( const char *string )
//==================================
{
    TOut( string );
    SDWrite( TermFile, TermBuff, TermCursor );
    TermCursor = 0;
}


void    TOut( const char *string )
//================================
{
    SendBuff( string, TermBuff, TERM_BUFF_SIZE, &TermCursor, TermFile, &ChkTermErr );
}


//========================================================================
//
//  Listing file routines
//
//========================================================================


static  void    OpenListingFile( bool reopen ) {
//==============================================

    char        errmsg[81];
    char        name[_MAX_PATH];

    /* unused parameters */ (void)reopen;

    if( ( Options & OPT_LIST ) == 0 ) {
        // no listing file
        // ignore other listing file options
    } else {
        GetLstName( name );
        if( Options & OPT_TYPE ) {
            SDSetAttr( TrmAttr );
        // On the VAX, /PRINT means to generate a disk file "xxx.LIS"
        // and set the spooling bit
        } else if( Options & OPT_PRINT ) {
            SDSetAttr( PrtAttr );
        } else { // DISK file
            SDSetAttr( DskAttr );
        }
        ListFile = SDOpen( name, WRITE_FILE );
        if( SDError( ListFile, errmsg ) ) {
            InfoError( SM_OPENING_FILE, name, errmsg );
        } else {
            ListBuff = FMemAlloc( LIST_BUFF_SIZE + 1 );
            if( ListBuff == NULL ) {
                CloseLst();
                InfoError( MO_DYNAMIC_OUT );
            }
        }
        SDInitAttr();
    }
}


void    OpenLst( void ) {
//=======================

    OpenListingFile( false );
}


void    ReOpenLst( void ) {
//=========================

    OpenListingFile( true );
}


void    ChkPntLst( void ) {
//=========================

    if( ListFlag & LF_QUIET ) {
        ListFlag &= ~LF_STMT_LISTED;
    } else {
        ListFlag |= LF_STMT_LISTED;
    }
}


bool    WasStmtListed( void ) {
//=============================

    return( ( ListFlag & LF_STMT_LISTED ) != 0 );
}


void    TOutBanner( void ) {
//==========================

#if defined( _BETAVER )
    TOutNL( banner1w1( _Banner ) );
    TOutNL( banner1w2( _WFC_VERSION_ ) );
#else
    TOutNL( banner1w( _Banner, _WFC_VERSION_ ) );
#endif
    TOutNL( banner2 );
    TOutNL( banner2a( 1984 ) );
    TOutNL( banner3 );
    TOutNL( banner3a );
}

#define MAX_TIME_STR    (4+1+2+1+2+1+2+1+2+1+2)

void    PrtBanner( void ) {
//=========================

    char        banner[LIST_BUFF_SIZE + 1];
    time_t      time_of_day;
    struct tm   *t;

#if defined( _BETAVER )
    strcpy( banner, banner1w1( _Banner ) );
    PrtLstNL( banner );
    strcpy( banner, banner1w2( _WFC_VERSION_ ) );
#else
    strcpy( banner, banner1w( _Banner, _WFC_VERSION_ ) );
#endif
    time_of_day = time( NULL );
    t = localtime( &time_of_day );
    MsgFormat( "  %4d1/%2d2/%2d3 %2d4:%2d5:%2d6", banner + strlen( banner ),
               1900 + t->tm_year, t->tm_mon + 1, t->tm_mday,
               t->tm_hour, t->tm_min, t->tm_sec );
    PrtLstNL( banner );
    PrtLstNL( banner2 );
    PrtLstNL( banner2a( 1984 ) );
    PrtLstNL( banner3 );
    PrtLstNL( banner3a );
}


void    GetLstName( char *buffer ) {
//==================================

    if( Options & OPT_TYPE ) {
        strcpy( buffer, SDTermOut );
#if !defined( __UNIX__ )
    // On the VAX, /PRINT means to generate a disk file "xxx.LIS"
    //             and set the spooling bit
    // On QNX, there is no /PRINT option
    } else if( Options & OPT_PRINT ) {
        strcpy( buffer, SDPrtName );
#endif
    } else {
        MakeName( SDFName( SrcName ), LstExtn, buffer );
    }
}


void    PrtLstNL( const char *string )
//====================================
{
    ListFlag |= LF_NEW_LINE;
    PrtLst( string );
    ListFlag &= LF_OFF;
}


void    PrtLst( const char *string )
//==================================
{
    if( ListFlag & LF_QUIET )
        return;
    if( ListFile == NULL )
        return;
    PutLst( string );
}


void    CloseLst( void ) {
//========================

    if( ListFile == NULL )
        return;
    SDClose( ListFile );
    ListFile = NULL;
    if( ListBuff == NULL )
        return;
    FMemFree( ListBuff );
    ListBuff = NULL;
}


void    LFEndSrc( void ) {
//==================

    if( ListFile == NULL ) return;
    ListFlag &= LF_OFF;
    ListCursor = 0;
    SetCtrlSeq();
    SendRec();
}


void    LFNewPage( void ) {
//=========================

    ListFlag |= LF_PAGE_FLAG;
}


void    LFSkip( void ) {
//======================

    ListFlag |= LF_SKIP_FLAG;
    if( ( ListFlag & LF_QUIET ) == 0 ) {
        ++ListCount;
    }
}


static  void    PutLst( const char *string )
//==========================================
{
    int         len;
    bool        newline;

    newline = ( ListFlag & LF_NEW_LINE );
    for( ; ListFile != NULL; ) {
        if( ListCursor == 0 ) {
            SetCtrlSeq();
        }
        len = LIST_BUFF_SIZE - ListCursor - 1; // -1 for NULLCHAR
        len = CharSetInfo.extract_text( string, len );
        len = CopyMaxStr( string, &ListBuff[ ListCursor ], len );
        ListCursor += len;
        string += len;
        if( *string == NULLCHAR )
            break;
        SendRec();
    }
    if( newline ) {
        SendRec();
    }
}


static  void    SetCtrlSeq( void ) {
//==================================

    char        *ctrlseq;

    ++ListCount;
    if( ListCount >= LinesPerPage - LF_PAGE_BOUNDARY ) {
        ListFlag |= LF_PAGE_FLAG;
    }
    if(  ListFlag & LF_PAGE_FLAG ) {
        ListCount = 0;
        if( Options & OPT_TYPE ) {
            ctrlseq = SkipCtrlSeq;
        } else {
            ctrlseq = FFCtrlSeq;
        }
    } else if( ListFlag & LF_SKIP_FLAG ) {
        ctrlseq = SkipCtrlSeq;
    } else {
        ctrlseq = NormalCtrlSeq;
    }
    ListCursor = CopyMaxStr( ctrlseq, ListBuff, LIST_BUFF_SIZE );
}


static  void    SendRec( void ) {
//===============================

    if( ListFile != NULL ) {
        SDWrite( ListFile, ListBuff, ListCursor );
        ChkLstErr();
    }
    ListFlag &= LF_OFF;
    ListCursor = 0;
}


static  void    ChkLstErr( void ) {
//=================================

    char        msg[81];
    char        fnbuff[_MAX_PATH];

    if( SDError( ListFile, msg ) ) {
        CloseLst();
        Options |= OPT_TERM;
        TermCursor = 0;
        GetLstName( fnbuff );
        InfoError( SM_IO_WRITE_ERR, fnbuff, msg );
    }
}


static  void    Erase( char *extn ) {
//===================================

    char        buffer[_MAX_PATH];

    MakeName( SDFName( SrcName ), extn, buffer );
    SDScratch( buffer );
}

