/****************************************************************************
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
* Description:  Built-in builder commands.
*
****************************************************************************/


#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/stat.h>
#ifdef __UNIX__
    #include <utime.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <fnmatch.h>
#else
    #include <sys/utime.h>
    #include <direct.h>
    #include <dos.h>
  #ifdef __WATCOMC__
    #include <fnmatch.h>
  #endif
#endif
#include "watcom.h"
#include "builder.h"
#include "pmake.h"
#include "wio.h"
#include "memutils.h"

#include "clibext.h"
#include "bldstruc.h"


#define WILD_METAS      "*?"

#ifdef __UNIX__
  #define MASK_ALL_ITEMS  "*"
#else
  #define MASK_ALL_ITEMS  "*.*"
#endif

#ifndef __UNIX__
#define ENTRY_NOT_CHANGED1() (access( entry_dst, R_OK ) == 0 && _dos_getfileattr( entry_src, &attr ) == 0 && (attr & _A_ARCH) == 0)
#define ENTRY_NOT_CHANGED2() (access( entry_dst, R_OK ) == 0 && (dent->d_attr & _A_ARCH) == 0)
#else
/* Linux has (strangely) no 'archive' attribute, compare modification times */
#define ENTRY_NOT_CHANGED1() ((stat(entry_dst, &statdst) == 0) && (stat(entry_src, &statsrc) == 0) && (statdst.st_mtime == statsrc.st_mtime))
#define ENTRY_NOT_CHANGED2() ((stat(entry_dst, &statdst) == 0) && (stat(entry_src, &statsrc) == 0) && (statdst.st_mtime == statsrc.st_mtime))
#endif

#define COPY_BUFF_SIZE  (32 * 1024)

typedef struct dd {
  struct dd     *next;
  char          attr;
  char          name[1];
} iolist;

static int      rflag = false;
static int      fflag = false;
static int      sflag = true;

static int RecursiveRM( const char *dir );

static void LogDir( const char *dir )
{
    Log( false, "%s", LogDirEquals( dir ) );
}

static int ProcSet( char *cmd )
{
    char        *var;
    char        *rep;

    var = cmd;
    rep = strchr( cmd, '=' );
    if( rep == NULL )
        return( 1 );
    *rep++ = '\0';
    /* Our setenv() is extended vs. POSIX, check for blank value is not necessary */
    /* Watcom implementation is non-conforming to POSIX, return value is incorrect in some cases */
    if( *rep == '\0' ) {
#if defined( __WATCOMC__ ) && ( __WATCOMC__ < 1300 )
        setenv( var, NULL, 1 );
#else
        /* Delete the environment variable! */
        unsetenv( var );
#endif
        return( 0 );
    }
    return( setenv( var, rep, 1 ) );
}

void ResetArchives( copy_entry *list )
{
    copy_entry  *next;
#ifndef __UNIX__
    unsigned    attr;
#endif

    while( list != NULL ) {
        next = list->next;
#ifndef __UNIX__
        if( _dos_getfileattr( list->src, &attr ) == 0 ) {
            _dos_setfileattr( list->src, attr & ~_A_ARCH );
        }
#endif
        MFree( list );
        list = next;
    }
}

static int IsDotOrDotDot( const char *fname )
{
    /* return 1 if fname is "." or "..", 0 otherwise */
    return( fname[0] == '.' && ( fname[1] == 0 || fname[1] == '.' && fname[2] == 0 ) );
}

static int BuildList( const char *src, char *dst, bool test_abit, bool cond_copy, copy_entry **list )
{
    copy_entry          *head;
    copy_entry          *curr;
    copy_entry          **owner;
    char                *end;
    char                path_buffer[_MAX_PATH2];
    char                full[_MAX_PATH];
    char                srcdir[_MAX_PATH];
    char                *drive;
    char                *dir;
    char                *fn;
    char                *ext;
    DIR                 *directory;
    struct dirent       *dent;
#ifndef __UNIX__
    unsigned            attr;
#else
    struct stat         statsrc, statdst;
    char                pattern[_MAX_PATH];
#endif
    int                 rc;
    char                entry_src[_MAX_PATH];
    char                entry_dst[_MAX_PATH];

    *list = NULL;
    strcpy( srcdir, src );
    end = &dst[strlen( dst ) - 1];
    while( IS_BLANK( end[0] ) ) {
        --end;
    }
    end[1] = '\0';
    if( strpbrk( srcdir, WILD_METAS ) == NULL ) {
        /* no wild cards */
        _fullpath( entry_src, srcdir, sizeof( entry_src ) );
        switch( *end ) {
        case '\\':
        case '/':
            /* need to append source file name */
            _splitpath2( srcdir, path_buffer, &drive, &dir, &fn, &ext );
            _makepath( full, NULL, dst, fn, ext );
            _fullpath( entry_dst, full, sizeof( entry_dst ) );
            break;
        default:
            _fullpath( entry_dst, dst, sizeof( entry_dst ) );
            break;
        }
        if( test_abit ) {
            if( ENTRY_NOT_CHANGED1() ) {
                return( 0 );
            }
        }
        head = MAlloc( sizeof( *head ) );
        head->next = NULL;
        strcpy( head->src, entry_src );
        strcpy( head->dst, entry_dst );
        *list = head;
        return( 0 );
    }
#ifdef __UNIX__
    _splitpath2( srcdir, path_buffer, &drive, &dir, &fn, &ext );
    _makepath( srcdir, drive, dir, NULL, NULL );
    _makepath( pattern, NULL, NULL, fn, ext );
    if( srcdir[0] == '\0' ) {
        srcdir[0] = '.';
        srcdir[1] = '\0';
    }
#endif
    head = NULL;
    rc = 1;
    directory = opendir( srcdir );
    if( directory == NULL ) {
        if( !cond_copy ) {
            Log( false, "Can not open source directory '%s': %s\n", srcdir, strerror( errno ) );
        }
    } else {
#ifdef __UNIX__
        char *srcdir_end = srcdir + strlen( srcdir );
#endif
        owner = &head;
        while( (dent = readdir( directory )) != NULL ) {
#ifdef __UNIX__
            struct stat buf;

            if( fnmatch( pattern, dent->d_name, FNM_PATHNAME | FNM_NOESCAPE ) == FNM_NOMATCH ) {
                if( !IsDotOrDotDot( dent->d_name ) )
                    rc = 0;
                continue;
            }

            strcpy( srcdir_end, dent->d_name );
            stat( srcdir, &buf );
            *srcdir_end = '\0';
            if( S_ISDIR( buf.st_mode ) ) {
                if( !IsDotOrDotDot( dent->d_name ) )
                    rc = 0;
                continue;
            }
#else
            if( dent->d_attr & _A_VOLID ) {
                continue;
            } else if( dent->d_attr & _A_SUBDIR ) {
                if( !IsDotOrDotDot( dent->d_name ) )
                    rc = 0;
                continue;
            }
#endif
            rc = 0;
            _splitpath2( srcdir, path_buffer, &drive, &dir, &fn, &ext );
            _makepath( full, drive, dir, dent->d_name, NULL );
            _fullpath( entry_src, full, sizeof( entry_src ) );
            strcpy( full, dst );
            switch( *end ) {
            case '\\':
            case '/':
                strcat( full, dent->d_name );
                break;
            }
            _fullpath( entry_dst, full, sizeof( entry_dst ) );
            if( test_abit ) {
                if( ENTRY_NOT_CHANGED2() ) {
                    continue;
                }
            }
            curr = MAlloc( sizeof( *curr ) );
            curr->next = NULL;
            strcpy( curr->src, entry_src );
            strcpy( curr->dst, entry_dst );
            *owner = curr;
            owner = &curr->next;
        }
        closedir( directory );
    }
    *list = head;
    if( cond_copy ) {
        return( 0 );
    }
    return( rc );
}

static int mkdir_nested( const char *path )
/*****************************************/
{
#ifdef __UNIX__
    struct stat sb;
#else
    unsigned    attr;
#endif
    char        pathname[ FILENAME_MAX ];
    char        *p;
    char        *end;

    p = pathname;
    strncpy( pathname, path, FILENAME_MAX );
    end = pathname + strlen( pathname );

#ifndef __UNIX__
    /* special case for drive letters */
    if( p[0] != '\0' && p[1] == ':' ) {
        p += 2;
    }
#endif
    /* skip initial path separator if present */
    if( (p[0] == '/') || (p[0] == '\\') )
        ++p;

    /* find the next path component */
    while( p < end ) {
        while( (p < end) && (*p != '/') && (*p != '\\') )
            ++p;
        *p = '\0';

        /* check if pathname exists */
#ifdef __UNIX__
        if( stat( pathname, &sb ) != 0 ) {
#else
        if( _dos_getfileattr( pathname, &attr ) != 0 ) {
#endif
            int rc;

#ifdef __UNIX__
            rc = mkdir( pathname, S_IRWXU | S_IRWXG | S_IRWXO );
#else
            rc = mkdir( pathname );
#endif
            if( rc != 0 ) {
                Log( false, "Can not create directory '%s': %s\n", pathname, strerror( errno ) );
                return( -1 );
            }
        } else {
            /* make sure it really is a directory */
#ifdef __UNIX__
            if( !S_ISDIR( sb.st_mode ) ) {
#else
            if( (attr & _A_SUBDIR) == 0 ) {
#endif
                Log( false, "Can not create directory '%s': file with the same name already exists\n", pathname );
                return( -1 );
            }
        }
        /* put back the path separator - forward slash always works */
        *p++ = '/';
    }
    return( 0 );
}

static int ProcOneCopy( const char *src, char *dst, bool cond_copy, char *copy_buff )
{
    FILE            *sp;
    FILE            *dp;
    size_t          len;
    size_t          out;
    struct stat     srcbuf;
    struct utimbuf  dstbuf;

    sp = fopen( src, "rb" );
    if( sp == NULL ) {
        if( cond_copy ) {
            return( 0 );    // Quietly ignore missing source
        } else {
            Log( false, "Can not open '%s' for reading: %s\n", src, strerror( errno ) );
            return( 1 );
        }
    }
    dp = fopen( dst, "wb" );
    if( dp == NULL ) {
        len = strlen( dst );
        while( len-- > 0 ) {
            char c = dst[len];
            if( c == '/' || c == '\\' ) {
                dst[len] = '\0';
                mkdir_nested( dst );
                dst[len] = c;
                dp = fopen( dst, "wb" );
                break;
            }
        }
        if( dp == NULL ) {
            Log( false, "Can not open '%s' for writing: %s\n", dst, strerror( errno ) );
            fclose( sp );
            return( 1 );
        }
    }
    Log( Quiet, "Copying '%s' to '%s'...\n", src, dst );
    while( (len = fread( copy_buff, 1, COPY_BUFF_SIZE, sp )) != 0 ) {
        if( ferror( sp ) ) {
            Log( false, "Error reading '%s': %s\n", src, strerror( errno ) );
            fclose( sp );
            fclose( dp );
            return( 1 );
        }
        out = fwrite( copy_buff, 1, len, dp );
        if( ferror( dp ) ) {
            Log( false, "Error writing '%s': %s\n", dst, strerror( errno ) );
            fclose( sp );
            fclose( dp );
            return( 1 );
        }
        if( out != len ) {
            Log( false, "Error writing '%s': Disk full\n", dst );
            fclose( sp );
            fclose( dp );
            return( 1 );
        }
    }
    fclose( sp );
    fclose( dp );

    /* make real copy, set the date back */
    stat( src, &srcbuf );
    dstbuf.actime = srcbuf.st_atime;
    dstbuf.modtime = srcbuf.st_mtime;
    utime( dst, &dstbuf );
#ifdef __UNIX__
    /* copy permissions: mostly necessary for the "x" bit */
    // some files is copied from the source tree with the read-only permission
    // for next run we need the write permission for the current user as minimum
    chmod( dst, srcbuf.st_mode | S_IWUSR );
#endif
    return( 0 );
}

static int ProcCopy( char *cmd, bool test_abit, bool cond_copy, bool ignore_errors )
{
    char        *dst;
    copy_entry  *list;
    copy_entry  *next;
    int         res;

    for( dst = cmd; *dst != '\0'; ++dst ) {
        if( IS_BLANK( *dst ) ) {
            *dst++ = '\0';
            dst = SkipBlanks( dst );
            break;
        }
    }
    if( *dst == '\0' ) {
        Log( false, "Missing parameter\n" );
        return( 1 );
    }
    res = BuildList( cmd, dst, test_abit, cond_copy, &list );
    if( res == 0 && list != NULL ) {
        char    *copy_buff = MAlloc( COPY_BUFF_SIZE );
        for( ; list != NULL; list = next ) {
            next = list->next;
            if( res == 0 || ignore_errors ) {
                int     rc;

                rc = ProcOneCopy( list->src, list->dst, cond_copy, copy_buff );
                if( rc != 0 ) {
                    res = rc;
#ifndef __UNIX__
                } else if( test_abit ) {
                    list->next = IncludeStk->reset_abit;
                    IncludeStk->reset_abit = list;
                    continue;
#endif
                }
            }
            MFree( list );
        }
        MFree( copy_buff );
    }
    return( res );
}

static int ProcMkdir( const char *cmd )
{
    return( mkdir_nested( cmd ) );
}

void PMakeOutput( const char *str )
{
    Log( false, "%s\n", str );
}

static int DoPMake( pmake_data *data )
{
    pmake_list  *curr;
    int         res;
    char        cmd[PMAKE_COMMAND_SIZE];
    int         rc = 0;

    for( curr = data->dir_list; curr != NULL; curr = curr->next ) {
        res = SysChdir( curr->dir_name );
        if( res != 0 ) {
            if( !data->ignore_errors ) {
                return( res );
            }
            Log( false, "'cd %s' non-zero return: %d\n", curr->dir_name, res );
            rc = res;
            continue;
        }
        getcwd( IncludeStk->cwd, sizeof( IncludeStk->cwd ) );
        if( data->display )
            LogDir( IncludeStk->cwd );
        PMakeCommand( data, cmd );
        res = SysRunCommand( cmd );
        if( res != 0 ) {
            if( !data->ignore_errors ) {
                return( res );
            }
            Log( false, "'%s' non-zero return: %d\n", cmd, res );
            rc = res;
        }
    }
    return( rc );
}

static int ProcPMake( char *cmd, bool ignore_errors )
{
    pmake_data  *data;
    int         res;
    char        save[_MAX_PATH];

    data = PMakeBuild( cmd );
    if( data == NULL )
        return( 1 );
    if( data->want_help || data->signaled ) {
        PMakeCleanup( data );
        return( 2 );
    }
    data->ignore_errors = ignore_errors;
    strcpy( save, IncludeStk->cwd );
    res = DoPMake( data );
    PMakeCleanup( data );
    SysChdir( save );
    getcwd( IncludeStk->cwd, sizeof( IncludeStk->cwd ) );
    return( res );
}

static int remove_item( const char *name, bool dir )
{
    int         rc;
    char        *err_msg;
    char        *inf_msg;

    if( dir ) {
        err_msg = "Unable to delete directory %s\n";
        inf_msg = "Directory %s deleted\n";
        rc = rmdir( name );
    } else {
        err_msg = "Unable to delete file %s\n";
        inf_msg = "File %s deleted\n";
        rc = unlink( name );
    }
    if( rc != 0 && fflag && errno == EACCES ) {
        rc = chmod( name, PMODE_RW );
        if( rc == 0 ) {
            if( dir ) {
                rc = rmdir( name );
            } else {
                rc = unlink( name );
            }
        }
    }
    if( rc != 0 && fflag && errno == ENOENT ) {
        rc = 0;
    }
    if( rc != 0 ) {
        rc = errno;
        Log( false, err_msg, name );
        return( rc );
    } else if( !sflag ) {
        Log( false, inf_msg, name );
    }
    return( 0 );
}

/* DoRM - perform RM on a specified file */
static int DoRM( const char *f )
{
    iolist              *tmp;
    iolist              *dhead = NULL;
    iolist              *dtail = NULL;

    char                fpath[_MAX_PATH];
    char                fname[_MAX_PATH];
    char                *fpathend;

    size_t              i;
    size_t              j;
    size_t              len;
    DIR                 *d;
    struct dirent       *nd;
    int                 rc;
    int                 retval = 0;

    /* separate file name to path and file name parts */
    len = strlen( f );
    for( i = len; i > 0; --i ) {
        char ch = f[i - 1];
        if( ch == '/' || ch == '\\' || ch == ':' ) {
            break;
        }
    }
    j = i;
    /* if no path then use current directory */
    if( i == 0 ) {
        fpath[i++] = '.';
        fpath[i++] = '/';
    } else {
        memcpy( fpath, f, i );
    }
    fpathend = fpath + i;
    *fpathend = '\0';
#ifdef __UNIX__
    memcpy( fname, f + j, len - j + 1 );
#else
    if( strcmp( f + j, MASK_ALL_ITEMS ) == 0 ) {
        fname[0] = '*';
        fname[1] = '\0';
    } else {
        memcpy( fname, f + j, len - j + 1 );
    }
#endif
    d = opendir( fpath );
    if( d == NULL ) {
        Log( false, "File (%s) not found.\n", f );
        return( ENOENT );
    }

    while( (nd = readdir( d )) != NULL ) {
#ifdef __UNIX__
        struct stat buf;

        if( fnmatch( fname, nd->d_name, FNM_PATHNAME | FNM_NOESCAPE ) == FNM_NOMATCH )
#else
        if( fnmatch( fname, nd->d_name, FNM_PATHNAME | FNM_NOESCAPE | FNM_IGNORECASE ) == FNM_NOMATCH )
#endif
            continue;
        /* set up file name, then try to delete it */
        len = strlen( nd->d_name );
        memcpy( fpathend, nd->d_name, len );
        fpathend[len] = 0;
        len += i + 1;
#ifdef __UNIX__
        stat( fpath, &buf );
        if( S_ISDIR( buf.st_mode ) ) {
#else
        if( nd->d_attr & _A_SUBDIR ) {
#endif
            /* process a directory */
            if( IsDotOrDotDot( nd->d_name ) )
                continue;

            if( rflag ) {
                /* build directory list */
                tmp = MAlloc( offsetof( iolist, name ) + len );
                tmp->next = NULL;
                if( dtail == NULL ) {
                    dhead = tmp;
                } else {
                    dtail->next = tmp;
                }
                dtail = tmp;
                memcpy( tmp->name, fpath, len );
            } else {
                Log( false, "%s is a directory, use -r\n", fpath );
                retval = EACCES;
            }
#ifdef __UNIX__
        } else if( access( fpath, W_OK ) == -1 && errno == EACCES && !fflag ) {
#else
        } else if( (nd->d_attr & _A_RDONLY) && !fflag ) {
#endif
            Log( false, "%s is read-only, use -f\n", fpath );
            retval = EACCES;
        } else {
            rc = remove_item( fpath, false );
            if( rc != 0 ) {
                retval = rc;
            }
        }
    }
    closedir( d );
    /* process any directories found */
    for( tmp = dhead; tmp != NULL; tmp = dhead ) {
        dhead = tmp->next;
        rc = RecursiveRM( tmp->name );
        if( rc != 0 ) {
            retval = rc;
        }
        MFree( tmp );
    }
    return( retval );
}

/* RecursiveRM - do an RM recursively on all files */
static int RecursiveRM( const char *dir )
{
    int         rc;
    int         rc2;
    char        fname[_MAX_PATH];

    /* purge the files */
    strcpy( fname, dir );
    strcat( fname, "/" MASK_ALL_ITEMS );
    rc = DoRM( fname );
    /* purge the directory */
    rc2 = remove_item( dir, true );
    if( rc == 0 )
        rc = rc2;
    return( rc );
}

static char *GetString( const char *cmd, char *buffer )
{
    char        c;
    char        quotechar;

    while( isspace( *cmd ) )
        ++cmd;
    if( *cmd == '\0' )
        return( NULL );
    quotechar = ( *cmd == '"' ) ? *cmd++ : '\0';
    for( ; (c = *cmd) != '\0'; ++cmd ) {
        if( c == quotechar ) {
            ++cmd;
            break;
        }
        *buffer++ = c;
    }
    *buffer = '\0';
    return( (char *)cmd );
}

static int ProcRm( char *cmd )
{
    char    buffer[_MAX_PATH];
    int     retval = 0;

    /* gather options */
    for( ;; ) {
        while( isspace( *cmd ) )
            ++cmd;
        if( *cmd != '-' )
            break;
        ++cmd;
        while( isalpha( *cmd ) ) {
            switch( *cmd++ ) {
            case 'f':
                fflag = true;
                break;
            case 'R':
            case 'r':
                rflag = true;
                break;
            case 'v':
                sflag = false;
                break;
            default:
                return( 1 );
            }
        }
    }

    if( rflag ) {
        /* process -r option */
        while( (cmd = GetString( cmd, buffer )) != NULL ) {
            if( strcmp( buffer, MASK_ALL_ITEMS ) == 0 ) {
                int rc = RecursiveRM( "." );
                if( rc != 0 ) {
                    retval = rc;
                }
            } else if( strpbrk( buffer, WILD_METAS ) != NULL ) {
                // wild cards is not processed for directories
                continue;
            } else {
                struct stat buf;
                if( stat( buffer, &buf ) == 0 ) {
                    if( S_ISDIR( buf.st_mode ) ) {
                        int rc = RecursiveRM( buffer );
                        if( rc != 0 ) {
                            retval = rc;
                        }
                    } else {
                        int rc = DoRM( buffer );
                        if( rc != 0 ) {
                            retval = rc;
                        }
                    }
                }
            }
        }
    } else {
        /* run through all specified files */
        while( (cmd = GetString( cmd, buffer )) != NULL ) {
            int rc = DoRM( buffer );
            if( rc != 0 ) {
                retval = rc;
            }
        }
    }
    return( retval );
}

int RunIt( char *cmd, bool ignore_errors, bool *res_nolog )
{
    int     res;

    #define BUILTIN( b )        \
        (strnicmp( cmd, b, sizeof( b ) - 1 ) == 0 && cmd[sizeof(b)-1] == ' ')
    *res_nolog = false;
    if( BUILTIN( "CD" ) ) {
        res = SysChdir( SkipBlanks( cmd + sizeof( "CD" ) ) );
        if( res == 0 ) {
            getcwd( IncludeStk->cwd, sizeof( IncludeStk->cwd ) );
        }
    } else if( BUILTIN( "CDSAY" ) ) {
        res = SysChdir( SkipBlanks( cmd + sizeof( "CDSAY" ) ) );
        if( res == 0 ) {
            getcwd( IncludeStk->cwd, sizeof( IncludeStk->cwd ) );
            LogDir( IncludeStk->cwd );
        }
    } else if( BUILTIN( "SET" ) ) {
        res = ProcSet( SkipBlanks( cmd + sizeof( "SET" ) ) );
    } else if( BUILTIN( "ECHO" ) ) {
        Log( Quiet, "%s\n", SkipBlanks( cmd + sizeof( "ECHO" ) ) );
        res = 0;
    } else if( BUILTIN( "ERROR" ) ) {
        Log( Quiet, "%s\n", SkipBlanks( cmd + sizeof( "ERROR" ) ) );
        res = 1;
    } else if( BUILTIN( "COPY" ) ) {
        res = ProcCopy( SkipBlanks( cmd + sizeof( "COPY" ) ), false, false, ignore_errors );
        *res_nolog = true;
    } else if( BUILTIN( "ACOPY" ) ) {
        res = ProcCopy( SkipBlanks( cmd + sizeof( "ACOPY" ) ), true, false, ignore_errors );
        *res_nolog = true;
    } else if( BUILTIN( "CCOPY" ) ) {
        res = ProcCopy( SkipBlanks( cmd + sizeof( "CCOPY" ) ), false, true, ignore_errors );
        *res_nolog = true;
    } else if( BUILTIN( "ACCOPY" ) ) {
        res = ProcCopy( SkipBlanks( cmd + sizeof( "ACCOPY" ) ), true, true, ignore_errors );
        *res_nolog = true;
    } else if( BUILTIN( "MKDIR" ) ) {
        res = ProcMkdir( SkipBlanks( cmd + sizeof( "MKDIR" ) ) );
    } else if( BUILTIN( "PMAKE" ) ) {
        res = ProcPMake( SkipBlanks( cmd + sizeof( "PMAKE" ) ), ignore_errors );
        *res_nolog = ignore_errors;
    } else if( BUILTIN( "RM" ) ) {
        res = ProcRm( SkipBlanks( cmd + sizeof( "RM" ) ) );
    } else if( cmd[0] == '!' ) {
        res = SysRunCommand( SkipBlanks( cmd + 1 ) );
    } else {
        res = SysRunCommand( cmd );
    }
    return( res );
}
