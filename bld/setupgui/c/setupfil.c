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
* Description:  Routines to modify registry and configuration files.
*
****************************************************************************/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#if defined( __DOS__ )
    #include <dos.h>
#elif defined( __WINDOWS__ ) || defined( __NT__ )
    #include <windows.h>
#elif defined( __OS2__ )
    #define INCL_DOSMISC
    #include <os2.h>
#endif
#include "wio.h"
#include "watcom.h"
#include "gui.h"
#include "guiutil.h"
#include "setup.h"
#include "setupinf.h"
#include "genvbl.h"
#include "gendlg.h"
#include "utils.h"
#include "iopath.h"

#include "clibext.h"


#if defined( __UNIX__ )
    #define SETENV          "export "
    #define SETENV_LEN      7
    #define ENV_NAME        "$%s"
#else
    #define SETENV          "SET "
    #define SETENV_LEN      4
    #define ENV_NAME        "%%%s%%"
#endif

#if defined( __UNIX__ )
    #define BATCHEXT        ".sh"
#elif defined( __OS2__ )
    #define BATCHEXT        ".cmd"
#else
    #define BATCHEXT        ".bat"
#endif

typedef enum {
    VAR_SETENV_ASSIGN,  // format is 'SETENV NAME = VALUE' and inf var name is "NAME"
    VAR_ASSIGN_SETENV,  // format is 'SETENV NAME = VALUE' and inf var name is "SETENV NAME"
    VAR_ASSIGN,         // format is 'NAME = VALUE"
    VAR_CMD,            // format is 'NAME VALUE"
    VAR_ERROR,
} var_type;

extern int              GetOptionVarValue( vhandle var_handle, bool is_minimal );

static char             new_val[MAXENVVAR + 1];

#if defined( __NT__ )
static struct reg_location {
    HKEY    key;
    bool    key_is_open;
    bool    modify;
} RegLocation[NUM_REG_LOCATIONS];
#endif

#if !defined( __UNIX__ )

#if !defined( __OS2__ )
static char     *WinDotCom = NULL;
#endif

static char     OrigAutoExec[] = "?:\\AUTOEXEC.BAT";
static char     OrigConfig[] = "?:\\CONFIG.SYS";


static char GetBootDrive( void )
/*******************************/
{
#ifdef __OS2__
    UCHAR           DataBuf[10];
    APIRET          rc;

    rc = DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, DataBuf, sizeof( DataBuf ) );
    if( rc != 0 ) {
        printf("DosQuerySysInfo error: return code = %ld ",rc);
    }
    return( *(unsigned long *)DataBuf );
#elif defined( __DOS__ )
    union REGS  r;

    r.w.ax = 0x3305;
    intdos( &r, &r );
    return( r.h.dl );
#else
    return( 0 );
#endif
}


static bool GetOldConfigFileDir( char *newauto, const char *drive_path, bool uninstall )
/**************************************************************************************/
{
    char        drive[_MAX_DRIVE];

    if( uninstall ) {
        _splitpath( drive_path, drive, NULL, NULL, NULL );
        if( drive[0] == '\0' ) {
            _fullpath( newauto, drive_path, _MAX_PATH );
            _splitpath( newauto, drive, NULL, NULL, NULL );
        }
        _makepath( newauto, drive, NULL, NULL, NULL );
    } else {
        strcpy( newauto, GetVariableStrVal( "DstDir" ) );
    }

    return( true );
}

static char *StrNDup( char *str, size_t len )
/*******************************************/
{
    char                *new;

    new = GUIMemAlloc( len + 1 );
    if( new != NULL ) {
        memcpy( new, str, len );
        new[len] = '\0';
    }
    return( new );
}

#endif  // !__UNIX__

static void NoDupPaths( char *old_value, char *new_value, char delim )
/********************************************************************/
{
    char        *semi;
    char        *dup;
    char        *look;
    size_t      len;
    char        *new_curr;

    new_curr = new_value;
    for( ;; ) {
        semi = strchr( new_curr, delim );
        if( semi != NULL ) {
            *semi = '\0';
        }
        look = old_value;
        len = strlen( new_curr );
        while( (dup = stristr( look, new_curr )) != NULL ) {
            if( dup[len] == delim ) {
                if( dup == old_value || dup[-1] == delim ) {
                    memmove( dup, dup + len + 1, strlen( dup + len + 1 ) + 1 );
                } else {
                    look = dup + len;
                }
            } else if( dup[len] == '\0' ) {
                if( dup == old_value ) {
                    dup[0] = '\0';
                } else if( dup[-1] == delim ) {
                    memmove( dup - 1, dup + len, strlen( dup + len ) + 1 );
                } else {
                    look = dup + len;
                }
            } else {
                look = dup + len;
            }
        }
        if( semi == NULL )
            break;
        *semi = delim;
        new_curr = semi + 1;
    }
    if( old_value[0] != '\0' ) {
        len = strlen( old_value );
        if( old_value[len - 1] == delim ) {
            old_value[len - 1] = '\0';
        }
    }
}

static void modify_value( char *value, char *new_value, append_mode append, bool uninstall )
/******************************************************************************************/
{
    char    buf[MAXENVVAR + 1];

    NoDupPaths( value, new_value, PATH_LIST_SEP );
    if( !uninstall ) {
        if( append == AM_AFTER ) {
            sprintf( buf, "%s%c%s", value, PATH_LIST_SEP, new_value );
            strcpy( value, buf );
        } else if( append == AM_BEFORE ) {
            sprintf( buf, "%s%c%s", new_value, PATH_LIST_SEP, value );
            strcpy( value, buf );
        } else {
            strcpy( value, new_value );
        }
    }
}

static void output_line( char *buf, var_type vt, const char *name, char *value )
/******************************************************************************/
{
    if( value[0] == '\0' )
        vt = VAR_ERROR;
    switch( vt ) {
    case VAR_SETENV_ASSIGN:
        sprintf( buf, SETENV "%s=%s", name, value );
        break;
    case VAR_ASSIGN:
    case VAR_ASSIGN_SETENV:
        sprintf( buf, "%s=%s", name, value );
        break;
    case VAR_CMD:
        sprintf( buf, "%s %s", name, value );
        break;
    case VAR_ERROR:
        buf[0] = '\0';
        break;
    default:
        break;
    }
}

static void modify_value_libpath( char *val_before, char *val_after, char *new_value, append_mode append )
/********************************************************************************************************/
{
    char    buf[MAXENVVAR + 1];

    if( append == AM_AFTER ) {
        NoDupPaths( val_before, new_value, PATH_LIST_SEP );
        NoDupPaths( val_after, new_value, PATH_LIST_SEP );
        sprintf( buf, "%s%c%s", val_after, PATH_LIST_SEP, new_value );
        strcpy( val_after, buf );
    } else if( append == AM_BEFORE ) {
        NoDupPaths( val_before, new_value, PATH_LIST_SEP );
        NoDupPaths( val_after, new_value, PATH_LIST_SEP );
        sprintf( buf, "%s%c%s", new_value, PATH_LIST_SEP, val_before );
        strcpy( val_before, buf );
    } else {
        strcpy( val_before, new_value );
        val_after[0] = '\0';
    }
}

#if !defined( __UNIX__ )
static var_type parse_line( char *line, char *name, char **value, var_type vt_setenv )
/************************************************************************************/
{
    char        c;
    var_type    vt;

    // parsed line formats
    // SETENV NAME = VALUE
    // NAME = VALUE
    // NAME VALUE
    *name = '\0';
    while( isspace( *line ) )
        ++line;
    if( memicmp( line, SETENV, SETENV_LEN ) == 0 ) {
        line += SETENV_LEN;
        while( isspace( *line ) )
            ++line;
        if( vt_setenv == VAR_ASSIGN_SETENV ) {
            strcpy( name, SETENV );
            name += SETENV_LEN;
        }
        vt = vt_setenv;
    } else {
        vt = VAR_ASSIGN;
    }
    for( c = *line; c != '\0' && !isspace( c ) && c != '='; c = *(++line) ) {
        *(name++) = c;
    }
    *name = '\0';
    while( isspace( *line ) )
        ++line;
    if( *line == '=' ) {
        ++line;
        while( isspace( *line ) )
            ++line;
        *value = line;
    } else if( vt == vt_setenv ) {
        *value = NULL;
        vt = VAR_ERROR;
    } else {
        *value = line;
        vt = VAR_CMD;
    }
    return( vt );
}
#endif

static var_type getEnvironVarType( const char *new_var )
/******************************************************/
{
    var_type        vt;

    vt = VAR_SETENV_ASSIGN;
#ifndef __UNIX__
#ifndef __OS2__
    if( GetVariableIntVal( "IsOS2DosBox" ) == 1 ) {
#endif
        // OS/2
        if( stricmp( new_var, "LIBPATH" ) == 0 ) {
            vt = VAR_ASSIGN;
        }
#ifndef __OS2__
    } else {
        // DOS, WINDOWS, NT
        if( stricmp( new_var, "PATH" ) == 0 ) {
            vt = VAR_CMD;
        }
    }
#endif
#endif
    return( vt );
}

#if !defined( __UNIX__ )
static void CheckEnvironmentLine( char *line, int num_env, bool *found_env, bool uninstall )
/******************************************************************************************/
{
    int                 i;
    append_mode         append;
    char                *p;
    char                *env_var;
    const char          *new_var;
    char                env_val[MAXENVVAR];
    var_type            vt;
    bool                modified;

    vt = parse_line( line, env_val, &p, VAR_SETENV_ASSIGN );
    switch( vt ) {
    case VAR_SETENV_ASSIGN:
        break;
    case VAR_ASSIGN:
        if( memicmp( env_val, "LIBPATH", 7 ) == 0 )
            break;
        // fall down
    case VAR_CMD:
        if( memicmp( env_val, "PATH", 4 ) == 0 )
            break;
        return;
    default:
        return;
    }
    env_var = StrNDup( env_val, strlen( env_val ) );
    strcpy( env_val, p );

    modified = false;
    for( i = 0; i < num_env; ++i ) {
        if( found_env[i] || !uninstall && !SimCheckEnvironmentCondition( i ) )
            continue;
        append = SimGetEnvironmentStrings( i, &new_var, new_val, sizeof( new_val ) );
        if( stricmp( env_var, new_var ) == 0 ) {
            // found an environment variable, replace its value
            found_env[i] = true;
            modify_value( env_val, new_val, append, uninstall );
            modified = true;
        }
    }
    if( modified ) {
        output_line( line, vt, env_var, env_val );
    }
    GUIMemFree( env_var );
}
#endif

static void FinishEnvironmentLines( FILE *fp, char *line, int num_env, bool *found_env, bool batch )
/**************************************************************************************************/
{
    int                 i;
    int                 j;
    append_mode         append;
    const char          *new_var;
    const char          *cur_var;
    char                env_val[MAXENVVAR];
    bool                libpath_batch;
    char                *val_before;
    char                *val_after;

    for( i = 0; i < num_env; ++i ) {
        if( found_env[i] || !SimCheckEnvironmentCondition( i ) )
            continue;
        append = SimGetEnvironmentStrings( i, &new_var, new_val, sizeof( new_val ) );
        libpath_batch = false;
        val_before = NULL;
        val_after = NULL;
        if( batch ) {
            if( stricmp( new_var, "LIBPATH" ) == 0 ) {
                libpath_batch = true;
                strcpy( line, "SET BEGINLIBPATH=" );
                val_before = line + 17;
                strcpy( env_val, "SET ENDLIBPATH=" );
                val_after = env_val + 15;
                if( append == AM_AFTER ) {
                    strcpy( val_after, new_val );
                } else {
                    strcpy( val_before, new_val );
                }
            } else if( append == AM_AFTER ) {
                sprintf( env_val, ENV_NAME "%c%s", new_var, PATH_LIST_SEP, new_val );
            } else if( append == AM_BEFORE ) {
                sprintf( env_val, "%s%c" ENV_NAME, new_val, PATH_LIST_SEP, new_var );
            } else {
                strcpy( env_val, new_val );
            }
        } else {
            strcpy( env_val, new_val );
        }
        for( j = i + 1; j < num_env; ++j ) {
            if( found_env[j] || !SimCheckEnvironmentCondition( j ) )
                continue;
            append = SimGetEnvironmentStrings( j, &cur_var, new_val, sizeof( new_val ) );
            if( stricmp( cur_var, new_var ) == 0 ) {
                found_env[j] = true;
                if( libpath_batch ) {
                    modify_value_libpath( val_before, val_after, new_val, append );
                } else {
                    modify_value( env_val, new_val, append, false );
                }
            }
        }
        if( libpath_batch ) {
            if( val_before[0] != '\0' ) {
                fputs( line, fp );
                fputc( '\n', fp );
            }
            if( val_after[0] != '\0' ) {
                fputs( env_val, fp );
                fputc( '\n', fp );
            }
        } else {
            output_line( line, getEnvironVarType( new_var ), new_var, env_val );
            if( line[0] != '\0' ) {
                fputs( line, fp );
                fputc( '\n', fp );
            }
        }
    }
}


#if !defined( __UNIX__ )

static bool ModFile( char *orig, char *new,
                     void (*func_xxx)( char *, int, bool *, bool ),
                     void (*finish_xxx)( FILE *, char *, int, bool *, bool ),
                     int num_xxx, int num_env, bool uninstall )
/*****************************************************************/
{
    FILE                *fp1, *fp2;
    char                *line;
    bool                *found_xxx = NULL;
    bool                *found_env = NULL;
    char                envbuf[MAXENVVAR + 1];

    fp1 = fopen( orig, "rt" );
    if( fp1 == NULL ) {
        MsgBox( NULL, "IDS_ERROR_OPENING", GUI_OK, orig );
        return( false );
    }
    fp2 = fopen( new, "wt" );
    if( fp2 == NULL ) {
        MsgBox( NULL, "IDS_ERROR_OPENING", GUI_OK, new );
        fclose( fp1 );
        return( false );
    }
    // allocate array to remember variables
    if( num_xxx > 0 ) {
        found_xxx = GUIMemAlloc( num_xxx * sizeof( bool ) );
        if( found_xxx == NULL ) {
            return( false );
        }
        memset( found_xxx, false, num_xxx * sizeof( bool ) );
    }
    if( num_env > 0 ) {
        found_env = GUIMemAlloc( num_env * sizeof( bool ) );
        if( found_env == NULL ) {
            GUIMemFree( found_xxx );
            return( false );
        }
        memset( found_env, false, num_env * sizeof( bool ) );
    }
    while( fgets( envbuf, MAXENVVAR, fp1 ) != NULL ) {
        line = strchr( envbuf, '\n' );
        if( line != NULL ) {
            *line = '\0';
        }
        // don't process empty lines but keep them in new file
        for( line = envbuf; isspace( *line ); ++line );
        if( line[0] != '\0' ) {
            func_xxx( line, num_xxx, found_xxx, uninstall );
            if( num_env > 0 ) {
                CheckEnvironmentLine( line, num_env, found_env, uninstall );
            }
            if( line[0] == '\0' ) {
                // skip removed lines
                continue;
            }
        }
        strcat( envbuf, "\n" );
        if( fputs( envbuf, fp2 ) < 0 ) {
            MsgBox( NULL, "IDS_ERROR_WRITING", GUI_OK, new );
            return( false );
        }
    }
    fclose( fp1 );
    if( !uninstall ) {
        // handle any remaining variables
        finish_xxx( fp2, envbuf, num_xxx, found_xxx, false );
        FinishEnvironmentLines( fp2, envbuf, num_env, found_env, false );
    }
    if( num_xxx > 0 ) {
        GUIMemFree( found_xxx );
    }
    if( num_env > 0 ) {
        GUIMemFree( found_env );
    }
    if( fclose( fp2 ) != 0 ) {
        MsgBox( NULL, "IDS_ERROR_CLOSING", GUI_OK, new );
        return( false );
    }
    return( true );
}


#ifndef __OS2__

static var_type getAutoVarType( const char *new_var )
/***************************************************/
{
    var_type        vt;

    if( memicmp( new_var, SETENV, SETENV_LEN ) == 0 ) {
        vt = VAR_ASSIGN_SETENV;
    } else {
        vt = VAR_CMD;
    }
    return( vt );
}

static void CheckAutoLine( char *line, int num_auto, bool *found_auto, bool uninstall )
/*************************************************************************************/
{
    int                 i;
    append_mode         append;
    char                *p;
    char                *env_var;
    char                env_val[MAXENVVAR];
    char                fname[_MAX_FNAME];
    char                fext[_MAX_EXT];
    const char          *new_var;
    var_type            vt;
    bool                modified;

    vt = parse_line( line, env_val, &p, VAR_ASSIGN_SETENV );
    switch( vt ) {
    case VAR_ASSIGN_SETENV:
        break;
    case VAR_CMD:
        if( memicmp( env_val, "PATH", 4 ) == 0 )
            return;
        if( !uninstall ) {
            _splitpath( env_val, NULL, NULL, fname, fext );
            if( stricmp( fname, "win" ) == 0
              && ( stricmp( fext, ".com" ) == 0 || fext[0] == '\0' ) ) {
                WinDotCom = StrNDup( line, strlen( line ) );
                line[0] = '\0';
                return;
            }
        }
        break;
    default:
        return;
    }
    env_var = StrNDup( env_val, strlen( env_val ) );
    strcpy( env_val, p );

    modified = false;
    for( i = 0; i < num_auto; ++i ) {
        if( found_auto[i] || !uninstall && !SimCheckAutoExecCondition( i ) )
            continue;
        append = SimGetAutoExecStrings( i, &new_var, new_val, sizeof( new_val ) );
        if( stricmp( env_var, new_var ) == 0 ) {
            // found an command, replace its value
            found_auto[i] = true;
            modify_value( env_val, new_val, append, uninstall );
            modified = true;
        }
    }
    if( modified ) {
        output_line( line, vt, env_var, env_val );
    }
    GUIMemFree( env_var );
}

static void FinishAutoLines( FILE *fp, char *line, int num_auto, bool *found_auto, bool batch )
/*********************************************************************************************/
{
    int                 i;
    int                 j;
    append_mode         append;
    const char          *new_var;
    const char          *cur_var;
    char                env_val[MAXENVVAR];

    batch=batch;
    for( i = 0; i < num_auto; ++i ) {
        if( found_auto[i] || !SimCheckAutoExecCondition( i ) )
            continue;
        append = SimGetAutoExecStrings( i, &new_var, new_val, sizeof( new_val ) );
        strcpy( env_val, new_val );
        for( j = i + 1; j < num_auto; ++j ) {
            if( found_auto[j] || !SimCheckAutoExecCondition( j ) )
                continue;
            append = SimGetAutoExecStrings( j, &cur_var, new_val, sizeof( new_val ) );
            if( stricmp( cur_var, new_var ) == 0 ) {
                found_auto[j] = true;
                modify_value( env_val, new_val, append, false );
            }
        }
        output_line( line, getAutoVarType( new_var ), new_var, env_val );
        if( line[0] != '\0' ) {
            fputs( line, fp );
            fputc( '\n', fp );
        }
    }
    if( WinDotCom != NULL ) {
        fputs( WinDotCom, fp );
        fputc( '\n', fp );
        GUIMemFree( WinDotCom );
        WinDotCom = NULL;
    }
}


static bool ModAuto( char *orig, char *new, bool uninstall )
/**********************************************************/
{
    int         num_auto;
    int         num_env;
    bool        rc;
#if defined( __DOS__ ) || defined( __WINDOWS__ )
    int         isOS2DosBox;
#endif

    num_auto = SimNumAutoExec();
    num_env = SimNumEnvironment();
#if defined( __DOS__ ) || defined( __WINDOWS__ )
    isOS2DosBox = GetVariableIntVal( "IsOS2DosBox" );
    SetVariableByName( "IsOS2DosBox", "0" );
#endif
    rc = ModFile( orig, new, CheckAutoLine, FinishAutoLines, num_auto, num_env, uninstall );
#if defined( __DOS__ ) || defined( __WINDOWS__ )
    SetVariableByName( "IsOS2DosBox", ( isOS2DosBox ) ? "1" : "0" );
#endif
    return( rc );
}

#endif

static var_type getConfigVarType( const char *new_var )
/*****************************************************/
{
    var_type        vt;

    if( memicmp( new_var, SETENV, SETENV_LEN ) == 0 ) {
        vt = VAR_ASSIGN_SETENV;
    } else {
        vt = VAR_ASSIGN;
    }
    return( vt );
}

static void CheckConfigLine( char *line, int num_cfg, bool *found_cfg, bool uninstall )
/*************************************************************************************/
{
    int                 i;
    append_mode         append;
    char                *p;
    char                *cfg_var;
    char                cfg_val[MAXENVVAR];
    var_type            vt;
    const char          *new_var;
    bool                run_find;
    bool                run_found;
    bool                modified;

    vt = parse_line( line, cfg_val, &p, VAR_ASSIGN_SETENV );
    switch( vt ) {
    case VAR_ASSIGN_SETENV:
    case VAR_ASSIGN:
        break;
    default:
        return;
    }
    cfg_var = StrNDup( cfg_val, strlen( cfg_val ) );
    strcpy( cfg_val, p );

    run_find = ( stricmp( cfg_var, "RUN" ) == 0 );
    modified = false;
    run_found = false;
    for( i = 0; i < num_cfg; ++i ) {
        if( found_cfg[i] || !uninstall && !SimCheckConfigCondition( i ) )
            continue;
        append = SimGetConfigStrings( i, &new_var, new_val, sizeof( new_val ) );
        if( stricmp( cfg_var, new_var ) == 0 ) {
            // found an variable
            if( run_find ) {
                // found RUN variable
                if( memicmp( cfg_val, new_val, strlen( new_val ) ) == 0 ) {
                    // if already there, just mark it as found
                    found_cfg[i] = true;
                    run_found = true;
                }
                continue;
            }
            if( isdigit( *cfg_val ) ) { // for files=20, linefers=30 etc
                if( uninstall || atoi( new_val ) <= atoi( cfg_val ) ) {
                    found_cfg[i] = true;
                    continue;
                }
            }
            found_cfg[i] = true;
            // replace its value
            modify_value( cfg_val, new_val, append, uninstall );
            modified = true;
        }
    }
    if( run_found && uninstall ) {
        output_line( line, vt, "REM RUN", cfg_val );
    } else if( modified ) {
        output_line( line, vt, cfg_var, cfg_val );
    }
    GUIMemFree( cfg_var );
}

static void FinishConfigLines( FILE *fp, char *line, int num_cfg, bool *found_cfg, bool batch )
/*********************************************************************************************/
{
    int                 i;
    int                 j;
    append_mode         append;
    const char          *new_var;
    const char          *cur_var;
    char                env_val[MAXENVVAR];

    batch=batch;
    for( i = 0; i < num_cfg; ++i ) {
        if( found_cfg[i] || !SimCheckConfigCondition( i ) )
            continue;
        append = SimGetConfigStrings( i, &new_var, new_val, sizeof( new_val ) );
        strcpy( env_val, new_val );
        for( j = i + 1; j < num_cfg; ++j ) {
            if( found_cfg[j] || !SimCheckConfigCondition( j ) )
                continue;
            append = SimGetConfigStrings( j, &cur_var, new_val, sizeof( new_val ) );
            if( stricmp( cur_var, new_var ) == 0 ) {
                found_cfg[j] = true;
                modify_value( env_val, new_val, append, false );
            }
        }
        output_line( line, getConfigVarType( new_var ), new_var, env_val );
        if( line[0] != '\0' ) {
            fputs( line, fp );
            fputc( '\n', fp );
        }
    }
}

static bool ModConfig( char *orig, char *new, bool uninstall )
/************************************************************/
{
    int         num_cfg;
    int         num_env;

    num_cfg = SimNumConfig();
#ifndef __OS2__
    if( GetVariableIntVal( "IsOS2DosBox" ) == 1 ) {
#endif
        num_env = SimNumEnvironment();
#ifndef __OS2__
    } else {
        num_env = 0;
    }
#endif
    if( num_cfg == 0 && num_env == 0 ) {
         return( true );
    }
    return( ModFile( orig, new, CheckConfigLine, FinishConfigLines, num_cfg, num_env, uninstall ) );
}

static void ReplaceExt( char *filename, char *new_ext )
/*****************************************************/
{
    char                drive[_MAX_DRIVE];
    char                dir[_MAX_DIR];
    char                fname[_MAX_FNAME];

    _splitpath( filename, drive, dir, fname, NULL );
    _makepath( filename, drive, dir, fname, new_ext );
}


static void BackupName( char *filename )
/**************************************/
{
    char        num_buf[5];
    int         num;

    for( num = 0; num < 999; num++ ) {
        sprintf( num_buf, "%3.3d", num );
        ReplaceExt( filename, num_buf );
        if( access( filename, F_OK ) != 0 ) {
            break;
        }
    }
}

bool ModifyAutoExec( bool uninstall )
/***********************************/
{
    int                 num_auto;
    int                 num_cfg;
    int                 num_env;
    char                boot_drive;
    int                 mod_type;
#ifndef __OS2__
    char                newauto[_MAX_PATH];
#endif
    char                newcfg[_MAX_PATH];
    FILE                *fp;

    num_auto = SimNumAutoExec();
    num_cfg = SimNumConfig();
    num_env = SimNumEnvironment();
    if( num_auto == 0 && num_cfg == 0 && num_env == 0 ) {
        return( true );
    }
#ifdef __OS2__
    SetVariableByName( "AutoText", GetVariableStrVal( "IDS_MODIFY_CONFIG" ) );
#else
    SetVariableByName( "AutoText", GetVariableStrVal( "IDS_MODIFY_AUTOEXEC" ) );
#endif
    if( DoDialog( "Modify" ) == DLG_CAN ) {
        return( false );
    }
    if( GetVariableIntVal( "ModNow" ) == 1 ) {
        mod_type = MOD_IN_PLACE;
    } else {
        mod_type = MOD_LATER;
    }

    if( GetVariableIntVal( "IsWin95" ) != 0 ) {
        boot_drive = 0;
    } else {
        boot_drive = GetBootDrive();
    }
    if( boot_drive == 0 ) {
#ifdef __NT__
        if( GetDriveType( "C:\\" ) == DRIVE_FIXED ) {
            OrigAutoExec[0] = 'C';   // assume C if it is a hard drive
        } else {
            // otherwise guess it is the same as the windows system directory
            const char  *sys_drv;
            sys_drv = GetVariableStrVal( "WinSystemDir" );
            boot_drive = toupper( sys_drv[0] ) - 'A' + 1;
            OrigAutoExec[0] = 'A' + boot_drive - 1;
        }
#else
        OrigAutoExec[0] = 'C';   // assume C
#endif
    } else {
        OrigAutoExec[0] = 'A' + boot_drive - 1;
    }
    OrigConfig[0] = OrigAutoExec[0];

    SetVariableByName( "FileToFind", "CONFIG.SYS" );
    while( access( OrigConfig, F_OK ) != 0 ) {
        SetVariableByName( "CfgDir", OrigConfig );
        if( DoDialog( "LocCfg" ) == DLG_CAN ) {
            MsgBox( NULL, "IDS_CANTFINDCONFIGSYS", GUI_OK );
            return( false );
        }
        strcpy( newcfg, GetVariableStrVal("CfgDir") );
        OrigConfig[0] = newcfg[0];
        OrigAutoExec[0] = OrigConfig[0];
        if( GetVariableIntVal( "CfgFileCreate" ) != 0 ) {
            fp = fopen( OrigConfig, "wt" );
            if( fp == NULL ) {
                MsgBox( NULL, "IDS_CANTCREATEFILE", GUI_OK, OrigConfig );
            } else {
                fclose( fp );
            }
        }
    }
#ifndef __OS2__
    SetVariableByName( "FileToFind", "AUTOEXEC.BAT" );
    while( access( OrigAutoExec, F_OK ) != 0 ) {
        SetVariableByName( "CfgDir", OrigAutoExec );
        if( DoDialog( "LocCfg" ) == DLG_CAN ) {
            MsgBox( NULL, "IDS_CANTFINDAUTOEXEC", GUI_OK );
            return( false );
        }
        strcpy( newcfg, GetVariableStrVal("CfgDir") );
        OrigAutoExec[0] = newcfg[0];
        if( GetVariableIntVal( "CfgFileCreate" ) != 0 ) {
            fp = fopen( OrigAutoExec, "wt" );
            if( fp == NULL ) {
                MsgBox( NULL, "IDS_CANTCREATEFILE", GUI_OK, OrigAutoExec );
            } else {
                fclose( fp );
            }
        }
    }
#endif

    if( mod_type == MOD_IN_PLACE ) {
        // copy current files to AUTOEXEC.BAK and CONFIG.BAK

#ifndef __OS2__
        strcpy( newauto, OrigAutoExec );
        BackupName( newauto );
#endif
        strcpy( newcfg, OrigConfig );
        BackupName( newcfg );

#ifdef __OS2__
        MsgBox( NULL, "IDS_COPYCONFIGSYS", GUI_OK, newcfg );
#else
        MsgBox( NULL, "IDS_COPYAUTOEXEC", GUI_OK, newauto, newcfg );
#endif

#ifndef __OS2__
        if( DoCopyFile( OrigAutoExec, newauto, false ) != CFE_NOERROR ) {
            MsgBox( NULL, "IDS_ERRORBACKAUTO", GUI_OK );
        } else {
            if( !ModAuto( newauto, OrigAutoExec, uninstall ) ) {
                return( false );
            }
        }
        if( DoCopyFile( OrigConfig, newcfg, false ) != CFE_NOERROR ) {
            MsgBox( NULL, "IDS_ERRORBACKCONFIG", GUI_OK );
        } else {
            if( !ModConfig( newcfg, OrigConfig, uninstall ) ) {
                return( false );
            }
        }
#else
        if( DoCopyFile( OrigConfig, newcfg, false ) != CFE_NOERROR ) {
            MsgBox( NULL, "IDS_ERRORBACKCONFIG", GUI_OK );
        } else {
            if( !ModConfig( newcfg, OrigConfig, uninstall ) ) {
                return( false );
            }
            MsgBox( NULL, "IDS_OS2CONFIGSYS", GUI_OK );
        }
#endif
        // indicate config files were modified if and only if we got this far
        ConfigModified = true;
    } else {
        // place modifications in AUTOEXEC.NEW and CONFIG.NEW
#ifndef __OS2__
        GetOldConfigFileDir( newauto, OrigAutoExec, uninstall );
        strcat( newauto, &OrigAutoExec[2] );
#if defined(__NT__)
        ReplaceExt( newauto, "W95" );
#else
        ReplaceExt( newauto, "DOS" );
#endif
#endif
        GetOldConfigFileDir( newcfg, OrigConfig, uninstall );
        strcat( newcfg, &OrigConfig[2] );
#if defined( __OS2__ )
        ReplaceExt( newcfg, "OS2" );
#elif defined( __NT__ )
        ReplaceExt( newcfg, "W95" );
#else
        ReplaceExt( newcfg, "DOS" );
#endif

#ifdef __OS2__
        MsgBox( NULL, "IDS_NEWCONFIGSYS", GUI_OK, newcfg );
        if( !ModConfig( OrigConfig, newcfg, uninstall ) ) {
            return( false );
        }
#else
        MsgBox( NULL, "IDS_NEWAUTOEXEC", GUI_OK, newauto, newcfg );
        if( !ModAuto( OrigAutoExec, newauto, uninstall ) ) {
            return( false );
        }
        if( !ModConfig( OrigConfig, newcfg, uninstall ) ) {
            return( false );
        }
#endif
    }
    return( true );
}

#endif   // !__UNIX__

char *ReplaceVars( char *buff, size_t buff_len, const char *src )
/***************************************************************/
//  Replace occurrences of %variable% in src with the destination directory,
//  and place the result in buffer.
{
    char                *quest;
    char                *p;
    char                *e;
    char                varname[MAXBUF];
    const char          *varval;
    char                *colon;
    size_t              len;

    len = strlen( src );
    --buff_len;     // reserve place for NULL character
    strncpy( buff, src, buff_len );
    buff[buff_len] = '\0';
    p = buff;
    while( *p != '\0' ) {
        if( *p++ != '%' )
            continue;
        if( *p == '%' ) {
            memmove( p - 1, p, strlen( p ) + 1 );
            continue;
        }
        e = strchr( p, '%' );
        if( e == NULL )
            break;
        len = e - p;
        if( len >= sizeof( varname ) )
            len = sizeof( varname ) - 1;
        memcpy( varname, p, len );
        varname[len] = '\0';
        for( ;; ) {     // loop for multiple '?' operators
            quest = strchr( varname, '?' );
            if( quest != NULL ) {
                *quest++ = '\0';
            }
            if( stricmp( varname, "root" ) == 0 ) { // kludge?
                varval = GetVariableStrVal( "DstDir" );
            } else if( varname[0] == '@' ) {
                varval = getenv( &varname[1] );
            } else {
                varval = GetVariableStrVal( varname );
            }
            if( quest == NULL ) {
                break;  // no '?' operator
            } else {
                colon = strchr( quest, ':' );
                if( GetOptionVarValue( GetVariableByName( varname ), false ) != 0 ) {
                    *colon = '\0';
                    varval = GetVariableStrVal( quest );
                    break;
                } else {
                    strcpy( varname, colon + 1 );
                    continue;
                }
            }
        }
        --p;
        if( varval != NULL ) {
            len = strlen( varval );
            memmove( p + len, e + 1, strlen( e + 1 ) + 1 );
            memcpy( p, varval, len );
        } else {
            memmove( p, e + 1, strlen( e + 1 ) + 1 );
        }
    }
    return( buff );
}

//***************************************************************************

/* place additional paths to search in here
   NOTE:  trailing \ is necessary as is final NULL
*/

static char *AdditionalPaths[] = { "drive:\\directory\\",
                                   /* insert paths here */
                                   NULL };

static void secondarysearch( char *filename, char *buffer )
/*********************************************************/
{
    char                drive[_MAX_DRIVE];
    char                dir[_MAX_DIR];
    char                ext[_MAX_EXT];
    char                path[_MAX_PATH];
    char                name[_MAX_PATH];
    unsigned int        counter;

    strcpy( buffer, "" );
    _splitpath( filename, NULL, NULL, name, ext );
    for( counter = 0; AdditionalPaths[counter]; counter++ ) {
        _splitpath( AdditionalPaths[counter],
                    drive,
                    dir,
                    NULL,
                    NULL );
        _makepath( path, drive, dir, name, ext );
        if( access( path, F_OK ) == 0 ) {
            strcpy( buffer, path );
            break;
        }
    }
}

static void VersionStr( int fp, char *ver, int verlen, char *verbuf )
/*******************************************************************/
{
    #define BUFF_SIZE   2048
    static char         Buffer[BUFF_SIZE];
    int                 len, size;
    char                *p;

    verbuf[0] = '\0';
    for( ;; ) {
        len = read( fp, Buffer, BUFF_SIZE );
        if( len < BUFF_SIZE ) {
            size = len;
            memset( Buffer + size, 0, BUFF_SIZE - size );
        } else {
            size = BUFF_SIZE - 256;
        }
        for( p = Buffer; p < Buffer + size; ++p ) {
            if( memcmp( p, ver, verlen ) == 0 ) {
                p += verlen;
                strcpy( verbuf, p );
                return;
            }
        }
        if( len < BUFF_SIZE ) break;    // eof
        if( lseek( fp, -256L, SEEK_CUR ) == -1 ) break;
    }
}


static void CheckVersion( char *path, char *drive, char *dir )
/************************************************************/
{
    int                 fp, hours;
    size_t              len;
    char                am_pm, buf[100];
    int                 check;
    struct stat         statbuf;
    struct tm           *timeptr;

    fp = open( path, O_RDONLY | O_BINARY );
    if( fp == -1 ) {
        return;     // shouldn't happen
    }

    // concat date and time to end of path
    check = fstat( fp, &statbuf );
    if( check == -1 ) {
        return;         // shouldn't happen
    }

    timeptr = gmtime( &(statbuf.st_mtime) );
    hours   = timeptr->tm_hour;
    if( hours <= 11 ) {
        am_pm = 'a';
        if( hours == 0 ) {
            hours += 12;
        }
    } else {
        am_pm = 'p';
        if( hours != 12 ) {
            hours -= 12;
        }
    }
    _splitpath( path, drive, dir, NULL, NULL );
    _makepath( path, drive, dir, NULL, NULL );
    len = strlen( path );
    sprintf( path + len, "  (%.2d-%.2d-%.4d %.2d:%.2d%cm)  ",
             timeptr->tm_mon + 1, timeptr->tm_mday, timeptr->tm_year + 1900,
             hours, timeptr->tm_min, am_pm );

    // also concat version number if it exists
    VersionStr( fp, "VeRsIoN=", 8, buf );
    if( buf[0] != '\0' ) {
        // Novell DLL
        strcat( path, buf );
    } else {
        lseek( fp, 0, SEEK_SET );
        VersionStr( fp, "FileVersion", 12, buf ); // includes '\0'
        if( buf[0] != '\0' ) {
            // Windows DLL
            strcat( path, buf );
        }
    }
    close( fp );
}

gui_message_return CheckInstallDLL( char *name, vhandle var_handle )
/******************************************************************/
{
    const char          *dst;
    size_t              dst_len;
    char                drive[_MAX_DRIVE];
    char                dir[_MAX_DIR];
    char                fname[_MAX_FNAME];
    char                ext[_MAX_EXT];
    char                unpacked_as[_MAX_PATH];
    char                dll_name[_MAX_FNAME + _MAX_EXT];
    char                path1[_MAX_PATH + 100];
    char                path2[_MAX_PATH + 100];
#if defined( __WINDOWS__ )
    OFSTRUCT            ofPrev;
    #define prev_path   ofPrev.szPathName
#else
    char                prev_path[_MAX_PATH];
#endif

    _splitpath( name, drive, dir, fname, ext );
    _makepath( dll_name, NULL, NULL, fname, ext );
#ifdef EXTRA_CAUTIOUS_FOR_DLLS
    _makepath( unpacked_as, drive, dir, fname, "._D_" );
#else
    _makepath( unpacked_as, drive, dir, fname, ext );
#endif
#if defined( __WINDOWS__ )
    if( OpenFile( dll_name, &ofPrev, OF_EXIST ) == -1 ) {
        secondarysearch( dll_name, prev_path );
        if( prev_path[0] == '\0' ) {
#ifdef EXTRA_CAUTIOUS_FOR_DLLS
            if( access( name, F_OK ) != 0 || remove( name ) == 0 ) {
                rename( unpacked_as, name );
            } else {
                remove( unpacked_as );
                if( MsgBox( NULL, "IDS_CANTREPLACE", GUI_YES_NO, name ) == GUI_RET_NO ) {
                    return( GUI_RET_CANCEL );
                }
            }
#endif
            return( GUI_RET_OK );     // did not previously exist
        }
    }
#else
    _searchenv( dll_name, "PATH", prev_path );
    if( prev_path[0] == '\0' ) {
        secondarysearch( dll_name, prev_path );
        if( prev_path[0] == '\0' ) {
#ifdef EXTRA_CAUTIOUS_FOR_DLLS
            if( access( name, F_OK ) != 0 || remove( name ) == 0 ) {
                rename( unpacked_as, name );
            } else {
                remove( unpacked_as );
                if( MsgBox( NULL, "IDS_CANTREPLACE", GUI_YES_NO, name ) == GUI_RET_NO ) {
                    return( GUI_RET_CANCEL );
                }
            }
#endif
            return( GUI_RET_OK );     // did not previously exist
        }
    }
#endif

    _splitpath( name, drive, dir, NULL, NULL );
    _makepath( path1, drive, dir, NULL, NULL );
    strupr( path1 );
    _splitpath( prev_path, drive, dir, NULL, NULL );
    _makepath( path2, drive, dir, NULL, NULL );
    strupr( path2 );
    dst = GetVariableStrVal( "DstDir" );
    dst_len = strlen( dst );
    if( memicmp( path1, dst, dst_len ) == 0 && memicmp( path2, dst, dst_len ) == 0 ) {
        /* both files are going into the main installation sub-tree */
#ifdef EXTRA_CAUTIOUS_FOR_DLLS
        if( access( name, F_OK ) != 0 || remove( name ) == 0 ) {
            rename( unpacked_as, name );
        } else {
            remove( unpacked_as );
            if( MsgBox( NULL, "IDS_CANTREPLACE", GUI_YES_NO, name ) == GUI_RET_NO ) {
                return( GUI_RET_CANCEL );
            }
        }
#endif
        return( GUI_RET_OK );
    }
    if( stricmp( path1, path2 ) == 0 ) {
#ifdef EXTRA_CAUTIOUS_FOR_DLLS
        /* both files are going into the same directory */
        struct stat         new, old;

        stat( prev_path, &old );
        stat( unpacked_as, &new );
        if( new.st_mtime < old.st_mtime ) {
            remove( unpacked_as );
        } else {
            if( access( name, F_OK ) != 0 || remove( name ) == 0 ) {
                rename( unpacked_as, name );
            } else {
                remove( unpacked_as );
                if( MsgBox( NULL, "IDS_CANTREPLACE", GUI_YES_NO, name ) == GUI_RET_NO ) {
                    return( GUI_RET_CANCEL );
                }
            }
        }
        return( GUI_RET_OK );
#else
        /* there is only one file & it's been zapped */
        return( GUI_RET_OK );
#endif
    }
    if( CheckForceDLLInstall( dll_name ) ) {
#ifdef EXTRA_CAUTIOUS_FOR_DLLS
        if( access( name, F_OK ) != 0 || remove( name ) == 0 ) {
            rename( unpacked_as, name );
        } else {
            remove( unpacked_as );
            if( MsgBox( NULL, "IDS_CANTREPLACE", GUI_YES_NO, name ) == GUI_RET_NO ) {
                return( GUI_RET_CANCEL );
            }
        }
#endif
        return( GUI_RET_OK );
    }

    strupr( dll_name );
    strcpy( path1, unpacked_as );
    strcpy( path2, prev_path );
    CheckVersion( path1, drive, dir );
    CheckVersion( path2, drive, dir );
    SetVariableByName( "FileDesc", dll_name );
    SetVariableByName( "DLLDir", path1 );
    SetVariableByName( "OtherDLLDir", path2 );

    // don't display the dialog if the user selected the "Skip dialog" option
    if( GetVariableIntVal( "DLL_Skip_Dialog" ) == 0 ) {
        if( DoDialog( "DLLInstall" ) == DLG_CAN ) {
            remove( unpacked_as );
            return( GUI_RET_CANCEL );
        }
    }

    if( GetVariableIntVal( "DLL_Delete_Old" ) == 1 ) {
        remove( prev_path );
#ifdef EXTRA_CAUTIOUS_FOR_DLLS
        if( access( name, F_OK ) != 0 || remove( name ) == 0 ) {
            rename( unpacked_as, name );
        } else {
            remove( unpacked_as );
            if( MsgBox( NULL, "IDS_CANTREPLACE", GUI_YES_NO, name ) == GUI_RET_NO ) {
                return( GUI_RET_CANCEL );
            }
        }
#endif
    } else if( GetVariableIntVal( "DLL_Keep_Both" ) == 1 ) {
#ifdef EXTRA_CAUTIOUS_FOR_DLLS
        if( access( name, F_OK ) != 0 || remove( name ) == 0 ) {
            rename( unpacked_as, name );
        } else {
            remove( unpacked_as );
            if( MsgBox( NULL, "IDS_CANTREPLACE", GUI_YES_NO, name ) == GUI_RET_NO ) {
                return( GUI_RET_CANCEL );
            }
        }
#endif
    } else if( GetVariableIntVal( "DLL_Replace_Old" ) == 1 ) {
        DoCopyFile( unpacked_as, prev_path, false );
        SetVariableByHandle( var_handle, prev_path );
        remove( unpacked_as );
    } else if( GetVariableIntVal( "DLL_Dont_Install" ) == 1 ) {
        SetVariableByHandle( var_handle, prev_path );
        remove( unpacked_as );
    } else if( GetVariableIntVal( "DLL_Abort_Install" ) == 1 ) {
        SetVariableByHandle( var_handle, prev_path );
        remove( unpacked_as );
        return( GUI_RET_CANCEL );
    }
    return( GUI_RET_OK );
}


//***************************************************************************

#if defined( __NT__ )

static bool ModEnv( int num_env, bool uninstall )
/***********************************************/
{
    bool                err = false;
    int                 i, j, k, rc;
    append_mode         append;
    DWORD               oldvar_len, oldval_len;
    DWORD               type, old_type;
    char                old_var[MAXBUF];
    char                old_val[MAXENVVAR];
    const char          *new_var;

    for( i = 0; i < num_env; i ++ ) {
        if( !uninstall && !SimCheckEnvironmentCondition( i ) )
            continue;

        append = SimGetEnvironmentStrings( i, &new_var, new_val, sizeof( new_val ) );
        for( j = CURRENT_USER; j < NUM_REG_LOCATIONS; j++ ) {
            if( !RegLocation[j].key_is_open || !RegLocation[j].modify ) {
                continue;
            }
            // Look for current definition
            // we need to do this all the time, since if we are uninstalling
            // we want to get rid of existing settings
            k = 0;
            do {
                oldvar_len = MAXENVVAR;
                oldval_len = MAXENVVAR;
                rc = RegEnumValue( RegLocation[j].key, k, old_var, &oldvar_len,
                                   NULL, NULL, (LPBYTE)old_val, &oldval_len );
                if( rc != 0 )
                    break;
                ++k;
            } while( stricmp( old_var, new_var ) != 0 );
            if( rc == ERROR_NO_MORE_ITEMS ) {
                // No existing value so add it
                if( uninstall ) {
                    rc = 0;
                } else {
                    rc = RegSetValueEx( RegLocation[j].key, new_var, 0, REG_SZ, (LPBYTE)new_val, (DWORD)( strlen( new_val ) + 1 ) );
                }
            } else if( rc == 0 ) {
                modify_value( old_val, new_val, append, uninstall );
                if( old_val[0] == '\0' ) {
                    rc = RegDeleteValue( RegLocation[j].key, new_var );
                } else {
                    rc = RegQueryValueEx( RegLocation[j].key, new_var, NULL, &old_type, NULL, NULL );
                    if( rc == 0 ) {
                        type = old_type;
                    } else {
                        type = REG_SZ;
                    }
                    rc = RegSetValueEx( RegLocation[j].key, new_var, 0, type, (LPBYTE)old_val, (DWORD)( strlen( old_val ) + 1 ) );
                }
            }
            if( rc != 0 ) {
                err = true;
            }
        }
    }
    return( !err );
}

bool ModifyConfiguration( bool uninstall )
/****************************************/
{
    int                 num_env;
    int                 mod_type;
    char                changes[_MAX_PATH];
    FILE                *fp;
    int                 i, j;
    bool                bRet;
    const char          *env_var;
    const char          *new_var;
    char                envbuf[MAXENVVAR + 1];
    bool                *found;
    append_mode         append;
    int                 rc;

    num_env = SimNumEnvironment();
    if( num_env == 0 ) {
        return( true );
    }

    rc = RegOpenKey( HKEY_CURRENT_USER, "Environment", &RegLocation[CURRENT_USER].key );
    RegLocation[CURRENT_USER].key_is_open = ( rc == 0 );

    rc = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
              "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
              0, KEY_ALL_ACCESS, &RegLocation[LOCAL_MACHINE].key );
    RegLocation[LOCAL_MACHINE].key_is_open = ( rc == 0 );

    if( RegLocation[LOCAL_MACHINE].key_is_open && !uninstall ) {
        if( DoDialog( "ModifyEnvironment" ) == DLG_CAN ) {
            return( false );
        }
    } else {
        // Note we use the same dialog as for AUTOEXEC changes
        // We set the Variable AUTOTEXT to contain the proper wording
        SetVariableByName( "AutoText", GetVariableStrVal( "IDS_MODIFY_ENVIRONMENT" ) );
        if( DoDialog( "Modify" ) == DLG_CAN ) {
            return( false );
        }
    }
    if( GetVariableIntVal( "ModLater" ) == 1 ) {
        mod_type = MOD_LATER;
    } else {
        mod_type = MOD_IN_PLACE;
        if( uninstall ) { //Clean up everywhere
            RegLocation[LOCAL_MACHINE].modify = true;
            RegLocation[CURRENT_USER].modify  = true;
        } else if( GetVariableIntVal( "ModMachine" ) == 1 ) {
            RegLocation[LOCAL_MACHINE].modify = true;
            RegLocation[CURRENT_USER].modify  = false;
        } else { // ModNow == 1 or ModUser == 1
            RegLocation[LOCAL_MACHINE].modify = false;
            RegLocation[CURRENT_USER].modify  = true;
        }
    }

    if( mod_type == MOD_IN_PLACE ) {
        bRet = ModEnv( num_env, uninstall );
        // indicate config files were modified if and only if we got this far
        ConfigModified = true;
    } else {  // handle MOD_LATER case
        found = GUIMemAlloc( num_env * sizeof( bool ) );
        memset( found, false, num_env * sizeof( bool ) );
        GetOldConfigFileDir( changes, GetVariableStrVal( "DstDir" ), uninstall );
        strcat( changes, "\\CHANGES.ENV" );
        MsgBox( NULL, "IDS_CHANGES", GUI_OK, changes );
        fp = fopen( changes, "wt" );
        if( fp != NULL ) {
            fprintf( fp, "%s\n\n", GetVariableStrVal( "IDS_ENV_CHANGES" ) );
            for( i = 0; i < num_env; i ++ ) {
                if( found[i] || !SimCheckEnvironmentCondition( i ) )
                    continue;

                append = SimGetEnvironmentStrings( i, &new_var, new_val, sizeof( new_val ) );
                if( append == AM_AFTER ) {
                    fprintf( fp, GetVariableStrVal( "IDS_ADD_TO_VAR" ), new_var );
                } else if( append == AM_BEFORE ) {
                    fprintf( fp, GetVariableStrVal( "IDS_ADD_TO_VAR" ), new_var );
                } else {
                    fprintf( fp, GetVariableStrVal( "IDS_SET_VAR_TO" ), new_var );
                }
                sprintf( envbuf, "%s", new_val );
                for( j = i + 1; j < num_env; ++j ) {
                    if( found[j] || !SimCheckEnvironmentCondition( j ) )
                        continue;
                    append = SimGetEnvironmentStrings( j, &env_var, new_val, sizeof( new_val ) );
                    if( stricmp( env_var, new_var ) == 0 ) {
                        found[j] = true;
                        modify_value( envbuf, new_val, append, uninstall );
                    }
                }
                fprintf( fp, "\n    %s\n", envbuf );
            }
            fclose( fp );
        }
        GUIMemFree( found );
        bRet = true;
    }

    if( RegLocation[CURRENT_USER].key_is_open ) {
        RegCloseKey( RegLocation[CURRENT_USER].key );
    }
    if( RegLocation[LOCAL_MACHINE].key_is_open ) {
        RegCloseKey( RegLocation[LOCAL_MACHINE].key );
    }

    return( bRet );
}

bool ModifyRegAssoc( bool uninstall )
/***********************************/
{
    HKEY    hkey;
    char    buff1[256];
    char    buff2[256];
    char    ext[16];
    char    keyname[256];
    char    program[256];
    int     num;
    int     i;

    if( !uninstall ) {
        if( DoDialog( "ModifyAssociations" ) == DLG_CAN ) {
            return( false );
        }
        if( GetVariableIntVal( "NoModEnv" ) == 1 ) {
            return( true );
        }
        num = SimNumAssociations();
        for( i = 0; i < num; i++ ) {
            if( !SimCheckAssociationCondition( i ) )
                continue;
            SimGetAssociationExt( i, ext );
            SimGetAssociationKeyName( i, keyname );
            sprintf( buff1, ".%s", ext );
            RegCreateKey( HKEY_CLASSES_ROOT, buff1, &hkey );
            RegSetValue( hkey, NULL, REG_SZ, keyname, (DWORD)strlen( keyname ) );
            RegCloseKey( hkey );
            RegCreateKey( HKEY_CLASSES_ROOT, keyname, &hkey );
            SimGetAssociationDescription( i, buff1 );
            RegSetValue( hkey, NULL, REG_SZ, buff1, (DWORD)strlen( buff1 ) );
            SimGetAssociationProgram( i, program );
            if( SimGetAssociationNoOpen( i ) != 1 ) {
                sprintf( buff1, "%s %%1", program );
                ReplaceVars( buff2, sizeof( buff2 ), buff1 );
                RegSetValue( hkey, "shell\\open\\command", REG_SZ, buff2, (DWORD)strlen( buff2 ) );
            }
            sprintf( buff1, "%s,%d", program, SimGetAssociationIconIndex( i ) );
            ReplaceVars( buff2, sizeof( buff2 ), buff1 );
            RegSetValue( hkey, "DefaultIcon", REG_SZ, buff2, (DWORD)strlen( buff2 ) );
            RegCloseKey( hkey );
        }
    }

    return( true );
}

bool AddToUninstallList( bool uninstall )
/***************************************/
{
    HKEY        hkey;
    const char  *val;
    DWORD       major, minor, dw;
    char        buf[256];

    sprintf( buf, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s",
             GetVariableStrVal( "UninstallKeyName" ) );
    if( !uninstall ) {
        RegCreateKey( HKEY_LOCAL_MACHINE, buf, &hkey );
        val = GetVariableStrVal( "UninstallDisplayName" );
        RegSetValueEx( hkey, "DisplayName", 0L, REG_SZ, (LPBYTE)val, (DWORD)( strlen( val ) + 1 ) );
        ReplaceVars( buf, sizeof( buf ), GetVariableStrVal( "UninstallCommand" ) );
        RegSetValueEx( hkey, "UninstallString", 0L, REG_SZ, (LPBYTE)buf, (DWORD)( strlen( buf ) + 1 ) );
        ReplaceVars( buf, sizeof( buf ), GetVariableStrVal( "UninstallIcon" ) );
        RegSetValueEx( hkey, "DisplayIcon", 0L, REG_SZ, (LPBYTE)buf, (DWORD)( strlen( buf ) + 1 ) );
        val = GetVariableStrVal( "UninstallCompany" );
        RegSetValueEx( hkey, "Publisher", 0L, REG_SZ, (LPBYTE)val, (DWORD)( strlen( val ) + 1 ) );
        val = GetVariableStrVal( "UninstallHelpURL" );
        RegSetValueEx( hkey, "HelpLink", 0L, REG_SZ, (LPBYTE)val, (DWORD)( strlen( val ) + 1 ) );
        major = GetVariableIntVal( "UninstallMajorVersion" );
        RegSetValueEx( hkey, "VersionMajor", 0L, REG_DWORD, (LPBYTE)&major, (DWORD)( sizeof( DWORD ) ) );
        minor = GetVariableIntVal( "UninstallMinorVersion" );
        RegSetValueEx( hkey, "VersionMinor", 0L, REG_DWORD, (LPBYTE)&minor, (DWORD)( sizeof( DWORD ) ) );
        sprintf( buf, "%d.%d", major, minor );
        RegSetValueEx( hkey, "DisplayVersion", 0L, REG_SZ, (LPBYTE)buf, (DWORD)( strlen( buf ) + 1 ) );
        val = GetVariableStrVal( "DstDir" );
        RegSetValueEx( hkey, "InstallLocation", 0L, REG_SZ, (LPBYTE)val, (DWORD)( strlen( val ) + 1 ) );
        dw = 1L;
        RegSetValueEx( hkey, "NoModify", 0L, REG_DWORD, (LPBYTE)&dw, (DWORD)( sizeof( DWORD ) ) );
        RegSetValueEx( hkey, "NoRepair", 0L, REG_DWORD, (LPBYTE)&dw, (DWORD)( sizeof( DWORD ) ) );
        RegCloseKey( hkey );
    } else {
        RegDeleteKey( HKEY_LOCAL_MACHINE, buf );
    }

    return( true );
}

#endif

bool GenerateBatchFile( bool uninstall )
/**************************************/
{
    int                 num_env;
    char                batch_file[_MAX_PATH];
    FILE                *fp;
    char                drive[_MAX_DRIVE];
    char                dir[_MAX_DIR];
    char                fname[_MAX_FNAME];
    char                ext[_MAX_EXT];
    bool                *found;
    char                buf[MAXENVVAR + 1];
#if defined( __DOS__ ) || defined( __WINDOWS__ )
    int                 isOS2DosBox;
#endif

    ReplaceVars( batch_file, sizeof( batch_file ), GetVariableStrVal( "BatchFileName" ) );
    _splitpath( batch_file, drive, dir, fname, ext );
    if( ext[0] == '\0' ) {
        strcpy( ext, BATCHEXT );
    }
    _makepath( batch_file, drive, dir, fname, ext );
    if( uninstall ) {
        remove( batch_file );
    } else {
        fp = fopen( batch_file, "wt" );
        if( fp != NULL ) {
#ifdef __UNIX__
            fprintf( fp, "#!/bin/sh\n" );
#else
            fprintf( fp, "@echo off\n" );
#endif
            fprintf( fp, "echo %s\n", GetVariableStrVal( "BatchFileCaption" ) );
#if defined( __DOS__ ) || defined( __WINDOWS__ )
            isOS2DosBox = GetVariableIntVal( "IsOS2DosBox" );
            SetVariableByName( "IsOS2DosBox", "0" );
#endif
            num_env = SimNumEnvironment();
            if( num_env > 0 ) {
                found = GUIMemAlloc( num_env * sizeof( bool ) );
                memset( found, false, num_env * sizeof( bool ) );
                FinishEnvironmentLines( fp, buf, num_env, found, true );
                GUIMemFree( found );
            }
#if defined( __DOS__ ) || defined( __WINDOWS__ )
            SetVariableByName( "IsOS2DosBox", ( isOS2DosBox ) ? "1" : "0" );
#endif
            fclose( fp );
        }
    }
    return( true );
}
