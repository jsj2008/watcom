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
* Description:  Utility routines for the command line parser.
*
****************************************************************************/


#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#if !defined( __UNIX__ )
#include <direct.h>
#endif
#include "wio.h"
#include "walloca.h"
#include "linkstd.h"
#include "loadfile.h"
#include "command.h"
#include "alloc.h"
#include "msg.h"
#include "wlnkmsg.h"
#include "fileio.h"
#include "ideentry.h"
#include "cmdline.h"

#include "clibext.h"

#define _LinkerPrompt "WLINK>"

#define IS_WHITESPACE(ptr) (*(ptr) == ' ' || *(ptr) =='\t' || *(ptr) == '\r')

cmdfilelist     *CmdFile = NULL;

/* Default File Extension array, see ldefext.h */

static  char    *DefExt[] = {
    #define pick1(enum,text) text,
    #include "ldefext.h"
    #undef pick1
};

static bool     CheckFence( void );
static bool     MakeToken( tokcontrol, sep_type );
static void     GetNewLine( void );
static void     BackupParser( void );
static void     StartNewFile( void );

static bool WildCard( bool (*rtn)( void ), tokcontrol ctrl )
/**********************************************************/
{
#if defined( __UNIX__ ) || defined( __ZDOS__ )
    /* unused parameters */ (void)ctrl;

    //opendir - readdir wildcarding not supported here.
    return( rtn() );
#else
    char                *p;
    char                *start;
    DIR                 *dir;
    struct dirent       *dirent;
    char                drive[_MAX_DRIVE];
    char                directory[_MAX_DIR];
    char                name[_MAX_FNAME];
    char                extin[_MAX_EXT];
    char                pathin[_MAX_PATH];
    bool                wildcrd;
    bool                retval;

    wildcrd = false;
    if( ctrl & TOK_IS_FILENAME ) {
        for( p = Token.this; ; ++p ) {      // check if wildcard
            /* end of parm: NULLCHAR or blank */
            if( *p == '\'' )
                break;     // don't wildcard a quoted string.
            if( *p == '\0' )
                break;
            if( *p == ' ' )
                break;
            if( *p == '?' || *p == '*' ) {
                wildcrd = true;
                break;
            }
        }
    }
    if( !wildcrd ) {
        retval = rtn();
    } else {
        retval = true;
        /* expand file names */
        start = tostring();
        dir = opendir( start );
        if( dir != NULL ) {
            _splitpath( start, drive, directory, NULL, NULL );
            for( ;; ) {
                dirent = readdir( dir );
                if( dirent == NULL )
                    break;
                if( dirent->d_attr & (_A_HIDDEN | _A_SYSTEM | _A_VOLID | _A_SUBDIR) )
                    continue;
                _splitpath( dirent->d_name, NULL, NULL, name, extin );
                _makepath( pathin, drive, directory, name, extin );
                Token.this = pathin;            // dangerous?
                Token.len = strlen( pathin );
                if( !(*rtn)() ) {
                    Token.this = NULL;
                    Token.thumb = false;    // make _sure_ we don't use token.this
                    retval = false;
                    break;
                }
            }
            closedir( dir );
        } else {
            retval = rtn();
        }
        _LnkFree( start );
    }
    return( retval );
#endif
}

bool ProcArgList( bool (*rtn)( void ), tokcontrol ctrl )
{
    return( ProcArgListEx( rtn, ctrl ,NULL ) );
}

bool ProcArgListEx( bool (*rtn)( void ), tokcontrol ctrl, cmdfilelist *resetpoint )
/*********************************************************************************/
{
    bool bfilereset = false;    /* did we open a file and get reset ? */

    if( GetTokenEx( SEP_LCURLY, ctrl, resetpoint, &bfilereset ) ) {
        for( ;; ) {
            if( !WildCard( rtn, ctrl ) ) {
                return( false );
            }
            if( CheckFence() ) {
                break;
            } else if( !GetTokenEx( SEP_NO, ctrl ,resetpoint, &bfilereset ) ) {
                LnkMsg( LOC+LINE+ERR+MSG_BAD_CURLY_LIST, NULL );
                break;
            }
        }
    } else {
        if( resetpoint && bfilereset )
            return( true );
        if( !GetTokenEx( SEP_NO, ctrl, resetpoint, &bfilereset ) )
            return( false );
        do {
            if( resetpoint && bfilereset )
                return( true );
            if( !WildCard( rtn, ctrl ) ) {
                return( false );
            }
        } while( GetTokenEx( SEP_COMMA, ctrl, resetpoint, &bfilereset ) );
    }
    return( true );
}

bool ProcOne( parse_entry *entry, sep_type req, bool suicide )
/************************************************************/
/* recognize token out of parse table, with required separator            */
/* return false if no separator, Suicide if not recognized (if suicide is */
/* true) otherwise use return code from action routine in matching entry  */
{
    const char          *key;
    const char          *ptr;
    size_t              len;
    bool                ret;
    char                keybuff[20];

    if( !GetToken( req, TOK_INCLUDE_DOT ) )
        return( false );
    ret = true;
    for( ; entry->keyword != NULL; ++entry ) {
        key = entry->keyword;
        ptr = Token.this;
        len = Token.len;
        for( ;; ) {
            if( len == 0 && !isupper( *key ) ) {
                if( HintFormat( entry->format ) ) {
                    ret = (*entry->rtn)();
                    CmdFlags |= entry->flags;
                } else {
                    strcpy( keybuff, entry->keyword );
                    strlwr( keybuff );
                    LnkMsg( LOC+LINE+WRN+MSG_FORMAT_BAD_OPTION, "s", keybuff );
                }
                return( ret );
            }
            if( *key == '\0' || tolower( *ptr ) != tolower( *key ) )
                break;
            ptr++;
            key++;
            len--;
        }
        /* here if this is no match */
    }
    /* here if no match in table */
    if( suicide ) {
        Syntax();
    } else {
        Token.thumb = true;         /*  try again later */
        ret = false;
    }
    return( ret );
}

bool MatchOne( parse_entry *entry, sep_type req, const char *match, size_t match_len )
/************************************************************************************/
/* recognize token out of parse table */
{
    const char          *key;
    const char          *ptr;
    size_t              len;

    /* unused parameters */ (void)req;

    for( ; entry->keyword != NULL; ++entry ) {
        key = entry->keyword;
        ptr = match;
        len = match_len;
        for( ;; ) {
            if( len == 0 && !isupper( *key ) ) {
                return( true );
            }
            if( *key == '\0' || tolower( *ptr ) != tolower( *key ) )
                break;
            ptr++;
            key++;
            len--;
        }
        /* here if this is no match */
    }

    /* here if no match in table */
    return( false );
}

ord_state getatoi( unsigned_16 *pnt )
/***********************************/
{
    unsigned_32 value;
    ord_state   retval;

    retval = getatol( &value );
    if( retval == ST_IS_ORDINAL ) {
        if( value > 0xffff ) {
            return( ST_INVALID_ORDINAL );
        }
        *pnt = (unsigned_16)value;
    }
    return( retval );
}

ord_state getatol( unsigned_32 *pnt )
/***********************************/
{
    const char      *p;
    size_t          len;
    unsigned long   value;
    unsigned        radix;
    bool            isvalid;
    bool            isdig;
    bool            gotdigit;
    char            ch;

    len = Token.len;
    if( len == 0 )
        return( ST_NOT_ORDINAL );
    p = Token.this;
    gotdigit = false;
    value = 0ul;
    radix = 10;
    if( len >= 2 && *p == '0' ) {
        --len;
        ++p;
        if( tolower( *p ) == 'x' ) {
            radix = 16;
            ++p;
            --len;
        }
    }
    for( ; len != 0; --len ) {
        ch = (char)tolower( (unsigned char)*p++ );
        if( ch == 'k' ) {         // constant of the form 64k
            if( len > 1 || !gotdigit ) {
                return( ST_NOT_ORDINAL );
            } else {
                value <<= 10;        // value = value * 1024;
            }
        } else if( ch == 'm' ) {        // constant of the form 64M
            if( len > 1 || !gotdigit ) {
                return( ST_NOT_ORDINAL );
            } else {
                value <<= 20;
            }
        } else {
            isdig = ( isdigit( ch ) != 0 );
            if( radix == 10 ) {
                isvalid = isdig;
            } else {
                isvalid = ( isxdigit( ch ) != 0 );
            }
            if( !isvalid ) {
                return( ST_NOT_ORDINAL );
            }
            value *= radix;
            if( isdig ) {
                value += ch - '0';
            } else {
                value += ch - 'a' + 10;
            }
            gotdigit = true;
        }
    }
    *pnt = (unsigned_32)value;
    return( ST_IS_ORDINAL );
}

bool HaveEquals( tokcontrol ctrl )
/********************************/
{
    if( !GetToken( SEP_EQUALS, ctrl ) ) {
        Token.this = Token.next;
        /* collect the token that caused the problem */
        GetToken( SEP_NO, ctrl );
        return( false );
    }
    return( true );
}

bool GetLong( unsigned_32 *addr )
/*******************************/
{
    unsigned_32     value;
    ord_state       ok;

    if( !HaveEquals( TOK_NORMAL ) )
        return( false );
    ok = getatol( &value );
    if( ok != ST_IS_ORDINAL ) {
        return( false );
    } else {
        *addr = value;
    }
    return( true );
}

char *tostring( void )
/********************/
// make the current token into a C string.
{
    return( ChkToString( Token.this, Token.len ) );
}

char *totext( void )
/******************/
/* get a possiblly quoted string */
{
    Token.thumb = true;
    if( !GetToken( SEP_NO, TOK_NORMAL ) ) {
        GetToken( SEP_NO, TOK_INCLUDE_DOT );
    }
    return( tostring() );
}

static void ExpandEnvVariable( void )
/***********************************/
/* parse the specified environment variable & deal with it */
{
    char        *envname;
    const char  *env;
    char        *buff;
    size_t      envlen;

    Token.next++;
    if( !MakeToken( TOK_INCLUDE_DOT, SEP_PERCENT ) ) {
        LnkMsg( LOC+LINE+FTL+MSG_ENV_NAME_INCORRECT, NULL );
    }
    envname = tostring();
    env = GetEnvString( envname );
    if( env == NULL ) {
        LnkMsg( LOC+LINE+WRN+MSG_ENV_NOT_FOUND, "s", envname );
        _LnkFree( envname );
    } else {
        envlen = strlen( env );
        if( !IS_WHITESPACE( Token.next ) ) {
            MakeToken( TOK_INCLUDE_DOT, SEP_SPACE );
            _ChkAlloc( buff, envlen + Token.len + 1);
            memcpy( buff, env, envlen );
            memcpy( buff + envlen, Token.this, Token.len );
            buff[Token.len + envlen] = '\0';
        } else {
            buff = ChkToString( env, envlen );
        }
        NewCommandSource( envname, buff, ENVIRONMENT );
    }
}

static bool CheckFence( void )
/****************************/
/* check for a "fence", and skip it if it is there */
{
    if( Token.thumb ) {
        if( Token.quoted )
            return( false );   /* no fence inside quotes */
        if( *Token.this == '}' ) {
            Token.this++;
            return( true );
        }
    } else {
        return( GetToken( SEP_RCURLY, TOK_NORMAL ) );
    }
    return( false );
}

bool GetToken( sep_type req, tokcontrol ctrl )
{
    return( GetTokenEx( req, ctrl, NULL, NULL ) );
}

bool GetTokenEx( sep_type req, tokcontrol ctrl, cmdfilelist *resetpoint, bool *pbreset )
/**************************************************************************************/
/* return true if no problem */
/* return false if problem   */
{
    char    hmm;
    bool    ret;
    bool    need_sep;

    if( Token.thumb ) {
        Token.thumb = false;
        if( Token.quoted )
            return( true );
        Token.next = Token.this;        /* re-process last token */
    }
    need_sep = true;
    for( ;; ) {                         /* finite state machine */

        /*
        //  carl.young
        //  We had a situation where an input file (in this case a Novell
        //  import or export file) does not have the consistent format
        //  expected from this FSM code. If the skipToNext flag is set,
        //  then we just skip to the next token and return rather than
        //  reporting an error.
        //  For reference the import files looked like:
        //      (PREFIX)
        //          symbol1,
        //          symbol2,
        //          symbolnm1,
        //          symboln
        //
        //  Note the missing comma separator after the prefix token. The
        //  prefix token(s) may also appear anywhere in the file.
        */

        if( Token.skipToNext && (req == SEP_COMMA) ) {
            Token.skipToNext = false;
            need_sep = false;
        }

        switch( Token.where ) {
        case MIDST:
            EatWhite();
            hmm = *Token.next;
            switch( hmm ) {
            case CTRLZ:
                Token.where = ENDOFFILE;
                break;
            case '\0':
                if( Token.how == BUFFERED
                 || Token.how == ENVIRONMENT
                 || Token.how == SYSTEM ) {
                    Token.where = ENDOFFILE;
                    break;
                }
                /* fall through */
            case '\n':
                if( Token.how == BUFFERED
                 || Token.how == ENVIRONMENT
                 || Token.how == SYSTEM ) {
                    Token.next++;               // just skip this.
                } else if( Token.how == COMMANDLINE ) {
                    Token.where = ENDOFCMD;
                } else {
                    Token.where = ENDOFLINE;
                }
                Token.line++;
                break;
            case '@':
                if( req != SEP_SPACE ) {
                    Token.next++;
                    GetToken( SEP_NO, TOK_INCLUDE_DOT|TOK_IS_FILENAME );
                    StartNewFile();
                    break;
                }
                Token.next--;   /* make a token out of this */
                ret = MakeToken( ctrl, req );
                Token.quoted = false;
                return( ret );
            case '#':
                Token.where = ENDOFLINE;            /* begin comment */
                Token.line++;
                break;
            case '^':
                if( req != SEP_SPACE ) {    /* if not storing system blocks */
                    Token.next++;
                    BackupParser();
                    break;
                }
                Token.next--;   /* make a token out of this */
                ret = MakeToken( ctrl, req );
                Token.quoted = false;
                return( ret );
            case '%':
                if( req != SEP_SPACE ) {
                    ExpandEnvVariable();
                    break;
                }
                /* fall through */
            default:
                if( need_sep ) {
                    Token.quoted = false;
                    switch( req ) {
                    case SEP_NO:
                        if( hmm == ',' || hmm == '=' )
                            return( false );
                        break;
                    case SEP_COMMA:
                        if(hmm != ',' )
                            return( false);
                        Token.next++;
                        break;
                    case SEP_EQUALS:
                        if( hmm != '=' )
                            return( false );
                        Token.next++;
                        break;
                    case SEP_PERIOD:
                    case SEP_DOT_EXT:
                        if( hmm != '.' )
                            return( false );
                        Token.next++;
                        break;
                    case SEP_PAREN:
                        if( hmm != '(' )
                            return( false );
                        Token.next++;
                        break;
                    case SEP_LCURLY:
                        if( hmm != '{' )
                            return( false );
                        Token.next++;
                        break;
                    case SEP_QUOTE:
                        if( hmm != '\'' )
                            return( false );
                        Token.next++;
                        Token.quoted = true;
                        break;
                    case SEP_RCURLY:
                        if( hmm != '}' )
                            return( false );
                        Token.next++;
                        return( true );
                    case SEP_END:
                        return( false );
                    }
                    need_sep = false;
                    EatWhite();
                } else {                /*  must have good separator here */
                    if( hmm == '\'' && req != SEP_PAREN && req != SEP_SPACE ) {
                        req = SEP_QUOTE;   /* token has been quoted */
                        Token.next++;      /* don't include the quote */
                        Token.quoted = true;
                    }
                    ret = MakeToken( ctrl, req );
                    return( ret );
                }
                break;
            }
            break;
        case ENDOFLINE:
            GetNewLine();
            break;
        case ENDOFFILE:
            if( Token.locked )
                return( false );
            RestoreCmdLine();
            if( Token.thumb ) {
                Token.thumb = false;
                Token.next = Token.this;        /* re-process last token */
            }
            Token.quoted = false;
            if( resetpoint && (CmdFile == resetpoint) ) {
                if( *Token.next == ',' )
                    break;
                if( pbreset != NULL )
                    *pbreset = true;            /* Show we have hit a file end-point for a directive */
                return( false );
            }
            break;
        case ENDOFCMD:
            if( CmdFile->next != NULL ) {
                RestoreParser();
                break;
            }
            Token.quoted = false;
            ret = ( req == SEP_END );
            return( ret );
        }
    }
}

static void OutPutPrompt( const char *str )
/*****************************************/
{
    if( QIsDevice( CmdFile->file ) ) {
        WriteStdOut( str );
    }
}

static void GetNewLine( void )
/****************************/
{
    if( Token.how == BUFFERED || Token.how == ENVIRONMENT || Token.how == SYSTEM ) {
        Token.where = MIDST;
        //go until next line found;
        for( ; *Token.next != '\n'; Token.next++ ) {
            if( *Token.next == '\0' || *Token.next == CTRLZ ) {
                Token.where = ENDOFFILE;
                break;
            }
        }
        Token.next++;
    } else if( Token.how == NONBUFFERED ) {
        if( QReadStr( CmdFile->file, Token.buff, MAX_REC, CmdFile->name ) ) {
            Token.where = ENDOFFILE;
        } else {
            Token.where = MIDST;
        }
        Token.next = Token.buff;
    } else {               // interactive.
        OutPutPrompt( _LinkerPrompt );
        Token.how = INTERACTIVE;
        if( QReadStr( STDIN_HANDLE, Token.buff, MAX_REC, "console" ) ) {
            Token.where = ENDOFCMD;
        } else {
            Token.where = MIDST;
        }
        Token.next = Token.buff;
    }
}

static void BackupParser( void )
/******************************/
/* move the parser temporarily back to a previous input source */
{
    if( CmdFile->prev == NULL ) {
        LnkMsg( LOC+LINE+WRN + MSG_NO_PREVIOUS_INPUT, NULL );
        return;
    }
    memcpy( &CmdFile->token, &Token, sizeof( tok ) );   // save current state
    CmdFile = CmdFile->prev;
    memcpy( &Token, &CmdFile->token, sizeof( tok ) ); // restore old state.
}

void RestoreParser( void )
/************************/
/* return the parser to the previous command state */
{
    if( CmdFile->next == NULL )
        return;
    memcpy( &CmdFile->token, &Token, sizeof( tok ) );  /* save current state */
    CmdFile = CmdFile->next;
    memcpy( &Token, &CmdFile->token, sizeof( tok ) ); // restore old state.
}

void NewCommandSource( char *name, char *buff, method how )
/*********************************************************/
/* start reading from a new command source, and save the old one */
{
    cmdfilelist     *newfile;

    _ChkAlloc( newfile, sizeof( cmdfilelist ) );
    newfile->file = STDIN_HANDLE;
    if( CmdFile != NULL ) {     /* save current state */
        memcpy( &CmdFile->token, &Token, sizeof( tok ) );
        newfile->next = CmdFile->next;
        if( newfile->next != NULL ) {
            newfile->next->prev = newfile;
        }
    } else {
        newfile->next = NULL;
    }
    newfile->prev = CmdFile;
    if( newfile->prev != NULL ) {
        newfile->prev->next = newfile;
    }
    CmdFile = newfile;
    CmdFile->name = name;
    CmdFile->token.buff = buff;     /* make sure token is freed */
    CmdFile->token.how = how;       /* but only if it needs to be */
    Token.buff = buff;
    Token.next = Token.buff;
    Token.where = MIDST;
    Token.line = 1;
    Token.how = how;
    Token.thumb = false;
    Token.locked = false;
    Token.quoted = false;
}

void SetCommandFile( f_handle file, char *fname )
/***********************************************/
/* read input from given file */
{
    unsigned long   long_size;
    char            *buff;

    if( QIsDevice( file ) ) {
        long_size = 0x10000;
    } else {
        long_size = QFileSize( file );
    }
    buff = NULL;
    if( long_size < 0x10000 - 16 - 1 ) {       // if can alloc a chunk big enough
        size_t  size = (size_t)long_size;

        _LnkAlloc( buff, size + 1 );
        if( buff != NULL ) {
            size = QRead( file, buff, size, fname );
            buff[size] = '\0';
            NewCommandSource( fname, buff, BUFFERED );
        }
    }
    if( buff == NULL ) {  // if couldn't buffer for some reason.
        _ChkAlloc( buff, MAX_REC + 1 ); // have to have at least this much RAM
        NewCommandSource( fname, buff, NONBUFFERED );
        Token.where = ENDOFLINE;
        Token.line++;
    }
    CmdFile->file = file;
}

static void StartNewFile( void )
/******************************/
{
    char        *fname;
    const char  *envstring;
    char        *buff;
    f_handle    file;

    fname = FileName( Token.this, Token.len, E_COMMAND, false );
    file = QObjOpen( fname );
    if( file == NIL_FHANDLE ) {
        _LnkFree( fname );
        fname = tostring();
        envstring = GetEnvString( fname );
        if( envstring != NULL ) {
            buff = ChkStrDup( envstring );
            NewCommandSource( fname, buff, ENVIRONMENT );
        } else {
            LnkMsg( LOC+LINE+ERR+MSG_CANT_OPEN_NO_REASON, "s", fname );
            _LnkFree( fname );
            Suicide();
        }
        return;
    } else {
        SetCommandFile( file, fname );
    }
    DEBUG(( DBG_OLD, "processing command file %s", CmdFile->name ));
}

void EatWhite( void )
/*******************/
{
    while( IS_WHITESPACE( Token.next ) ) {
        Token.next++;
    }
}

static int ParseNumber( char *str, int radix )
/********************************************/
/* read a (possibly hexadecimal) number */
{
    bool        isdig;
    bool        isvalid;
    char        ch;
    int         size;
    unsigned    value;

    size = 0;
    value = 0;
    for( ;; ) {
        ch = tolower( *str );
        isdig = ( isdigit( ch ) != 0 );
        if( radix == 8 ) {
            isvalid = isdig && !(ch == '8' || ch == '9');
        } else {
            isvalid = ( isxdigit( ch ) != 0 );
        }
        if( !isvalid )
            break;
        value *= radix;
        if( isdig ) {
            value += ch - '0';
        } else {
            value += ch - 'a' + 10;
        }
        size++;
        str++;
    }
    *Token.next = (char)value;
    return( size );
}

static void MapEscapeChar( void )
/*******************************/
/* turn the current character located at Token.next into a possibly unprintable
 * character using C escape codes */
{
    char        *str;
    int         shift;

    shift = 2;
    str = Token.next + 1;
    switch( *str ) {
    case 'a':
        *Token.next = '\a';
        break;
    case 'b':
        *Token.next = '\b';
        break;
    case 'f':
        *Token.next = '\f';
        break;
    case 'n':
        *Token.next = '\n';
        break;
    case 'r':
        *Token.next = '\r';
        break;
    case 't':
        *Token.next = '\t';
        break;
    case 'v':
        *Token.next = '\v';
        break;
    case 'x':
        shift += ParseNumber( ++str, 16 );
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        shift += ParseNumber( str, 8 ) - 1;
        break;
    default:
        *Token.next = *str;
        break;
    }
    str = Token.next + shift;
    memmove( Token.next + 1, str, strlen( str ) + 1 );
}

static unsigned MapDoubleByteChar( unsigned char c )
/**************************************************/
/* if the double byte character support is on, check if the current character
 * is a double byte character skip it */
{
    switch( CmdFlags & CF_LANGUAGE_MASK ) {
    case CF_LANGUAGE_JAPANESE:
        if( (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <=0xFC) ) {
            Token.next++;
            return( 1 );
        }
        break;
    case CF_LANGUAGE_CHINESE:
        if( c > 0xFC )
            break;
        /* fall through */
    case CF_LANGUAGE_KOREAN:
        if( c > 0xFD )
            break;
        if( c < 0x81 )
            break;
        Token.next++;
        return( 1 );
    }
    return( 0 );
}

static bool MakeToken( tokcontrol ctrl, sep_type separator )
/**********************************************************/
{
    bool        quit;
    char        hmm;
    size_t      len;
    bool        forcematch;
    bool        hitmatch;
    bool        keepspecial;

    Token.this = Token.next;
    len = 0;
    quit = false;
    forcematch = (separator == SEP_QUOTE) || (separator == SEP_PAREN) || (separator == SEP_PERCENT);
    keepspecial = (separator == SEP_SPACE) || (separator == SEP_DOT_EXT);
    if( separator == SEP_DOT_EXT ) {    /* KLUDGE! we want to allow a zero*/
        len--;                  /* length token for parsing wlib files, so */
        Token.next--;           /* artificially back up one here. */
    }
    if( *Token.next == '\\' && separator == SEP_QUOTE && (ctrl & TOK_IS_FILENAME) == 0 ) {
        MapEscapeChar();        /* get escape chars starting in 1st pos. */
    }
    hmm = *Token.next;
    len += MapDoubleByteChar( (unsigned char)hmm );
    hitmatch = false;
    for( ;; ) {
        len++;
        hmm = *++Token.next;
        switch( hmm ) {
        case '\'':
            if( separator == SEP_QUOTE ) {
                ++Token.next;      // don't include end quote in next token.
                hitmatch = true;
                quit = true;
            }
            break;
        case ')':
            if( separator == SEP_PAREN ) {
                ++Token.next;    // don't include end paren in next token.
                hitmatch = true;
                quit = true;
            }
            break;
        case '%':
            if( separator == SEP_PERCENT ) {
                ++Token.next;    // don't include end percent in next token.
                hitmatch = true;
                quit = true;
            }
            break;
        case '.':
            if( (ctrl & TOK_INCLUDE_DOT) == 0 && !forcematch ) {
                quit = true;
            }
            break;
        case '{':
        case '}':
        case '(':
        case ',':
        case '=':
        case '#':
        case '@':
            if( keepspecial ) {
                break;
            }
            /* fall through */
        case '\t':
        case ' ':
            if( !forcematch ) {
                quit = true;
            }
            break;
        case '\\':
            if( separator == SEP_QUOTE && (ctrl & TOK_IS_FILENAME) == 0 ) {
                MapEscapeChar();
            }
            break;
        case '\0':
        case '\r':
        case '\n':
        case CTRLZ:
            quit = true;
            break;
        default:
            len += MapDoubleByteChar( (unsigned char)hmm );
        }
        if( quit ) {
            break;
        }
    }
    Token.len = len;
    if( forcematch && !hitmatch ) {
        return( false );     // end quote/paren not found before token end.
    }
    return( true );
}


char *FileName( const char *buff, size_t len, file_defext etype, bool force )
/***************************************************************************/
{
    const char  *namptr;
    const char  *namstart;
    char        *ptr;
    size_t      cnt;
    size_t      namelen;
    char        c;


    for( namptr = buff + len; namptr != buff; --namptr ) {
        c = namptr[-1];
        if( IS_PATH_SEP( c ) ) {
            break;
        }
    }
    namstart = namptr;
    cnt = len - ( namptr - buff );
    if( cnt == 0 ) {
        DUPSTR_STACK( ptr, buff, len );
        LnkMsg( LOC+LINE+FTL+MSG_INV_FILENAME, "s", ptr );
    }
    namelen = cnt;
    namptr = buff + len - 1;
    while( --cnt != 0 && *namptr != '.' ) {
        namptr--;
    }
    if( force || *namptr != '.' ) {
        if( force && etype == E_MAP ) {         // op map goes in current dir.
            buff = namstart;
            len = namelen;
        }
        if( cnt != 0 ) {
            len = namptr - buff;
        }
        _ChkAlloc( ptr, len + strlen( DefExt[etype] ) + 1 );
        memcpy( ptr, buff, len );
        strcpy( ptr + len, DefExt[etype] );
    } else {
        ptr = ChkToString( buff, len );
    }
    return( ptr );
}

void RestoreCmdLine( void )
/*************************/
// Restore a saved command line.
{
    cmdfilelist     *temp;

    if( CmdFile->prev == NULL ) {     /* can't free last file */
        Token.where = ENDOFCMD;
        return;
    }
    switch( Token.how ) {
    case SYSTEM:
        break;
    default:
        _LnkFree( Token.buff );
        if( CmdFile->file > STDIN_HANDLE ) {
            QClose( CmdFile->file, CmdFile->name );
        }
        break;
    }
    if( CmdFile->symprefix != NULL ) {
        _LnkFree( CmdFile->symprefix );
        CmdFile->symprefix = NULL;
    }
    _LnkFree( CmdFile->name );
    temp = CmdFile->prev;
    temp->next = CmdFile->next;
    if( temp->next != NULL ) {
        temp->next->prev = temp;
    }
    _LnkFree( CmdFile );
    CmdFile = temp;
    memcpy( &Token, &CmdFile->token, sizeof( tok ) ); // restore old state.
}

bool IsSystemBlock( void )
/************************/
// Are we in a system block?
{
    cmdfilelist     *temp;

    if( Token.how == SYSTEM )
        return( true );

    for( temp = CmdFile; temp != NULL; temp = temp->prev ) {
        if( temp->token.how == SYSTEM ) {
            return( true );
        }
    }
    return( false );
}

void BurnUtils( void )
/********************/
// Burn data structures used in command utils.
{
    void        *prev;

    if( CmdFile->next != NULL ) {
        LnkMsg( LOC+LINE+ERR+MSG_NO_INPUT_LEFT, NULL );
    }
    for( ; CmdFile != NULL; CmdFile = prev ) {
        prev = CmdFile->prev;
        if( CmdFile->file > STDIN_HANDLE ) {
            QClose( CmdFile->file, CmdFile->name );
        }
        if( CmdFile->symprefix != NULL ) {
            _LnkFree( CmdFile->symprefix );
            CmdFile->symprefix = NULL;
        }
        _LnkFree( CmdFile->name );
        switch( CmdFile->token.how ) {
        case ENVIRONMENT:
        case SYSTEM:
            break;
        default:
            _LnkFree( CmdFile->token.buff );
            break;
        }
        _LnkFree( CmdFile );
    }
    Token.how = BUFFERED;       // so error message stuff reports right name
}

outfilelist *NewOutFile( char *filename )
/***************************************/
{
    outfilelist     *fnode;

    for( fnode = OutFiles; fnode != NULL; fnode = fnode->next ) {
        if( FNAMECMPSTR( filename, fnode->fname ) == 0 ) {
            _LnkFree( filename );       // don't need this now.
            return( fnode );
        }
    }
    // file name not already in list, so add a list entry.
    _ChkAlloc( fnode, sizeof( outfilelist ) );
    InitBuffFile( fnode, filename, true );
    fnode->next = OutFiles;
    OutFiles = fnode;
    return( fnode );
}

static int stricmp_wrapper( const void *s1, const void *s2 )
{
    return( stricmp( s1, s2 ) );
}

section *NewSection( void )
/*************************/
{
    section             *sect;

    OvlNum++;
    _ChkAlloc( sect, sizeof( section ) );
    sect->next_sect = NULL;
    sect->classlist = NULL;
    sect->orderlist = NULL;
    sect->areas = NULL;
    sect->files = NULL;
    sect->modFilesHashed = CreateHTable( 256, StringiHashFunc, stricmp_wrapper, ChkLAlloc, LFree );
    sect->mods = NULL;
    sect->reloclist = NULL;
    SET_ADDR_UNDEFINED( sect->sect_addr );
    sect->ovl_num = 0;
    sect->parent = NULL;
    sect->relocs = 0;
    sect->size = 0;
    sect->outfile = NULL;
    sect->u.dist_mods = NULL;
    sect->dbg_info = NULL;
    return( sect );
}

char *GetFileName( char **membname, bool setname )
/************************************************/
{
    char        *ptr;
    size_t      namelen;
    char        *objname;
    const char  *fullmemb;
    size_t      memblen;

    DUPBUF_STACK( objname, Token.this, Token.len );
    namelen = Token.len;
    if( GetToken( SEP_PAREN, TOK_INCLUDE_DOT ) ) {   // got LIBNAME(LIB_MEMBER)
        fullmemb = GetBaseName( Token.this, Token.len, &memblen );
        *membname = ChkToString( fullmemb, memblen );
        ptr = FileName( objname, namelen, E_LIBRARY, false );
    } else {
        *membname = NULL;
        if( setname && Name == NULL ) {
            Name = ChkToString( objname, namelen );
        }
        ptr = FileName( objname, namelen, E_OBJECT, false );
    }
    return( ptr );
}
