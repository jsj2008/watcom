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
* Description:  Processing of linker options for OS/2 and Windows formats.
*
****************************************************************************/


#include <string.h>
#include <ctype.h>
#include "linkstd.h"
#include "alloc.h"
#include "walloca.h"
#include "command.h"
#include "cmdos2.h"
#include "exeos2.h"
#include "exepe.h"
#include "loados2.h"
#include "loadpe.h"
#include "msg.h"
#include "wlnkmsg.h"
#include "objfree.h"
#include "cmdline.h"
#include "fileio.h"
#include "impexp.h"
#include "objpass1.h"

static void             ParseVersion( void );
static bool             GetWlibImports( void );
static bool             getimport( void );
static bool             getexport( void );
static bool             getsegflags( void );

bool ProcOS2Import( void )
/*******************************/
{
    return( ProcArgList( &getimport, TOK_NORMAL ) );
}

bool ProcOS2Export( void )
/*******************************/
{
    bool    retval;

    if( GetToken( SEP_EQUALS, TOK_INCLUDE_DOT ) ) {
        retval = GetWlibImports();
    } else {
        retval =  ProcArgList( &getexport, TOK_NORMAL );
    }
    return( retval );
}

bool ProcAnonExport( void )
/********************************/
{
    bool    retval;

    CmdFlags |= CF_ANON_EXPORT;
    retval = ProcOS2Export();
    CmdFlags &= ~CF_ANON_EXPORT;
    return( retval );
}

bool ProcOS2Segment( void )
/********************************/
{
    return( ProcArgList( &getsegflags, TOK_INCLUDE_DOT ) );
}

static entry_export *ProcWlibDLLImportEntry( void )
{
    unsigned_16     ordinal;
    entry_export    *exp;
    length_name     symname;
    length_name     internal;

    DUPSTR_STACK( symname.name, Token.this, Token.len );
    symname.len = Token.len;
    if( !GetToken( SEP_DOT_EXT, TOK_NORMAL ) ) {
        return( NULL );
    }
    internal.len = 0;
    internal.name = NULL;
    ordinal = 0;
    if( GetToken( SEP_DOT_EXT, TOK_NORMAL ) ) {
        if( getatoi( &ordinal ) != ST_IS_ORDINAL ) {
            if( Token.len > 0 ) {
                internal = symname;
                DUPSTR_STACK( symname.name, Token.this, Token.len );
                symname.len = Token.len;
            }
            if( GetToken( SEP_DOT_EXT, TOK_NORMAL ) && getatoi( &ordinal ) != ST_IS_ORDINAL ) {
                if( GetToken( SEP_DOT_EXT, TOK_NORMAL ) ) {
                    getatoi( &ordinal );
                }
            }
        }
    }
    exp = AllocExport( symname.name, symname.len );
    exp->isanonymous = ( (CmdFlags & CF_ANON_EXPORT) != 0 );
    if( internal.name != NULL ) {
        exp->sym = SymOp( ST_CREATE | ST_REFERENCE, internal.name, internal.len );
    } else {
        exp->sym = SymOp( ST_CREATE | ST_REFERENCE, symname.name, symname.len );
    }
    exp->sym->info |= SYM_DCE_REF;      // make sure it isn't removed
    exp->ordinal = ordinal;
    if( ordinal == 0 ) {
        exp->isresident = true;   // no ord spec'd so must be resident
    }
    return( exp );
}

static bool GetWlibImports( void )
/********************************/
/* read in a wlib command file, get the import directives, and treat them
 * as exports (hey man, GO asked for it ...... ) */
{
    char            *fname;
    f_handle        handle;
    entry_export    *exp;

    fname = FileName( Token.this, Token.len, E_LBC, false );
    handle = QOpenR( fname );
    SetCommandFile( handle, fname );
    Token.locked = true;      /* make sure only this file parsed */
    while( GetToken( SEP_SPACE, TOK_NORMAL ) ) {
        if( Token.len <= 2 )
            continue;
        if( (Token.this[0] == '+') && (Token.this[1] == '+') ) {
            Token.this += 2;
            Token.len -= 2;
            if( Token.this[0] == '\'' ) {
                Token.thumb = true;
                if( !GetToken( SEP_QUOTE, TOK_NORMAL ) ) {
                    LnkMsg( LOC+LINE+ERR+MSG_BAD_WLIB_IMPORT, NULL );
                    RestoreCmdLine();   /* get rid of this file */
                    return( true );
                }
            }
            exp = ProcWlibDLLImportEntry();
            if( exp == NULL ) {
                LnkMsg( LOC+LINE+ERR+MSG_BAD_WLIB_IMPORT, NULL );
                RestoreCmdLine();       /* get rid of this file */
                return( true );
            }
            AddToExportList( exp );
        }
    }
    Token.locked = false;
    return( true );
}

static bool getimport( void )
/***************************/
{
    length_name         intname;
    length_name         modname;
    length_name         extname;
    unsigned_16         ordinal;
    ord_state           state;

    DUPSTR_STACK( intname.name, Token.this, Token.len );
    intname.len = Token.len;
    if( !GetToken( SEP_NO, TOK_NORMAL ) ) {
        return( false );
    }
    DUPSTR_STACK( modname.name, Token.this, Token.len );
    modname.len = Token.len;
    ordinal = 0;
    state = ST_INVALID_ORDINAL;   // assume to extname or ordinal.
    if( GetToken( SEP_PERIOD, TOK_INCLUDE_DOT ) ) {
        state =  getatoi( &ordinal );
        if( state == ST_NOT_ORDINAL ) {
            DUPSTR_STACK( extname.name, Token.this, Token.len );
            extname.len = Token.len;
        } else if( state == ST_INVALID_ORDINAL ) {
            LnkMsg( LOC+LINE+MSG_IMPORT_ORD_INVALID + ERR, NULL );
            return( true );
        }
    }
    if( state == ST_IS_ORDINAL ) {
        HandleImport( &intname, &modname, &intname, ordinal );
    } else {
        if( state == ST_NOT_ORDINAL ) {
            HandleImport( &intname, &modname, &extname, NOT_IMP_BY_ORDINAL );
        } else {
            HandleImport( &intname, &modname, &intname, NOT_IMP_BY_ORDINAL );
        }
    }
    return( true );
}

static bool getexport( void )
/***************************/
{
    entry_export    *exp;
    unsigned_16     val16;
    unsigned_32     val32;

    exp = AllocExport( Token.this, Token.len );
    exp->isanonymous = ( (CmdFlags & CF_ANON_EXPORT) != 0 );
    if( GetToken( SEP_PERIOD, TOK_INCLUDE_DOT ) ) {
        if( getatol( &val32 ) != ST_IS_ORDINAL ) {
            LnkMsg( LOC+LINE+ERR + MSG_EXPORT_ORD_INVALID, NULL );
            _LnkFree( exp );
            GetToken( SEP_EQUALS, TOK_INCLUDE_DOT );
            return( true );
        }
        exp->ordinal = val32;
    }
    if( GetToken( SEP_EQUALS, TOK_INCLUDE_DOT ) ) {
        exp->sym = SymOp( ST_CREATE | ST_REFERENCE, Token.this, Token.len );
        if( GetToken( SEP_EQUALS, TOK_INCLUDE_DOT ) ) {
            exp->impname = tostring();
        }
    } else {
        exp->sym = RefISymbol( exp->name.u.ptr );
    }
    exp->sym->info |= SYM_DCE_REF;      //make sure it is not removed
    if( exp->ordinal == 0 ) {
        exp->isresident = true;   // no ordinal spec'd so must be kept resident
    }
    exp->next = FmtData.u.os2.exports;    // put in the front of the list for
    FmtData.u.os2.exports = exp;          // now so ProcResidant can get to it.
    while( ProcOne( Exp_Keywords, SEP_NO, false ) ) {
        // handle misc options
    }
    FmtData.u.os2.exports = exp->next;       // take it off the list
    exp->iopl_words = 0;
    if( (FmtData.type & (MK_WINDOWS|MK_PE)) == 0 && GetToken( SEP_NO, TOK_INCLUDE_DOT ) ) {
        if( getatoi( &val16 ) == ST_IS_ORDINAL ) {
            if( val16 > 63 ) {
                LnkMsg( LOC+LINE+MSG_TOO_MANY_IOPL_WORDS+ ERR, NULL );
            } else {
                exp->iopl_words = val16;
            }
        } else {
            Token.thumb = true;     // reprocess the token.
        }
    }
    AddToExportList( exp );
    return( true );
}

bool ProcExpResident( void )
/*********************************/
{
    FmtData.u.os2.exports->isresident = true;
    return( true );
}

bool ProcPrivate( void )
/******************************/
{
    FmtData.u.os2.exports->isprivate = true;
    return( true );
}

bool ProcOS2Alignment( void )
/**********************************/
/* process Alignment option */
{
    ord_state           ret;
    unsigned_32         value;

    if( !HaveEquals( TOK_NORMAL ) ) return( false );
    ret = getatol( &value );
    if( ret != ST_IS_ORDINAL || value == 0 ) {
        return( false );
    }
    FmtData.u.os2.segment_shift = blog_32( value - 1 ) + 1;     //round up.
    return( true );
}

bool ProcObjAlign( void )
/******************************/
/* process ObjAlign option */
{
    ord_state           ret;
    unsigned_32         value;

    if( !HaveEquals( TOK_NORMAL ) ) return( false );
    ret = getatol( &value );
    if( ret != ST_IS_ORDINAL || value == 0 ) {
        return( false );
    }                                            /* value not a power of 2 */
    if( value < 16 || value > (256 * 1024UL * 1024) || (value & (value - 1)) ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "objalign" );
        value = 64*1024;
    }
    FmtData.objalign = value;
    ChkBase(value);
    return( true );
}

bool ProcModName( void )
/*****************************/
{
    if( !HaveEquals( TOK_INCLUDE_DOT ) ) return( false );
    FmtData.u.os2.module_name = totext();
    return( true );
}

bool ProcNewFiles( void )
/******************************/
{
    FmtData.u.os2.flags |= LONG_FILENAMES;
    return( true );
}

bool ProcProtMode( void )
/******************************/
{
    FmtData.u.os2.flags |= PROTMODE_ONLY;
    return( true );
}

bool ProcOldLibrary( void )
/********************************/
{
    if( !HaveEquals(TOK_INCLUDE_DOT | TOK_IS_FILENAME) ) return( false );
    FmtData.u.os2.old_lib_name = FileName( Token.this, Token.len, E_DLL, false );
    return( true );
}

bool ProcOS2HeapSize( void )
/*********************************/
{
    ord_state           ret;
    unsigned_32         value;

    if( !HaveEquals( TOK_NORMAL ) ) return( false );
    ret = getatol( &value );
    if( ret != ST_IS_ORDINAL || value == 0 ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "heapsize" );
    } else {
        FmtData.u.os2.heapsize = value;
    }
    return( true );
}

bool ProcDescription( void )
/*********************************/
{
    if( !GetToken( SEP_NO, TOK_INCLUDE_DOT ) ) {
        return( false );
    }
    if( FmtData.description != NULL ) {
        _LnkFree( FmtData.description );
    }
    FmtData.description = tostring();
    return( true );
}

bool ProcCommitStack( void )
/*********************************/
{
    return( GetLong( &FmtData.u.pe.stackcommit ) );
}

bool ProcCommitHeap( void )
/********************************/
{
    return( GetLong( &FmtData.u.pe.heapcommit ) );
}

static bool AddCommit( void )
/***************************/
{
    Token.thumb = true;
    return( ProcOne( CommitKeywords, SEP_NO, false ) );
}

bool ProcCommit( void )
/****************************/
// set NT stack and heap commit sizes.
{
    return( ProcArgList( AddCommit, TOK_INCLUDE_DOT ) );
}

bool ProcRWRelocCheck( void )
/**********************************/
// check for segment relocations pointing to read/write data segments
{
    FmtData.u.os2.chk_seg_relocs = true;
    return( true );
}

bool ProcSelfRelative( void )
/**********************************/
{
    FmtData.u.os2.gen_rel_relocs = true;
    return( true );
}

bool ProcInternalRelocs( void )
/************************************/
// in case someone wants internal relocs generated.
{
    FmtData.u.os2.gen_int_relocs = true;
    return( true );
}

bool ProcToggleRelocsFlag( void )
/***************************************/
// Rational wants internal relocs generated, but wants the "no internal relocs"
// flag set
{
    FmtData.u.os2.toggle_relocs = true;
    return( true );
}

bool ProcMixed1632( void )
/***************************************/
// Sometimes it's useful to mix 16-bit and 32-bit code/data into one segment
// specially for OS/2 Device Drivers
{
    LinkFlags &= ~FAR_CALLS_FLAG ; // must be turned off for mixed code
    FmtData.u.os2.mixed1632 = true;
    return( true );
}

bool ProcPENoRelocs( void )
/*********************************/
{
    LinkState &= ~MAKE_RELOCS;
    return( true );
}

bool ProcNoStdCall( void )
/*******************************/
{
    FmtData.u.pe.no_stdcall = true;
    return( true );
}

bool ProcOS2( void )
/*************************/
// process the format os2 or format windows directives
//
{
    Extension = E_LOAD;
    while( ProcOne( SubFormats, SEP_NO, false ) ) {
        // NOTE NULL loop
    }
    if( FmtData.type & MK_WINDOWS ) {
        if( ProcOne( WindowsFormatKeywords, SEP_NO, false ) ) {
            ProcOne( WindowsFormatKeywords, SEP_NO, false );
        }
    } else if( FmtData.type & MK_WIN_VXD ) {
        ProcOne( VXDFormatKeywords, SEP_NO, false );
        FmtData.dll = true;
    } else {
        ProcOne( OS2FormatKeywords, SEP_NO, false );
        if( FmtData.type & MK_OS2_LX ) {
            if( FmtData.dll ) {
                FmtData.u.os2.gen_int_relocs = true;
            }
        }
    }
    if( FmtData.type & MK_ONLY_OS2_16 ) {       // if no 32-bit thing specd
        HintFormat( MK_ONLY_OS2_16 );   // make sure 16-bit is what we get
        if( FmtData.dll ) {
            FmtData.u.os2.flags &= ~MULTIPLE_AUTO_DATA;
            FmtData.u.os2.flags |= SINGLE_AUTO_DATA;
        }
    }
    return( true );
}

bool ProcPE( void )
/************************/
{
    ProcOne( NTFormatKeywords, SEP_NO, false );
    FmtData.u.pe.heapcommit   = PE_DEF_HEAP_COMMIT; // arbitrary non-zero default.
    FmtData.u.pe.os2.heapsize = PE_DEF_HEAP_SIZE;   // another arbitrary non-zero default
    FmtData.u.pe.stackcommit = DEF_VALUE;
    FmtData.u.pe.os2.segment_shift = 9;    // 512 byte arbitrary rounding
    return( true );
}

bool ProcVXD( void )
/************************/
{
    return( ProcOS2() );
/*
    ProcOne( VXDFormatKeywords, SEP_NO, false );
    FmtData.u.pe.heapcommit   = PE_DEF_HEAP_COMMIT; // arbitrary non-zero default.
    FmtData.u.pe.os2.heapsize = PE_DEF_HEAP_SIZE;   // another arbitrary non-zero default
    FmtData.u.pe.stackcommit = DEF_VALUE;
    return( true );
*/
}

bool ProcWindows( void )
/*****************************/
{
    return( ProcOS2() );
}

void ChkBase( offset align )
/*********************************/
// Note: align must be a power of 2
{
    if( FmtData.objalign != NO_BASE_SPEC && FmtData.objalign > align ) {
        align = FmtData.objalign;
    }
    if( FmtData.base != NO_BASE_SPEC && (FmtData.base & (align - 1)) != 0 ) {
        LnkMsg( LOC+LINE+WRN+MSG_OFFSET_MUST_BE_ALIGNED, "l", align );
        FmtData.base = ROUND_UP( FmtData.base, align );
    }
}

void SetOS2Fmt( void )
/*********************/
// set up the structures needed to be able to process something in OS/2 mode.
{
    if( LinkState & FMT_INITIALIZED )
        return;
    LinkState |= FMT_INITIALIZED;
    FmtData.u.os2.flags = MULTIPLE_AUTO_DATA;
    FmtData.u.os2.heapsize = 0;
    FmtData.u.os2.segment_shift = 0;
    FmtData.u.os2.module_name = NULL;
    FmtData.u.os2.stub_file_name = NULL;
    FmtData.u.os2.os2_seg_flags = NULL;
    FmtData.description = NULL;
    FmtData.u.os2.exports = NULL;
    FmtData.u.os2.old_lib_name = NULL;
    if( FmtData.type & MK_WINDOWS ) {
        FmtData.def_seg_flags |= SEG_PRELOAD;
    }
    Extension = E_LOAD;
    ChkBase(64*1024);
}

void FreeOS2Fmt( void )
/**********************/
{
    _LnkFree( FmtData.u.os2.stub_file_name );
    _LnkFree( FmtData.u.os2.module_name );
    _LnkFree( FmtData.u.os2.old_lib_name );
    _LnkFree( FmtData.description );
    FreeImpNameTab();
    FreeExportList();
    FreeSegFlags( (seg_flags *) FmtData.u.os2.os2_seg_flags );
}

bool ProcLE( void )
/************************/
{
    return( true );
}

bool ProcLX( void )
/************************/
{
    return( true );
}

bool ProcOS2DLL( void )
/****************************/
{
    FmtData.dll = true;
    Extension = E_DLL;
    if( FmtData.type & MK_WINDOWS ) {
        FmtData.u.os2.flags &= ~MULTIPLE_AUTO_DATA;
        FmtData.u.os2.flags |= SINGLE_AUTO_DATA;
        FmtData.def_seg_flags |= SEG_PURE | SEG_MOVABLE;
    }
    if( ProcOne( Init_Keywords, SEP_NO, false ) ) {
        if( !ProcOne( Term_Keywords, SEP_NO, false ) ) {
            if( FmtData.u.os2.flags & INIT_INSTANCE_FLAG ) {
                FmtData.u.os2.flags |= TERM_INSTANCE_FLAG;
            }
        }
    }
    return( true );
}

bool ProcPhysDevice( void )
/********************************/
{
    FmtData.dll = true;
    Extension = E_DLL;
    FmtData.u.os2.flags |= PHYS_DEVICE;
    return( true );
}

bool ProcVirtDevice( void )
/********************************/
{
    FmtData.dll = true;
    Extension = E_DLL;
    FmtData.u.os2.flags |= VIRT_DEVICE;
    return( true );
}

bool ProcPM( void )
/************************/
{
    FmtData.u.os2.flags |= PM_APPLICATION;
    return( true );
}

bool ProcPMCompatible( void )
/**********************************/
{
    FmtData.u.os2.flags |= PM_COMPATIBLE;
    return( true );
}

bool ProcPMFullscreen( void )
/**********************************/
{
    FmtData.u.os2.flags |= PM_NOT_COMPATIBLE;
    return( true );
}

bool ProcMemory( void )
/****************************/
{
    FmtData.u.os2.flags |= CLEAN_MEMORY;
    return( true );
}

bool ProcFont( void )
/**************************/
{
    FmtData.u.os2.flags |= PROPORTIONAL_FONT;
    return( true );
}

bool ProcDynamicDriver( void )
/********************************/
{
    FmtData.u.os2.flags |= VIRT_DEVICE;
    return( true );
}

bool ProcStaticDriver( void )
/********************************/
{
    FmtData.u.os2.flags |= PHYS_DEVICE;
    return( true );
}

bool ProcInitGlobal( void )
/********************************/
{
    FmtData.u.os2.flags &= ~INIT_INSTANCE_FLAG;
    return( true );
}

bool ProcInitInstance( void )
/**********************************/
{
    FmtData.u.os2.flags |= INIT_INSTANCE_FLAG;
    return( true );
}

bool ProcInitThread( void )
/********************************/
{
    FmtData.u.os2.flags |= INIT_THREAD_FLAG;
    return( true );
}

bool ProcTermGlobal( void )
/********************************/
{
    FmtData.u.os2.flags &= ~TERM_INSTANCE_FLAG;
    return( true );
}

bool ProcTermInstance( void )
/**********************************/
{
    FmtData.u.os2.flags |= TERM_INSTANCE_FLAG;
    return( true );
}

bool ProcTermThread( void )
/********************************/
{
    FmtData.u.os2.flags |= TERM_THREAD_FLAG;
    return( true );
}

static bool getsegflags( void )
/*****************************/
{
    os2_seg_flags   *entry;

    Token.thumb = true;
    _ChkAlloc( entry, sizeof( os2_seg_flags ) );
    entry->specified = 0;
    entry->flags = FmtData.def_seg_flags;    // default value.
    entry->name = NULL;
    entry->type = SEGFLAG_SEGMENT;
    entry->next = FmtData.u.os2.os2_seg_flags;
    FmtData.u.os2.os2_seg_flags = entry;
    ProcOne( SegDesc, SEP_NO, false );          // look for an optional segdesc
    if( entry->type != SEGFLAG_CODE && entry->type != SEGFLAG_DATA ) {
        if( !GetToken( SEP_NO, TOK_INCLUDE_DOT ) ) {
            FmtData.u.os2.os2_seg_flags = entry->next;
            _LnkFree( entry );
            return( false );
        }
        entry->name = tostring();
    }
    while( ProcOne( SegModel, SEP_NO, false ) ) {
    }
    return( true );
}

bool ProcOS2Class( void )
/******************************/
{
    FmtData.u.os2.os2_seg_flags->type = SEGFLAG_CLASS;
    return( true );
}

bool ProcSegType( void )
/*****************************/
{
    if( !ProcOne( SegTypeDesc, SEP_NO, false ) ) {
        LnkMsg( LOC+LINE+WRN+MSG_INVALID_TYPE_DESC, NULL );
    }
    return( true );
}

bool ProcSegCode( void )
/*****************************/
{
    FmtData.u.os2.os2_seg_flags->type = SEGFLAG_CODE;
    return( true );
}

bool ProcSegData( void )
/*****************************/
{
    FmtData.u.os2.os2_seg_flags->type = SEGFLAG_DATA;
    return( true );
}

bool ProcPreload( void )
/*****************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_PRELOAD ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags |= SEG_PRELOAD;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_PRELOAD;
    return( true );
}

bool ProcLoadoncall( void )
/********************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_PRELOAD ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_PRELOAD;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_PRELOAD;
    return( true );
}

bool ProcIopl( void )
/**************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_IOPL_SPECD ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_LEVEL_3;
    FmtData.u.os2.os2_seg_flags->flags |= SEG_LEVEL_2;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_IOPL_SPECD;
    return( true );
}

bool ProcNoIopl( void )
/****************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_IOPL_SPECD ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags |= SEG_LEVEL_3;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_IOPL_SPECD;
    return( true );
}

bool ProcExecuteonly( void )
/*********************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_RFLAG ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags |= SEG_READ_ONLY;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_READ_ONLY;
    return( true );
}

bool ProcExecuteread( void )
/*********************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_RFLAG ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_READ_ONLY;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_READ_ONLY;
    return( true );
}

bool ProcReadOnly( void )
/******************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_RFLAG ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags |= SEG_READ_ONLY;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_READ_SPECD;
    return( true );
}

bool ProcReadWrite( void )
/******************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_RFLAG ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_READ_ONLY;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_READ_SPECD;
    return( true );
}

bool ProcShared( void )
/****************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_PURE ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags |= SEG_PURE;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_PURE;
    return( true );
}

bool ProcNonShared( void )
/*******************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_PURE ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_PURE;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_PURE;
    return( true );
}

bool ProcConforming( void )
/********************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_CONFORMING ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags |= SEG_CONFORMING;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_CONFORMING;
    return( true );
}

bool ProcNonConforming( void )
/***********************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_CONFORMING ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_CONFORMING;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_CONFORMING;
    return( true );
}

bool ProcMovable( void )
/*****************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_MOVABLE ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags |= SEG_MOVABLE;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_MOVABLE;
    return( true );
}

bool ProcFixed( void )
/*****************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_MOVABLE ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_MOVABLE;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_MOVABLE;
    return( true );
}

bool ProcNonPageable( void )
/*********************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_NOPAGE ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags |= SEG_NOPAGE;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_NOPAGE;
    return( true );
}

bool ProcPageable( void )
/******************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_NOPAGE ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_NOPAGE;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_NOPAGE;
    return( true );
}

bool ProcDiscardable( void )
/*********************************/
{
    FmtData.u.os2.os2_seg_flags->flags |= SEG_DISCARD;
    return( true );
}

bool ProcNonDiscardable( void )
/*********************************/
{
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_DISCARD;
    return( true );
}

bool ProcOS2Dynamic( void )
/********************************/
{
//    FmtData.u.os2.os2_seg_flags->flags |= SEG_DISCARD;
    return( true );
}

bool ProcInvalid( void )
/*****************************/
{
    FmtData.u.os2.os2_seg_flags->flags |= SEG_INVALID;
    return( true );
}

bool ProcPermanent( void )
/*******************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_RESIDENT ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags |= SEG_RESIDENT;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_RESIDENT;
    return( true );
}

bool ProcNonPermanent( void )
/**********************************/
{
    if( FmtData.u.os2.os2_seg_flags->specified & SEG_RESIDENT ) {
        LnkMsg( LOC+LINE+WRN+MSG_SEG_FLAG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.os2_seg_flags->flags &= ~SEG_RESIDENT;
    FmtData.u.os2.os2_seg_flags->specified |= SEG_RESIDENT;
    return( true );
}

bool ProcContiguous( void )
/********************************/
{
    FmtData.u.os2.os2_seg_flags->flags |= SEG_CONTIGUOUS;
    return( true );
}

bool ProcSingle( void )
/****************************/
{
    if( CmdFlags & CF_AUTO_SEG_FLAG ) {
        LnkMsg( LOC+LINE+WRN+MSG_AUTO_SEG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.flags |= SINGLE_AUTO_DATA;
    FmtData.u.os2.flags &= ~MULTIPLE_AUTO_DATA;
    return( true );
}

bool ProcMultiple( void )
/******************************/
{
    if( CmdFlags & CF_AUTO_SEG_FLAG ) {
        LnkMsg( LOC+LINE+WRN+MSG_AUTO_SEG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.flags &= ~SINGLE_AUTO_DATA;
    FmtData.u.os2.flags |= MULTIPLE_AUTO_DATA;
    return( true );
}

bool ProcNone( void )
/**************************/
{
    if( CmdFlags & CF_AUTO_SEG_FLAG ) {
        LnkMsg( LOC+LINE+WRN+MSG_AUTO_SEG_MULT_DEFD, NULL );
    }
    FmtData.u.os2.flags &= ~(SINGLE_AUTO_DATA | MULTIPLE_AUTO_DATA);
    return( true );
}

bool ProcRunNative( void )
/*******************************/
{
    FmtData.u.pe.subsystem = PE_SS_NATIVE;
    ParseVersion();
    return( true );
}

bool ProcRunWindows( void )
/********************************/
{
    FmtData.u.pe.subsystem = PE_SS_WINDOWS_GUI;
    ParseVersion();
    return( true );
}

bool ProcRunConsole( void )
/********************************/
{
    FmtData.u.pe.subsystem = PE_SS_WINDOWS_CHAR;
    ParseVersion();
    return( true );
}

bool ProcRunPosix( void )
/******************************/
{
    FmtData.u.pe.subsystem = PE_SS_POSIX_CHAR;
    ParseVersion();
    return( true );
}

bool ProcRunOS2( void )
/****************************/
{
    FmtData.u.pe.subsystem = PE_SS_OS2_CHAR;
    ParseVersion();
    return( true );
}


bool ProcRunDosstyle( void )
/*********************************/
{
    FmtData.u.pe.subsystem = PE_SS_PL_DOSSTYLE;
    FmtData.u.pe.signature = PL_SIGNATURE;
    ParseVersion();
    return( true );
}

bool ProcTNT( void )
/*************************/
{
    FmtData.u.pe.signature = PL_SIGNATURE;
    return( true );
}

bool ProcRDOS( void )
/*************************/
{
    FmtData.u.pe.subsystem = PE_SS_RDOS;

    FmtData.u.pe.osmajor = 8;
    FmtData.u.pe.osminor = 8;
    FmtData.u.pe.osv_specd = true;

    FmtData.u.pe.submajor = 1;
    FmtData.u.pe.subminor = 0;
    FmtData.u.pe.sub_specd = true;

    return( true );
}

bool ProcEFI( void )
/*************************/
{
    Extension = E_EFI;
    FmtData.u.pe.subsystem = PE_SS_EFI_BOOT;
    ParseVersion();

    return( true );
}

static void ParseVersion( void )
/******************************/
{
    ord_state   retval;

    if( !GetToken( SEP_EQUALS, TOK_NORMAL ) )
        return;
    FmtData.u.pe.submajor = 0;
    FmtData.u.pe.subminor = 0;
    retval = getatoi( &FmtData.u.pe.submajor );
    if( retval != ST_IS_ORDINAL ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "subsystem" );
        return;
    }
    FmtData.u.pe.sub_specd = true;
    if( !GetToken( SEP_PERIOD, TOK_NORMAL ) ) {  /*if we don't get a minor number*/
       return;                          /* that's OK */
    }
    retval = getatoi( &FmtData.u.pe.subminor );
    if( retval != ST_IS_ORDINAL ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "subsystem" );
    }
}

static bool AddResource( void )
/*****************************/
{
    DoAddResource( tostring() );
    return( true );
}

bool ProcResource( void )
/******************************/
{
    return( ProcArgList( &AddResource, TOK_INCLUDE_DOT | TOK_IS_FILENAME ) );
}

enum{
    valid_result    = 0x01,
    major_valid     = 0x02,
    minor_valid     = 0x04,
    revision_valid  = 0x08
};

typedef struct tagVersBlock
{
    unsigned_32 major;
    unsigned_32 minor;
    unsigned_32 revision;
}VersBlock;

static unsigned_32 ProcGenericVersion( VersBlock *pVers, unsigned_32 major_limit, unsigned_32 minor_limit, unsigned_32 revision_limit)
{
    unsigned_32 state = 0;
    ord_state   retval;
    unsigned_32 value;

    if(NULL == pVers) {
        return( state );
    }
    if( !GetToken( SEP_EQUALS, TOK_NORMAL ) ) {
        return( state );
    }

    retval = getatol( &value );
    if( retval != ST_IS_ORDINAL ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "version" );
        return( state );
    } else if( ( major_limit ) && ( value > major_limit ) ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "version" );
        return( state );
    }
    /*
    //  From now on, all results are valid despite warnings
    */
    pVers->major = value;
    pVers->minor = 0;
    pVers->revision = 0;
    state |= (valid_result | major_valid);

    if( !GetToken( SEP_PERIOD, TOK_NORMAL ) ) {  /*if we don't get a minor number*/
       return( state );                      /* that's OK */
    }
    retval = getatol( &value );
    if( retval != ST_IS_ORDINAL ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "version" );
        return( state );
    } else if( ( minor_limit ) && ( value > minor_limit ) ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "version" );
        return( state );
    }

    pVers->minor = value;
    state |= minor_valid;

    if( !GetToken( SEP_PERIOD, TOK_NORMAL ) ) {  /* if we don't get a revision*/
        return( state );                 /* that's all right */
    }

    /*
    //  Netware supports a revision field 0-26 (null or a-z(A-Z))
    */
    retval = getatol( &value );
    if( retval == ST_NOT_ORDINAL && Token.len == 1 ) {
        value  = tolower( *Token.this ) - 'a' + 1;
    } else if( retval == ST_NOT_ORDINAL ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "version" );
        return( state );
    }

    if( ( revision_limit ) && ( value > revision_limit ) ) {
        LnkMsg( LOC+LINE+WRN+MSG_VALUE_INCORRECT, "s", "version" );
        return( state );
    }
    pVers->revision = value;
    state |= revision_valid;
    return( state );
}

bool     ProcLinkVersion( void )
{
    unsigned_32 result;
    VersBlock   vb;

    result = ProcGenericVersion( &vb , 255, 255, 0);
    if( (result & valid_result) == 0 ) {
        return( false );    /* error has occurred */
    }

    FmtData.u.pe.lnk_specd = true;
    FmtData.u.pe.linkmajor = (result & major_valid) ? vb.major : 0;
    FmtData.u.pe.linkminor = (result & minor_valid) ? vb.minor : 0;

    return( true );
}

bool     ProcOsVersion( void )
{
    unsigned_32 result;
    VersBlock   vb;

    result = ProcGenericVersion( &vb , 0, 99, 0);   /* from old default of 100 max */
    if( (result & valid_result) == 0 ) {
        return( false );    /* error has occurred */
    }

    FmtData.u.pe.osv_specd = true;
    FmtData.u.pe.osmajor = (result & major_valid) ? vb.major : 0;
    FmtData.u.pe.osminor = (result & minor_valid) ? vb.minor : 0;

    return( true );
}

bool     ProcChecksum( void )
{
    FmtData.u.pe.checksumfile = true;
    return( true );
}

bool ProcLargeAddressAware( void )
/********************************/
{
    FmtData.u.pe.largeaddressaware = true;
    FmtData.u.pe.nolargeaddressaware = false;
    return( true );
}
bool ProcNoLargeAddressAware( void )
/********************************/
{
    FmtData.u.pe.nolargeaddressaware = true;
    FmtData.u.pe.largeaddressaware = false;
    return( true );
}
