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
* Description:  I/O support routines.
*
****************************************************************************/


#include "plusplus.h"
#include <stdarg.h>
#if defined(__UNIX__)
 #include <dirent.h>
#else
 #include <direct.h>
#endif
#include "wio.h"
#include "preproc.h"
#include "memmgr.h"
#include "iosupp.h"
#include "cgdata.h"
#include "fname.h"
#include "hfile.h"
#include "initdefs.h"
#include "stats.h"
#include "pcheader.h"
#include "ring.h"
#include "brinfo.h"
#include "autodept.h"
#include "iopath.h"
#include "sysdep.h"
#include "ialias.h"

#include "clibext.h"


typedef struct buf_alloc BUF_ALLOC;
struct buf_alloc {              // BUF_ALLOC -- allocated buffer
    BUF_ALLOC* next;            // - next buffer allocation
    FILE* file;                 // - file in question
    void* buffer;               // - allocated buffer
};


static carve_t carve_buf;       // carver: BUF_ALLOC
static BUF_ALLOC* buffers;      // allocated buffers
static int  temphandle;         // handle for temp file
static char *tempname;          // name of temp file
static DISK_ADDR tempBlock;     // next available block in temp file
static unsigned outFileChecked; // mask for checking output files

                                // template for work file name
static char workFile[] = "__wrk0__.tmp";

#define MAX_TMP_PATH (_MAX_PATH - sizeof( workFile ) - 1)       // 1 character for missing path separator

#if defined(__OS2__) || defined(__DOS__) || defined(__NT__) || defined(__RDOS__)

static const char* pathSrc[] =        // paths for source file
    {   "..\\cpp"
    ,   "..\\c"
    ,   NULL
    };

static const char* pathHdr[] =        // paths for header files
    {   "..\\h"
    ,   "..\\include"
    ,   NULL
    };

static const char* pathCmd[] =        // paths for command files
    {   "..\\occ"
    ,   NULL
    };

static const char* extsHdr[] =        // extensions for header files
    {   ".hpp"
    ,   ".h"
    ,   NULL
    };

static const char* extsSrc[] =        // extensions for source files
    {   ".cpp"
    ,   ".cc"
    ,   ".c"
    ,   NULL
    };

static const char* extsCmd[] =        // extensions for command files
    {   ".occ"
    ,   NULL
    };

static const char* extsOut[] =        // extensions for output files
    {   ".obj"
    ,   ".i"
    ,   ".err"
    ,   ".mbr"
    ,   ".def"
#ifdef OPT_BR
    ,   ".brm"
#endif
    ,   ".d"
    ,   ".obj"
    };

#elif defined(__UNIX__)

static const char* pathSrc[] =        // paths for source file
    {   "../C"
    ,   "../cpp"
    ,   "../c"
    ,   NULL
    };

static const char* pathHdr[] =        // paths for header files
    {   "../H"
    ,   "../h"
    ,   NULL
    };

static const char* pathCmd[] =        // paths for command files
    {   "../occ"
    ,   NULL
    };

static const char* extsHdr[] =        // extensions for header files
    {   ".H"
    ,   ".hpp"
    ,   ".h"
    ,   NULL
    };

static const char* extsSrc[] =        // extensions for source files
    {   ".C"
    ,   ".cpp"
    ,   ".cc"
    ,   ".c"
    ,   NULL
    };

static const char* extsCmd[] =        // extensions for command files
    {   ".occ"
    ,   NULL
    };

static const char* extsOut[] =        // extensions for output files
    {   ".o"
    ,   ".i"
    ,   ".err"
    ,   ".mbr"
    ,   ".def"
#ifdef OPT_BR
    ,   ".brm"
#endif
    ,   ".d"
    ,   ".o"
    };

#else

#error IOSUPP not configured for OS

#endif

static char *FNameBuf = NULL;   // file name buffer for output files

char *IoSuppOutFileName(        // BUILD AN OUTPUT NAME FROM SOURCE NAME
    out_file_type typ )         // - extension
{
    char *drive;
    char *dir;
    char *fname;
    const char *ext;
    char       *path;
    bool use_defaults;
    unsigned mask;
    FILE *try_create;
    auto char buff[_MAX_PATH2];
    auto char extsrc[_MAX_EXT];
    char *extf;

    path = WholeFName;
    use_defaults = true;
    switch( typ ) {
    case OFT_DEF:
#ifdef OPT_BR
    case OFT_BRI:
#endif
        break;
    case OFT_DEP:
        if( DependFileName != NULL ) {
            path = DependFileName;
            use_defaults = false;
        }
        break;
    case OFT_ERR:
        if( ErrorFileName == NULL )
            return( NULL );
        outFileChecked |= 1 << typ; // don't create a file. it's just a name.
        path = ErrorFileName;
        use_defaults = false;
        break;
    case OFT_SRCDEP:
        outFileChecked |= 1 << typ;
        if( SrcDepFileName != NULL ) {
            path = SrcDepFileName;
        }
        break;
    case OFT_TRG:
        outFileChecked |= 1 << typ; // don't create a file. it's just a name.
        if( TargetFileName != NULL ) {
            path = TargetFileName;
            use_defaults = false;
            break;
        }
        /* fall through */
    case OFT_PPO:
    case OFT_OBJ:
    case OFT_MBR:
        if( ObjectFileName != NULL ) {
            path = ObjectFileName;
            if( typ != OFT_MBR ) {
                use_defaults = false;
            }
        }
        break;
    }
    _splitpath2( path, buff, &drive, &dir, &fname, &extf );
    ext = extf;
    if( typ == OFT_SRCDEP ) {
        if( ext == NULL || ext[0] == '\0' ) {
            if( SrcDepFileName != NULL ) {
                _splitpath2( WholeFName, extsrc, NULL, NULL, NULL, &extf );
                ext = extf;
            }
        }
    } else if( use_defaults || ext[0] == '\0' ) {
        ext = extsOut[typ];
    }
    if( fname[0] == '\0' || fname[0] == '*' ) {
        fname = ModuleName;
    }
    if( use_defaults ) {
        drive = "";
        dir = "";
    }
    _makepath( FNameBuf, drive, dir, fname, ext );
    mask = 1 << typ;
    if( (outFileChecked & mask) == 0 ) {
        outFileChecked |= mask;
        try_create = fopen( FNameBuf, "w" );
        if( try_create != NULL ) {
            fclose( try_create );
        } else {
            CErr2p( ERR_CANNOT_CREATE_OUTPUT_FILE, FNameBuf );
        }
    }
    return( FNameBuf );
}


static void set_buffering(      // SET BUFFERING FOR AN OPEN FILE
    FILE *fp,                   // - opened file
    size_t buf_size,            // - buffer size
    int mode )                  // - buffering mode
{
    BUF_ALLOC* ba = RingCarveAlloc( carve_buf, &buffers );
    ba->file = fp;
    ba->buffer = CMemAlloc( buf_size );
    setvbuf( fp, ba->buffer, mode, buf_size );
}


void IoSuppSetBuffering(        // SET FULL BUFFERING FOR AN OPEN FILE
    FILE *fp,                   // - opened file
    size_t buf_size )           // - buffer size
{
    set_buffering( fp, buf_size, _IOFBF );
}


void IoSuppSetLineBuffering(    // SET LINE BUFFERING FOR AN OPEN FILE
    FILE *fp,                   // - opened file
    size_t buf_size )           // - buffer size
{
    set_buffering( fp, buf_size, _IOLBF );
}


static void freeBuffer(         // FREE A BUFFER
    BUF_ALLOC* ba )             // - allocated buffer
{
    CMemFree( ba->buffer );
    RingPrune( &buffers, ba );
    CarveFree( carve_buf, ba );
}


bool IoSuppCloseFile(           // CLOSE FILE IF OPENED
    FILE **file_ptr )           // - addr( file pointer )
{
    bool ok;                    // - return: true ==> was open
    BUF_ALLOC* ba;              // - current allocated buffer

    if( *file_ptr == NULL ) {
        ok = false;
    } else {
        RingIterBegSafe( buffers, ba ) {
            if( *file_ptr == ba->buffer ) {
                freeBuffer( ba );
                break;
            }
        } RingIterEndSafe( ba );
        SrcFileFClose( *file_ptr );
        *file_ptr = NULL;
        ok = true;
    }
    return( ok );
}


struct path_descr               // path description
{   char buffer[_MAX_PATH2];    // - buffer
    char *drv;                  // - drive
    char *dir;                  // - directory
    char *fnm;                  // - file name
    char *ext;                  // - extension
};


static void splitFileName(      // SPLIT APART PATH/FILENAME
    const char *name,           // - name to be split
    struct path_descr *descr )  // - descriptor
{
    _splitpath2( name
               , descr->buffer
               , &descr->drv
               , &descr->dir
               , &descr->fnm
               , &descr->ext );
}


static void makeDirName(        // MAKE FILE NAME (WITHOUT DRIVE)
    char *pp,                   // - target location
    struct path_descr *nd )     // - name descriptor
{
    _makepath( pp, NULL, nd->dir, nd->fnm, nd->ext );
}


static bool openSrc(            // ATTEMPT TO OPEN FILE
    char *name,                 // - file name
    src_file_type typ )         // - type of file being opened
{
    pch_absorb pch_OK;          // - pre-compiled header load status
    FILE *fp;                   // - file pointer
    time_t ftime;
#ifdef OPT_BR
    bool might_browse;          // - true ==> might browse, if right file type
#endif

    if( SrcFileProcessOnce( name ) ) {
        SrcFileOpen( NULL, name, 0 );
        return( true );
    }
    ftime = SysFileTime( name );
    fp = SrcFileFOpen( name, SFO_SOURCE_FILE );
    if( fp == NULL ) {
        return( false );
    }
#ifdef OPT_BR
    might_browse = false;
#endif
    if( CompFlags.watch_for_pcheader ) {
        CompFlags.watch_for_pcheader = false;
        pch_OK = PCHeaderAbsorb( name );
        if( pch_OK != PCHA_OK ) {
            SrcFileSetCreatePCHeader();
            SrcFileOpen( fp, name, ftime );
#ifdef OPT_BR
            might_browse = true;
#endif
        } else {
            SrcFileOpen( NULL, name, 0 );
            fclose( fp );
        }
    } else {
        SrcFileOpen( fp, name, ftime );
        if( typ == FT_SRC ) {
            SetSrcFilePrimary();
        }
#ifdef OPT_BR
        might_browse = true;
#endif
    }
#ifdef OPT_BR
    if( might_browse ) {
        switch( typ ) {
        case FT_SRC:
        case FT_LIBRARY:
        case FT_HEADER:
        case FT_HEADER_FORCED:
        case FT_HEADER_PRE:
            BrinfOpenSource( SrcFileCurrent() );
            break;
        }
    }
#endif
    return( true );
}


static const char *openSrcExt(  // ATTEMPT TO OPEN FILE (EXT. TO BE APPENDED)
    const char *ext,            // - extension
    struct path_descr *nd,      // - name descriptor
    src_file_type typ )         // - type of file being opened
{
    char name[_MAX_PATH];       // - buffer for file name

    _makepath( name, nd->drv, nd->dir, nd->fnm, ext );
    /* so we can tell if the open worked */
    if( openSrc( name, typ ) )
        return( (ext != NULL) ? ext : "" );
    return( NULL );
}


static const char *openSrcExts( // ATTEMPT TO OPEN FILE (EXT.S TO BE APPENDED)
    const char **exts,          // - extensions
    struct path_descr *nd,      // - name descriptor
    src_file_type typ )         // - type of file being opened
{
    const char *ext;            // - current extension

    if( nd->ext[0] == '\0' ) {
        ext = openSrcExt( NULL, nd, typ );
        if( ext == NULL ) {
            switch( typ ) {
            case FT_SRC:
                if( CompFlags.dont_autogen_ext_src )
                    exts = NULL;
                break;
            case FT_HEADER:
            case FT_HEADER_FORCED:
            case FT_HEADER_PRE:
            case FT_LIBRARY:
                if( CompFlags.dont_autogen_ext_inc )
                    exts = NULL;
                break;
            default:
                break;
            }
            if( exts != NULL ) {
                while( (ext = *exts++) != NULL ) {
                    ext = openSrcExt( ext, nd, typ );
                    if( ext != NULL ) {
                        break;
                    }
                }
            }
        }
    } else {
        ext = openSrcExt( nd->ext, nd, typ );
    }
    return( ext );
}


static bool openSrcPath(        // ATTEMPT TO OPEN FILE (PATH TO BE PREPENDED)
    const char *path,           // - path
    struct path_descr *fd,      // - file descriptor
    const char **exts,          // - file extensions
    src_file_type typ )         // - type of file being opened
{
    bool ok;                    // - return: true ==> opened
    struct path_descr pd;       // - path descriptor
    char dir[_MAX_PATH * 2];    // - new path
    char *pp;                   // - pointer into path
    const char *ext;            // - extension opened

    ok = false;
    dir[0] = '\0';
    splitFileName( path, &pd );
    if( fd->drv[0] == '\0' ) {
        pp = stxpcpy( dir, path );
    } else if( pd.drv[0] == '\0' ) {
        pp = stxpcpy( dir, fd->drv );
        pp = stxpcpy( pp, path );
    } else {
        return( ok );
    }
    if( pp > dir ) {
        if( !IS_PATH_SEP( pp[-1] ) ) {
            *pp++ = DIR_SEP;
        }
    }
    makeDirName( pp, fd );
    splitFileName( dir, &pd );
    ext = openSrcExts( exts, &pd, typ );
    if( ext != NULL ) {
        ok = true;
        if( ( typ == FT_SRC ) && ( ext != fd->ext ) ) {
            _makepath( dir, fd->drv, fd->dir, fd->fnm, ext );
            WholeFName = FNameAdd( dir );
        }
    }
    return( ok );
}

static bool try_open_file( const char *path, struct path_descr *fd, struct path_descr *fa, const char **exts, src_file_type typ )
{
    bool    ok;
    bool    truncated;
    char    save_chr_name;
    char    save_chr_ext;

    ok = openSrcPath( path, fd, exts, typ );
    if( ok ) {
        return( ok );
    }
    if( fa != NULL ) {
        ok = openSrcPath( path, fa, exts, typ );
        if( ok ) {
            return( ok );
        }
    }
    if( CompFlags.check_truncated_fnames ) {
        save_chr_name = fd->fnm[8];
        save_chr_ext = fd->ext[4];
        truncated = false;
        if( strlen( fd->fnm ) > 8 ) {
            fd->fnm[8] = '\0';
            truncated = true;
        }
        if( strlen( fd->ext ) > 4 ) {
            fd->ext[4] = '\0';
            truncated = true;
        }
        if( truncated ) {
            ok = openSrcPath( path, fd, exts, typ );
            if( ok ) {
                return( ok );
            }
            fd->fnm[8] = save_chr_name;
            fd->ext[4] = save_chr_ext;
        }
    }
    return( ok );
}

static bool doIoSuppOpenSrc(    // OPEN A SOURCE FILE (PRIMARY,HEADER)
    struct path_descr *fd,      // - descriptor for file name
    struct path_descr *fai,     // - descriptor for alias file name
    src_file_type typ )         // - type of search path to use
{
    const char  **paths;        // - optional paths to prepend
    const char  **exts;         // - optional extensions to append
    bool ok;                    // - return: true ==> opened
    const char  *path;          // - next path
    char bufpth[_MAX_PATH];     // - buffer for next path
    SRCFILE curr;               // - current included file
    SRCFILE stdin_srcfile;      // - srcfile for stdin
    struct path_descr idescr;   // - descriptor for included file
    LINE_NO dummy;              // - dummy line number holder
    char prevpth[_MAX_PATH];    // - buffer for previous path
    bool alias_abs;
    bool alias_check;
    struct path_descr *fa;

    alias_abs = false;
    alias_check = false;
    ok = false;
    fa = NULL;
    switch( typ ) {
    case FT_SRC:
        exts = extsSrc;
        if( fd->fnm[0] == '\0' && fd->ext[0] == '.' && fd->ext[1] == '\0' ) {
            if( ErrCount != 0 ) {
                // command line errors may result in "." as the input name
                // so the user thinks that the compiler is hung!
                return( false );
            }
            WholeFName = FNameAdd( "stdin" );
            stdin_srcfile = SrcFileOpen( stdin, WholeFName, 0 );
            SrcFileNotAFile( stdin_srcfile );
            ok = true;
            break;
        }
        ok = openSrcPath( "", fd, exts, typ );
        if( ok )
            break;
        if( !CompFlags.ignore_default_dirs && !IS_DIR_SEP( fd->dir[0] ) ) {
            for( paths = pathSrc; (path = *paths) != NULL; ++paths ) {
                ok = openSrcPath( path, fd, exts, typ );
                if( ok ) {
                    break;
                }
            }
        }
        break;
    case FT_HEADER:
    case FT_HEADER_FORCED:
    case FT_HEADER_PRE:
    case FT_LIBRARY:
        exts = extsHdr;
        alias_abs = ( fai != NULL && ( fai->drv[0] != '\0' || IS_DIR_SEP( fai->dir[0] ) ) );
        // have to look for absolute paths
        if( fd->drv[0] != '\0' || IS_DIR_SEP( fd->dir[0] ) ) {
            if( alias_abs )
                fa = fai;
            alias_abs = false;
            ok = try_open_file( "", fd, fa, exts, typ );
            break;
        }
        /* if alias contains abs path then check it after last check for regular name */
        alias_check = ( fai != NULL && fai->drv[0] == '\0' && !IS_DIR_SEP( fai->dir[0] ) );
        if( alias_check )
            fa = fai;
        if( typ != FT_LIBRARY && !IS_DIR_SEP( fd->dir[0] ) ) {
            if( CompFlags.ignore_default_dirs ) {
                bufpth[0] = '\0';
                curr = SrcFileCurrent();
                if( curr != NULL ) {
                    splitFileName( SrcFileName( curr ), &idescr );
                    _makepath( bufpth, idescr.drv, idescr.dir, NULL, NULL );
                }
                ok = try_open_file( bufpth, fd, fa, exts, typ );
                if( ok ) {
                    break;
                }
            } else {
                if( !CompFlags.ignore_current_dir ) {
                    // check for current directory
                    ok = try_open_file( "", fd, fa, exts, typ );
                    if( ok ) {
                        break;
                    }
                }
                /* check directories of currently included files */
                prevpth[0] = '\xff';  /* to make it not compare with anything else */
                prevpth[1] = '\0';
                curr = SrcFileCurrent();
                for( ; curr != NULL; ) {
                    splitFileName( SrcFileName( curr ), &idescr );
                    _makepath( bufpth, idescr.drv, idescr.dir, NULL, NULL );
                    /*optimization: don't try and open if in previously checked dir*/
                    if( strcmp( bufpth, prevpth ) != 0 ) {
                        ok = try_open_file( bufpth, fd, fa, exts, typ );
                        if( ok ) {
                            break;
                        }
                    }
                    curr = SrcFileIncluded( curr, &dummy );
                    strcpy( prevpth, bufpth );
                }
                if( ok ) {
                    break;
                }
            }
        }
        HFileListStart();
        for( ;; ) {
            HFileListNext( bufpth );
            if( *bufpth == '\0' )
                break;
            ok = try_open_file( bufpth, fd, fa, exts, typ );
            if( ok ) {
                break;
            }
        }
        if( ok ) {
            break;
        }
        if( typ != FT_LIBRARY && !CompFlags.ignore_default_dirs && !IS_DIR_SEP( fd->dir[0] ) ) {
            for( paths = pathHdr; (path = *paths) != NULL; ++paths ) {
                ok = try_open_file( path, fd, fa, exts, typ );
                if( ok ) {
                    break;
                }
            }
        }
        if( alias_abs ) {
            ok = openSrcPath( "", fai, exts, typ );
        }
        break;
    case FT_CMD:
        exts = extsCmd;
        ok = openSrcPath( "", fd, exts, typ );
        if( ok )
            break;
        if( !IS_DIR_SEP( fd->dir[0] ) ) {
            for( paths = pathCmd; (path = *paths) != NULL; ++paths ) {
                ok = openSrcPath( path, fd, exts, typ );
                if( ok ) {
                    break;
                }
            }
        }
        break;
    default:
        exts = NULL;
        break;
    }
    if( ok ) {
        switch( typ ) {
        case FT_CMD:
            SetSrcFileCommand();
            break;
        case FT_LIBRARY:
            SetSrcFileLibrary();
            break;
        }
    }
    return( ok );
}


bool IoSuppOpenSrc(             // OPEN A SOURCE FILE (PRIMARY,HEADER)
    const char *file_name,      // - supplied file name
    src_file_type typ )         // - type of search path to use
{
    struct path_descr   fd;     // - descriptor for file name
    struct path_descr   fa;     // - descriptor for alias file name
    struct path_descr   *fap;   // - pointer to descriptor for alias file name
    const char          *alias_file_name;

#ifdef OPT_BR
    if( NULL != file_name
     && file_name[0] != '\0' ) {
        TOKEN_LOCN locn;
        switch( typ ) {
        case FT_SRC:
        case FT_HEADER:
        case FT_HEADER_FORCED:
        case FT_HEADER_PRE:
        case FT_LIBRARY:
            SrcFileGetTokenLocn( &locn );
            BrinfIncludeSource( file_name, &locn );
            break;
        }
    }
#endif
    splitFileName( file_name, &fd );
    fap = NULL;
    switch( typ ) {
    case FT_HEADER:
    case FT_HEADER_FORCED:
    case FT_HEADER_PRE:
    case FT_LIBRARY:
        // See if there's an alias for this file name
        alias_file_name = IAliasLookup( file_name, typ == FT_LIBRARY );
        if( alias_file_name != file_name ) {
            splitFileName( alias_file_name, &fa );
            fap = &fa;
        }
        break;
    }
    return( doIoSuppOpenSrc( &fd, fap, typ ) );
}

static void tempFname( char *fname )
{
    const char  *env;
    size_t      len;

#if defined(__UNIX__)
    env = CppGetEnv( "TMPDIR" );
    if( env == NULL )
        env = CppGetEnv( "TMP" );
#else
    env = CppGetEnv( "TMP" );
#endif
    if( env == NULL )
        env = "";

    strncpy( fname, env, MAX_TMP_PATH );
    fname[MAX_TMP_PATH] = '\0';
    len = strlen( fname );
    fname += len;
    if( len > 0 && !IS_PATH_SEP( fname[-1] ) ) {
        *fname++ = DIR_SEP;
    }
    strcpy( fname, workFile );
}

#if defined(__DOS__)

#include "tinyio.h"
extern void __SetIOMode( int, unsigned );

#endif

static void ioSuppError(        // SIGNAL I/O ERROR AND ABORT
    MSG_NUM error_code )            // - error code
{
    CErr2( error_code, errno );
    CSuicide();
}


static void ioSuppReadError(      // SIGNAL ERROR ON READ
    void )
{
    ioSuppError( ERR_WORK_FILE_READ_ERROR );
}


static void ioSuppWriteError(     // SIGNAL ERROR ON WRITE
    void )
{
    ioSuppError( ERR_WORK_FILE_WRITE_ERROR );
}

#ifdef __QNX__
#define AMODE   (O_RDWR | O_CREAT | O_EXCL | O_BINARY | O_TEMP)
#else
#define AMODE   (O_RDWR | O_CREAT | O_EXCL | O_BINARY)
#endif

static void ioSuppTempOpen(             // OPEN TEMPORARY FILE
    void )
{
    auto char   fname[_MAX_PATH];

    for(;;) {
        tempFname( fname );
#if defined(__DOS__)
        {
            tiny_ret_t  rc;

            rc = TinyCreateNew( fname, 0 );
            if( TINY_ERROR( rc ) ) {
                temphandle = -1;
            } else {
                temphandle = TINY_INFO( rc );
                __SetIOMode( temphandle, _READ | _WRITE | _BINARY );
            }
        }
#else
        temphandle = open( fname, AMODE, PMODE_RW );
#endif
        if( temphandle != -1 )
            break;
        if( workFile[5] == 'Z' ) {
            temphandle = -1;
            break;
        }
        switch( workFile[5] ) {
        case '9':
            workFile[5] = 'A';
            break;
        case 'I':
            workFile[5] = 'J';  /* file-system may be EBCDIC */
            break;
        case 'R':
            workFile[5] = 'S';  /* file-system may be EBCDIC */
            break;
        default:
            ++workFile[5];
            break;
        }
    }
#if defined(__UNIX__)
    /* Under POSIX it's legal to remove a file that's open. The file
       space will be reclaimed when the handle is closed. This makes
       sure that the work file always gets removed. */
    remove( fname );
    tempname = NULL;
#else
    tempname = FNameAdd( fname );
#endif
    if( temphandle == -1 ) {
        ioSuppError( ERR_UNABLE_TO_OPEN_WORK_FILE );
    }
}


char *IoSuppFullPath(           // GET FULL PATH OF FILE NAME (ALWAYS USE RET VALUE)
    char *name,                 // - input file name
    char *buff,                 // - output buffer
    unsigned size )             // - output buffer size
{
    DbgAssert( size >= _MAX_PATH );
#ifndef NDEBUG
    // caller should use return value only!
    // - this code will make sure caller doesn't use buff
    *buff = '.';
    ++buff;
    --size;
#endif
    return( _getFilenameFullPath( buff, name, size ) );
}


DISK_ADDR IoSuppTempNextBlock(  // GET NEXT BLOCK NUMBER
    unsigned num_blocks )       // - number of blocks allocated
{
    DISK_ADDR retn;

    retn = tempBlock + 1;
    tempBlock += num_blocks;
    return( retn );
}


void IoSuppTempWrite(           // WRITE TO TEMPORARY FILE
    DISK_ADDR   block_num,      // - block within temp file
    size_t      block_size,     // - size of blocks
    void        *data )         // - buffer to write
{
    if( temphandle == -1 )
        ioSuppTempOpen();
    block_num--;
    if( -1 == lseek( temphandle, block_size * block_num, SEEK_SET ) ) {
        ioSuppWriteError();
    }
    if( block_size != write( temphandle, data, block_size ) ) {
        ioSuppWriteError();
    }
}


void IoSuppTempRead(            // READ FROM TEMPORARY FILE
    DISK_ADDR   block_num,      // - block within temp file
    size_t      block_size,     // - size of blocks
    void        *data )         // - buffer to read
{
    if( temphandle == -1 )
        ioSuppTempOpen();
    block_num--;
    if( -1 == lseek( temphandle, block_size * block_num, SEEK_SET ) ) {
        ioSuppReadError();
    }
    if( block_size != read( temphandle, data, block_size ) ) {
        ioSuppReadError();
    }
}


static bool pathExists(         // TEST IF A PATH EXISTS
    const char *path )          // - path to be tested
{
    DIR *dir;                   // - control for directory
    bool ok;                    // - return: true ==> directory exists

    ok = false;
    dir = opendir( path );
    if( dir != NULL ) {
        closedir( dir );
        ok = true;
    }
    return( ok );
}

static void setPaths(           // SET PATHS (IF THEY EXIST)
    const char **vect )         // - the vector of potential paths
{
    const char  **dest;         // - place to store
    const char  **test;         // - path to test
    const char  *path;          // - current path

    dest = vect;
    test = vect;
    for( ;; ) {
        path = *test;
        if( path == NULL )
            break;
        if( pathExists( path ) ) {
            *dest++ = path;
        }
        ++test;
    }
    *dest = NULL;
}


static void ioSuppInit(         // INITIALIZE IO SUPPORT
    INITFINI* defn )            // - definition
{
    /* unused parameters */ (void)defn;

    outFileChecked = 0;
    tempBlock = 0;
    tempname = NULL;
    temphandle = -1;
    workFile[5] = '0';
    FNameBuf = CMemAlloc( _MAX_PATH );
    carve_buf = CarveCreate( sizeof( BUF_ALLOC ), 8 );
    setPaths( pathSrc );
    setPaths( pathHdr );
    setPaths( pathCmd );
}


static void ioSuppFini(         // FINALIZE IO SUPPORT
    INITFINI* defn )            // - definition
{
    /* unused parameters */ (void)defn;

    if( temphandle != -1 ) {
        close( temphandle );
        if( tempname != NULL ) {
            remove( tempname );
        }
    }
    while( NULL != buffers ) {
        freeBuffer( buffers );
    }
    CarveDestroy( carve_buf );
    CMemFree( FNameBuf );
}


INITDEFN( io_support, ioSuppInit, ioSuppFini )
