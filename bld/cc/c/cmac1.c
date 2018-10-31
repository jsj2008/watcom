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
* Description:  Macro processing, part one.
*
****************************************************************************/


#include "cvars.h"
#include "scan.h"
#include "cfeinfo.h"
#include "dumpapi.h"

#include "clibext.h"


#define T_UNEXPANDABLE_ID       T_LAST_TOKEN

typedef struct tokens {
    struct  tokens  *next;
    size_t          length;
    char            buf[1];
} tokens;

typedef struct macro_token {
    struct macro_token  *next;
    TOKEN               token;
    char                data[1];
} MACRO_TOKEN;

typedef struct macro_arg {
    char    *arg;
} MACRO_ARG;

typedef struct nested_macros {
    struct nested_macros    *next;
    MEPTR                   mentry;
    MACRO_ARG               *macro_parms;
    bool                    rescanning;
    bool                    substituting_parms;
} NESTED_MACRO;

typedef struct special_macro_names {
    char            *name;
    special_macros  value;
} special_macro_names;

extern void         DumpMTokens( MACRO_TOKEN *mtok );

static NESTED_MACRO *NestedMacros;
static MACRO_TOKEN  *TokenList;
static size_t       MTokenLen;              /* macro token length */

static MACRO_TOKEN  *ExpandNestedMacros( MACRO_TOKEN *head, bool rescanning );

static struct special_macro_names  SpcMacros[] = {
    { "__DATE__",           MACRO_DATE          },
    { "__FILE__",           MACRO_FILE          },
    { "__LINE__",           MACRO_LINE          },
    { "__STDC__",           MACRO_STDC          },
    { "__STDC_HOSTED__",    MACRO_STDC_HOSTED   },
    { "__STDC_VERSION__",   MACRO_STDC_VERSION  },
    { "__TIME__",           MACRO_TIME          },
    { NULL,                 0                   }
};

static void SpecialMacrosInit( special_macro_names *mac )
{
    MEPTR               mentry;

    for( ; mac->name != NULL; ++mac ) {
        mentry = CreateMEntry( mac->name, strlen( mac->name ) );
        mentry->parm_count = (mac_parm_count)mac->value;
        MacroAdd( mentry, NULL, 0, MFLAG_NONE );
        FreeMEntry( mentry );
    }
}

void MacroInit( void )
{
    mac_hash_idx    h;

    MTokenLen = 0;
    MacroCount = 0;
    MacroPtr = NULL;
    CppStackInit();
    NestedMacros = NULL;
    TokenList = NULL;
    UndefMacroList = NULL;
    InitialMacroFlag = MFLAG_DEFINED_BEFORE_FIRST_INCLUDE;
    AllocMacroSegment( 0x1000 );
    MacHash = (MEPTR *)MacroSegment;
    MacroOffset = MacroSegment + MACRO_HASH_SIZE * sizeof( MEPTR );
    for( h = 0; h < MACRO_HASH_SIZE; ++h ) {
        MacHash[h] = NULL;
    }
    SpecialMacrosInit( SpcMacros );
    TimeInit(); /* grab time and date for __TIME__ and __DATE__ */
}

static struct special_macro_names  SpcMacroCompOnly[] = {
    { "__FUNCTION__",   MACRO_FUNC },
    { "__func__",       MACRO_FUNC },
    { NULL,             0 }
};

void MacroAddComp( void )
{
    SpecialMacrosInit( SpcMacroCompOnly );
}

void MacroFini( void )
{
    CppStackFini();
    MacroPurge();
}


void MacroPurge( void )
{
#if 0
    mac_hash_idx    h;
    MEPTR           mentry;

    for( h = 0; h < MACRO_HASH_SIZE; ++h ) {
        for( ; mentry = MacHash[h]; ) {
            MacHash[h] = mentry->next_macro;
            CMemFree( mentry );
        }
    }
#endif
}

static void DeleteNestedMacro( void )
{
    MEPTR           mentry;
    NESTED_MACRO    *nested;
    MACRO_ARG       *macro_parms;
    mac_parm_count  parmno;

    nested = NestedMacros;
    if( nested != NULL ) {
        NestedMacros = nested->next;
        macro_parms =  nested->macro_parms;
        mentry = nested->mentry;
        CMemFree( nested );
        if( macro_parms != NULL ) {
            for( parmno = mentry->parm_count - 1; parmno-- > 0; ) {
                CMemFree( macro_parms[parmno].arg );
            }
            CMemFree( macro_parms );
        }
    }
}

void GetMacroToken( void )
{
    MACRO_TOKEN     *mtok;
    size_t          len;
    char            *buf;
    bool            keep_token;
    bool            next_token;

    buf = Buffer;
    buf[0] = '\0';
    TokenLen = 0;
    CurToken = T_NULL;
    for( ; (mtok = TokenList) != NULL; ) {
        CurToken = mtok->token;
        len = 0;
        while( (buf[len] = mtok->data[len]) != '\0' ) {
            len++;
        }
        TokenLen = len;
        keep_token = false;
        next_token = false;
        switch( CurToken ) {
        case T_UNEXPANDABLE_ID:
            CalcHash( buf, len );
            if( !CompFlags.doing_macro_expansion ) {
                CurToken = KwLookup( buf, len );
            }
            break;
        case T_ID:
        case T_SAVED_ID:
            CalcHash( buf, len );
            if( CompFlags.doing_macro_expansion ) {
                CurToken = T_ID;
            } else {
                CurToken = KwLookup( buf, len );
            }
            break;
        case T_BAD_TOKEN:
        case T_CONSTANT:
            ReScanInit( mtok->data );
            ReScanToken();
            break;
        case T_PPNUMBER:
            ReScanInit( mtok->data );
            if( ReScanToken() ) {   // didn't finish string bad boy
                char    *tcur;
                char    *tbeg;

                tbeg = mtok->data;
                tcur = ReScanPos();
                // ppnumber is quite general so it may absorb multiple tokens
                // overlapping src & dst so do our own copy;
                for( ; (*tbeg = *tcur) != '\0'; ) {
                    ++tcur;
                    ++tbeg;
                }
                keep_token = true;
            }
            break;
        case T_LSTRING:
            CurToken = T_STRING;
            CompFlags.wide_char_string = true;
            break;
        case T_STRING:
            CompFlags.wide_char_string = false;
            break;
        case T_BAD_CHAR:
            break;
        case T_NULL:
            if( mtok->data[0] == 'Z' ) {    // if end of macro
                DeleteNestedMacro();
            }
            next_token = true;
            break;
        default:
            break;
        }
        if( !keep_token ) {
            TokenList = mtok->next;
            CMemFree( mtok );
        }
        if( !next_token ) {
            break;
        }
    }
    if( mtok == NULL ) {
        MacroPtr = NULL;
    }
}

/* returns Dynamically allocated buffer with expanded macro */
static char *ExpandMacroToken( void )
{
    size_t      i;
    size_t      len;
    char        *p;
    char        *buf;
    TOKEN       tok;

    tok = MTOK( MacroPtr );
    if( tok == T_NULL )
        return( NULL );
    MTOKINC( MacroPtr );
    p = NULL;
    len = 0;
    switch( tok ) {
    case T_CONSTANT:
    case T_PPNUMBER:
    case T_ID:
    case T_UNEXPANDABLE_ID:
    case T_SAVED_ID:
    case T_BAD_TOKEN:
        p = MacroPtr;
        len = strlen( p );
        MacroPtr += len;
        break;
    case T_LSTRING:
        len = 1;
        /* fall through */
    case T_STRING:
        len += strlen( MacroPtr ) + 3;
        buf = CMemAlloc( len );
        i = 0;
        if( tok == T_LSTRING )
            buf[i++] = 'L';
        buf[i++] = '"';
        while( (buf[i] = *MacroPtr++) != '\0' )
            ++i;
        buf[i++] = '"';
        buf[i] = '\0';
        return( buf );
    default:
        p = Tokens[tok];
        len = strlen( p );
        break;
    }
    buf = NULL;
    if( len > 0 ) {
        len++;              /* account for terminating null character */
        buf = CMemAlloc( len );
        memcpy( buf, p, len );
    }
    return( buf );
}


TOKEN SpecialMacro( special_macros spc_macro )
{
    char            *p;
    char            *bufp;

    CompFlags.wide_char_string = false;
    switch( spc_macro ) {
    case MACRO_LINE:
        sprintf( Buffer, "%u", TokenLoc.line );
        Constant = TokenLoc.line;
        ConstType = TYPE_INT;
        return( T_CONSTANT );
    case MACRO_FILE:
        bufp = Buffer;
        for( p = FileIndexToFName( TokenLoc.fno )->name; (*bufp++ = *p) != '\0'; ++p ) {
            if( *p == '\\' ) {
                *bufp++ = '\\';
            }
        }
        return( T_STRING );
    case MACRO_DATE:
        memcpy( Buffer, __Date, 12 );
        return( T_STRING );
    case MACRO_TIME:
        memcpy( Buffer, __Time, 9 );
        return( T_STRING );
    case MACRO_STDC:
        Buffer[0] = '1';
        Buffer[1] = '\0';
        Constant = 1;
        ConstType = TYPE_INT;
        return( T_CONSTANT );
    case MACRO_STDC_HOSTED:
        Buffer[0] = '1';
        Buffer[1] = '\0';
        Constant = 1;
        ConstType = TYPE_INT;
        return( T_CONSTANT );
    case MACRO_STDC_VERSION:
        if( CompFlags.c99_extensions ) {
            CPYLIT( Buffer, "199901L" );
            Constant = 199901;
        } else {
            CPYLIT( Buffer, "199409L" );
            Constant = 199409;
        }
        ConstType = TYPE_LONG;
        return( T_CONSTANT );
    case MACRO_FUNC:
        p = "";
        if( CurFunc != NULL ) {
            if( CurFunc->name != NULL ) {
                p = CurFunc->name;
            }
        }
        strcpy( Buffer, p );
        return( T_STRING );
    default:
        Buffer[0] = '\0';
        return( T_NULL ); // shut up the compiler
    }
}


static TOKEN NextMToken( void )
{
    CompFlags.doing_macro_expansion = true;
    GetMacroToken();
    if( CurToken == T_NULL ) {
        CurToken = ScanToken();
    }
    CompFlags.doing_macro_expansion = false;
    return( CurToken );
}

void EnlargeBuffer( size_t size )
{
    char       *newBuffer;

    newBuffer = CMemAlloc( size );
    memcpy( newBuffer, Buffer, BufSize );
    CMemFree( (void *)Buffer );
    Buffer = newBuffer;
    newBuffer = CMemAlloc( size );
    memcpy( newBuffer, TokenBuf, BufSize );
    CMemFree( TokenBuf );
    TokenBuf = newBuffer;
    BufSize = size;
}

static void SaveParm( MEPTR mentry, size_t size, mac_parm_count parmno,
                     MACRO_ARG *macro_parms, tokens *token_list )
{
    tokens          *token;
    char            *p;
    size_t          total;

    MTOK( TokenBuf + size ) = T_NULL;
    MTOKINC( size );

    if( parmno < mentry->parm_count - 1 ) {
        p = CMemAlloc( size );
        macro_parms[parmno].arg = p;
        if( p != NULL ) {
            total = 0;
            while( (token = token_list) != NULL ) {
                token_list = token->next;
                memcpy( &p[total], token->buf, token->length );
                total += token->length;
                CMemFree( token );
            }
            memcpy( &p[total], TokenBuf, size );
        }
    }
}

static MACRO_ARG *CollectParms( void )
{
    MEPTR           mentry;
    size_t          len;
    int             bracket;
    TOKEN           tok;
    TOKEN           prev_tok;
    mac_parm_count  parmno;
    bool            ppscan_mode;
    MACRO_ARG       *macro_parms;
    tokens          *token_head;

    macro_parms = NULL;
    mentry = NextMacro;
    if( mentry->parm_count != 0 ) {     /* if() expected */
        ppscan_mode = InitPPScan();     // enable T_PPNUMBER tokens
        if( mentry->parm_count > 1 ) {
            macro_parms = (MACRO_ARG *)CMemAlloc( ( mentry->parm_count - 1 ) * sizeof( MACRO_ARG ) );
        }
        parmno = 0;
        do {
            tok = NextMToken();
        } while( tok == T_WHITE_SPACE );
        /* tok will now be a '(' */
        bracket = 0;
        token_head = NULL;
        MTokenLen = 0;
        for( ;; ) {
            prev_tok = tok;
            do {
                tok = NextMToken();
            } while( tok == T_WHITE_SPACE && MTokenLen == 0 );
            if( tok == T_EOF || tok == T_NULL ) {
                CErr2p( ERR_INCOMPLETE_MACRO, mentry->macro_name );
                break;
            }
            if( tok == T_BAD_TOKEN && BadTokenInfo == ERR_MISSING_QUOTE ) {
                CErr1( ERR_MISSING_QUOTE );
                tok = T_RIGHT_PAREN;
            }
            if( tok == T_LEFT_PAREN ) {
                ++bracket;
            } else if( tok == T_RIGHT_PAREN ) {
                if( bracket == 0 )
                    break;
                --bracket;
            } else if( tok == T_COMMA && bracket == 0
              && ( (mentry->macro_flags & MFLAG_VAR_ARGS) == 0 || parmno != mentry->parm_count - 2 ) ) {
                if( prev_tok == T_WHITE_SPACE ) {
                    MTOKDEC( MTokenLen );
                }
                if( macro_parms != NULL ) {     // if expecting parms
                    SaveParm( mentry, MTokenLen, parmno, macro_parms, token_head );
                }
                ++parmno;
                token_head = NULL;
                MTokenLen = 0;
                continue;
            }
            /* determine size of current token */
            len = sizeof( TOKEN );
            switch( tok ) {
            case T_WHITE_SPACE:
                if( prev_tok == T_WHITE_SPACE )
                    len = 0;
                break;
            case T_STRING:
                if( CompFlags.wide_char_string ) {
                    tok = T_LSTRING;
                }
                /* fall through */
            case T_CONSTANT:
            case T_PPNUMBER:
            case T_ID:
            case T_UNEXPANDABLE_ID:
            case T_BAD_TOKEN:
                len += TokenLen + 1;
                break;
            default:
                break;
            }
            if( MTokenLen + len >= BufSize ) { /* if not enough space */
                EnlargeBuffer( ( MTokenLen + len ) * 2 );
            }
            MTOK( TokenBuf + MTokenLen ) = tok;
            MTOKINC( MTokenLen );
            switch( tok ) {
            case T_WHITE_SPACE:
                if( prev_tok == T_WHITE_SPACE ) {
                    MTOKDEC( MTokenLen );
                }
                break;
            case T_BAD_CHAR:
                TokenBuf[MTokenLen++] = Buffer[0];
                if( Buffer[1] != '\0' ) {
                     MTOK( TokenBuf + MTokenLen ) = T_WHITE_SPACE;
                     MTOKINC( MTokenLen );
                }
                break;
            case T_CONSTANT:
            case T_PPNUMBER:
            case T_ID:
            case T_UNEXPANDABLE_ID:
            case T_LSTRING:
            case T_STRING:
            case T_BAD_TOKEN:
                memcpy( TokenBuf + MTokenLen, Buffer, TokenLen + 1 );
                MTokenLen += TokenLen + 1;
                break;
            default:
                break;
            }
        }
        if( prev_tok == T_WHITE_SPACE ) {
            MTOKDEC( MTokenLen );
        }
        if( macro_parms != NULL ) {     // if expecting parms
            SaveParm( mentry, MTokenLen, parmno, macro_parms, token_head );
            ++parmno;
        } else if( MTokenLen != 0 ) {
            ++parmno;                   // will cause "too many parms" error
        }
        if( (mentry->macro_flags & MFLAG_VAR_ARGS) && parmno < ( mentry->parm_count - 2 )
           || (mentry->macro_flags & MFLAG_VAR_ARGS) == 0 && parmno < ( mentry->parm_count - 1 ) ) {
            CErr2p( ERR_TOO_FEW_MACRO_PARMS, mentry->macro_name );
        } else if( (mentry->macro_flags & MFLAG_VAR_ARGS) == 0 && parmno > ( mentry->parm_count - 1 ) ) {
            if( mentry->parm_count - 1 != 0 ) {
                CWarn2p( WARN_PARM_COUNT_MISMATCH, ERR_TOO_MANY_MACRO_PARMS, mentry->macro_name  );
            }
        } else if( CMPLIT( mentry->macro_name, "va_start" ) == 0 ) {
            if( SymLevel != 0 && !VarParm( CurFunc ) ) {
                CErr1( ERR_MUST_BE_VAR_PARM_FUNC );
            }
        }
        FiniPPScan( ppscan_mode );      // disable T_PPNUMBER tokens
    }
    return( macro_parms );
}


#ifndef NDEBUG

void DumpMDefn( const char *p )
{
    unsigned char   c;
    TOKEN           tok;

    while( (tok = MTOK( p )) != T_NULL ) {
        MTOKINC( p );
        switch( tok ) {
        case T_CONSTANT:
        case T_PPNUMBER:
        case T_ID:
        case T_UNEXPANDABLE_ID:
            for( ; (c = *p++) != '\0'; ) {
                putchar( c );
            }
            continue;
        case T_LSTRING:
            putchar( 'L' );
            /* fall through */
        case T_STRING:
            putchar( '\"' );
            for( ; (c = *p++) != '\0'; ) {
                putchar( c );
            }
            putchar( '\"' );
            continue;
        case T_WHITE_SPACE:
            putchar( ' ' );
            continue;
        case T_BAD_CHAR:
            putchar( *p++ );
            continue;
        case T_MACRO_PARM:
            printf( "<parm#%d>", MTOKPARM( p ) );
            MTOKPARMINC( p );
            continue;
        case T_MACRO_VAR_PARM:
            printf( "<varparm#%d>", MTOKPARM( p ) );
            MTOKPARMINC( p );
            continue;
        default:
            printf( "%s", Tokens[tok] );
            continue;
        }
    }
    putchar( '\n' );
    fflush( stdout );
}


void DumpMTokens( MACRO_TOKEN *mtok )
{
    for( ; mtok != NULL; mtok = mtok->next ) {
        printf( "%s\n", mtok->data );
    }
    fflush( stdout );
}


void DumpNestedMacros( void )
{
    NESTED_MACRO *nested;

    for( nested = NestedMacros; nested != NULL; nested = nested->next ) {
        printf( "%s\n", nested->mentry->macro_name );
    }
    fflush( stdout );
}
#endif


static MACRO_TOKEN *BuildAToken( TOKEN token, const char *p )
{
    size_t      len;
    MACRO_TOKEN *mtok;

    len = strlen( p ) + 1;
    mtok = (MACRO_TOKEN *)CMemAlloc( sizeof( MACRO_TOKEN ) - 1 + len );
    memcpy( mtok->data, p, len );
    mtok->token = token;
    mtok->next = NULL;
    return( mtok );
}


static MACRO_TOKEN *AppendToken( MACRO_TOKEN *head, TOKEN token, const char *data )
{
    MACRO_TOKEN *tail;
    MACRO_TOKEN *new;

    new = BuildAToken( token, data );
    if( head == NULL ) {
        head = new;
    } else {
        tail = head;
        while( tail->next != NULL ) {
            tail = tail->next;
        }
        tail->next = new;
    }
    return( head );
}

static bool MacroBeingExpanded( MEPTR mentry )
{
    NESTED_MACRO    *nested;

    for( nested = NestedMacros; nested != NULL; nested = nested->next ) {
        if( nested->mentry == mentry )
            return( true );
        if( !nested->rescanning ) {
            break;
        }
    }
    return( false );
}

static int Expandable( MACRO_TOKEN *mtok, bool macro_parm )
{
    int         lparen;

    if( NextMacro->macro_defn == 0 ) {  /* if special macro */
        return( 1 );
    }
    if( NextMacro->parm_count == 0 ) {  /* if() not expected */
        if( macro_parm ) {
            if( MacroBeingExpanded( NextMacro ) ) {
                return( 0 );
            }
        }
        return( 1 );
    }
    for( ; mtok != NULL; mtok = mtok->next ) {
        if( mtok->token != T_WHITE_SPACE && mtok->token != T_NULL ) {
            break;
        }
    }
    if( mtok != NULL ) {
        if( mtok->token == T_LEFT_PAREN ) {
            if( MacroDepth == 1 && !macro_parm )
                return( 1 );
            lparen = 0;
            for( ; (mtok = mtok->next) != NULL; ) {
                if( mtok->token == T_LEFT_PAREN ) {
                    ++lparen;
                } else if( mtok->token == T_RIGHT_PAREN ) {
                    if( lparen == 0 )
                        return( 1 );
                    --lparen;
                }
            }
        }
    } else if( !macro_parm ) {
        SkipAhead();
        if( CurrChar == '(' ) {
            return( 1 );
        } else if( CompFlags.cpp_output ) {
            return( 2 );
        }
    }
    return( 0 );
}

static char *GlueTokenToBuffer( MACRO_TOKEN *first, char *gluebuf )
{
    size_t      gluelen;
    size_t      tokenlen;
    char        *buf;

    buf = NULL;
    if( first != NULL ) {
        MacroPtr = (char *)&first->token;
        buf = ExpandMacroToken();
    }
    if( buf == NULL ) {
        buf = gluebuf;
    } else if( gluebuf != NULL ) {
        /* now do a "strcat( gluebuf, buf )" */
        gluelen = strlen( gluebuf );
        tokenlen = strlen( buf );
        gluebuf = CMemRealloc( gluebuf, gluelen + tokenlen + 1 );
        memcpy( gluebuf + gluelen, buf, tokenlen + 1 );
        CMemFree( buf );
        buf = gluebuf;
    }
    return( buf );
}

static MACRO_TOKEN *ReTokenGlueBuffer( char *gluebuf )
// retokenize starting at gluebuf
{
    MACRO_TOKEN *head;
    MACRO_TOKEN **lnk;
    MACRO_TOKEN *new;
    bool        ppscan_mode;

    ppscan_mode = InitPPScan();
    if( gluebuf == NULL )
        gluebuf = "";
    ReScanInit( gluebuf );
    head = NULL;
    lnk = &head;
    for( ;; ) {
        ReScanToken();
        new = BuildAToken( CurToken, Buffer );
        *lnk = new;
        lnk = &new->next;
        if( CompFlags.rescan_buffer_done ) {
            break;
        }
    }
    FiniPPScan( ppscan_mode );
    return( head );
}


static MACRO_TOKEN *GlueTokens( MACRO_TOKEN *head )
{
    MACRO_TOKEN *mtok;
    MACRO_TOKEN **lnk,**_lnk;// prior lnk
    MACRO_TOKEN *next;
    char        *buf;
    char        *gluebuf;

    gluebuf = NULL;
    _lnk = NULL;
    lnk = &head;
    mtok = *lnk;
    buf = Buffer;
    for( ; mtok != NULL; ) {
        if( mtok->token != T_WHITE_SPACE ) {
            next = mtok->next;
            if( next == NULL )
                break;
            if( next->token == T_WHITE_SPACE )
                next = next->next;
            if( next == NULL )
                break;
            if( next->token == T_MACRO_SHARP_SHARP ) {  // let's paste
                int         pos;
                MACRO_TOKEN *rem;

                next = next->next;
                pos = 0;
                // glue mtok->token with next->token to make one token
                if( ( mtok->token == T_COMMA && next->token == T_MACRO_EMPTY_VAR_PARM ) ||
                    ( mtok->token == T_MACRO_EMPTY_VAR_PARM && next->token == T_COMMA ) )
                {
                    // delete [mtoken(a comma),##,empty __VA_ARGS__]
                    // delete [empty __VA_ARGS__,##,mtoken(a comma)]
                    buf[10] = '\0';
                    pos = -1;
                } else {
                    if( mtok->token == T_MACRO_EMPTY_VAR_PARM ) {
                        // well should never be in this state if no next - since ## cannot
                        // appear at the end of a macro
                        gluebuf = GlueTokenToBuffer( next, NULL );
                    } else {
                        gluebuf = GlueTokenToBuffer( mtok, NULL );
                        if( next != NULL && next->token != T_MACRO_EMPTY_VAR_PARM ) {
                            gluebuf = GlueTokenToBuffer( next, gluebuf ); //paste in next
                        }
                    }
                }
                if( next != NULL ) {
                    rem = next->next;   // save unseen stuff
                    next->next = NULL;  // break link;
                } else {
                    rem = NULL;         // happens if no arg after comma
                }
                while( mtok != NULL ) { // free old stuff [mtoken,##,{atok,} next]
                    next = mtok->next;
                    CMemFree( mtok );
                    mtok = next;
                }
                if( pos >= 0 ) {
                    if( gluebuf != NULL ) {
                        mtok = ReTokenGlueBuffer( gluebuf );
                        CMemFree( gluebuf );
                    } else {
                        /* Both ends of ## were empty */
                        mtok = BuildAToken( T_NULL, "P-<placemarker>" );
                    }
                    *lnk = mtok;  // link in new mtok to mtok's link
                    while( mtok != NULL && mtok->next != NULL ) {   //position mtok & lnk to last token
                        _lnk = lnk;
                        lnk = &mtok->next;
                        mtok = *lnk;
                        if( mtok == mtok->next ) {
                            return( head );
                        }
                    }
                    // mtok == last token of retokenizing
                    // lnk == the pointer which references mtok
                    mtok->next = rem;
                } else {
                    if( _lnk ) {
                        *lnk = rem;
                        lnk = _lnk;
                        mtok = *lnk;
                    } else {
                        *lnk = rem;
                        mtok = head;
                    }
                }
                continue;          //ready to go
            }
        }
        _lnk = lnk;
        lnk = &mtok->next;
        mtok = *lnk;
    }
    return( head );
}

static MACRO_TOKEN **NextString( MACRO_TOKEN **lnk, const char *buf )
{
    MACRO_TOKEN *mtok;

    mtok = BuildAToken( T_STRING, buf );
    *lnk = mtok;
    lnk = &mtok->next;
    return( lnk );
}


static MACRO_TOKEN *BuildString( const char *p )
{
    MACRO_TOKEN     *head;
    MACRO_TOKEN     **lnk;
    size_t          i;
    char            c;
    char            *tokenstr;
    size_t          len;
    char            *buf;
    size_t          bufsize;
    TOKEN           tok;

    head = NULL;
    lnk = &head;

    len = 0;
    if( p != NULL ) {
        bufsize = BUF_SIZE;
        buf = CMemAlloc( bufsize );
        while( MTOK( p ) == T_WHITE_SPACE ) {
            MTOKINC( p );   //eat leading wspace
        }
        while( (tok = MTOK( p )) != T_NULL ) {
            MTOKINC( p );
            if( len >= ( bufsize - 8 ) ) {
                buf = CMemRealloc( buf, 2 * len );
            }
            switch( tok ) {
            case T_CONSTANT:
            case T_PPNUMBER:
            case T_ID:
            case T_UNEXPANDABLE_ID:
            case T_BAD_TOKEN:
                for( ; (c = *p++) != '\0'; ) {
                    if( c == '\\' )
                        buf[len++] = c;
                    buf[len++] = c;
                    if( len >= ( bufsize - 8 ) ) {
                        buf = CMemRealloc( buf, 2 * len );
                    }
                }
                break;
            case T_LSTRING:
                buf[len++] = 'L';
                /* fall through */
            case T_STRING:
                buf[len++] = '\\';
                buf[len++] = '"';
                for( ; (c = *p++) != '\0'; ) {
                    if( c == '\\' || c == '"' )
                        buf[len++] = '\\';
                    buf[len++] = c;
                    if( len >= ( bufsize - 8 ) ) {
                        buf = CMemRealloc( buf, 2 * len );
                    }
                }
                buf[len++] = '\\';
                buf[len++] = '"';
                break;
            case T_WHITE_SPACE:
                while( (tok = MTOK( p )) == T_WHITE_SPACE ) {
                    MTOKINC( p );
                }
                if( tok != T_NULL ) {
                    buf[len++] = ' ';
                }
                break;
            case T_BAD_CHAR:
                if( *p == '\\' && MTOK( p + 1 ) == T_NULL ) {
                    CErr1( ERR_INVALID_STRING_LITERAL );
                }
                buf[len++] = *p++;
                break;
            default:
                tokenstr = Tokens[tok];
                i = strlen( tokenstr );
                if( len >= ( bufsize - i ) )
                    buf = CMemRealloc( buf, 2 * len );
                memcpy( &buf[len], tokenstr, i );
                len += i;
                break;
            }
        }
        if( len > 0 ) {
            buf[len] = '\0';
            lnk = NextString( lnk, buf );
        }
        CMemFree( buf );
    }
    return( head );
}


static MACRO_TOKEN *BuildMTokenList( const char *p, MACRO_ARG *macro_parms )
{
    MACRO_TOKEN     *mtok;
    MACRO_TOKEN     *head;
    MACRO_TOKEN     **lnk;
    NESTED_MACRO    *nested;
    const char      *p2;
    char            buf[2];
    TOKEN           prev_token;
    TOKEN           tok;
    mac_parm_count  parmno;

    head = NULL;
    lnk = &head;
    nested = NestedMacros;
    buf[1] = '\0';
    prev_token = T_NULL;
    if( p == NULL )
        return( NULL );
    while( (tok = MTOK( p )) != T_NULL ) {
        MTOKINC( p );
        switch( tok ) {
        case T_CONSTANT:
        case T_PPNUMBER:
        case T_ID:
        case T_UNEXPANDABLE_ID:
        case T_BAD_TOKEN:
        case T_LSTRING:
        case T_STRING:
            mtok = BuildAToken( tok, p );
            while( *p++ != '\0' )
                ;
            break;
        case T_WHITE_SPACE:
            if( prev_token == T_MACRO_SHARP_SHARP )
                continue;
            mtok = BuildAToken( T_WHITE_SPACE, " " );
            break;
        case T_BAD_CHAR:
            buf[0] = *p++;
            mtok = BuildAToken( T_BAD_CHAR, buf );
            break;
        case T_MACRO_SHARP:
            while( MTOK( p ) == T_WHITE_SPACE ) {
                MTOKINC( p );
            }
            MTOKINC( p );   // skip over T_MACRO_PARM
            // If no macro arg given, result must be "", not empty
            parmno = MTOKPARM( p );
            MTOKPARMINC( p );
            if( macro_parms != NULL && macro_parms[parmno].arg != NULL && macro_parms[parmno].arg[0] != '\0' ) {
                mtok = BuildString( macro_parms[parmno].arg );
            } else {
                mtok = BuildAToken( T_STRING, "" );
            }
            break;
        case T_MACRO_PARM:
            parmno = MTOKPARM( p );
            MTOKPARMINC( p );
            p2 = p;
            while( MTOK( p2 ) == T_WHITE_SPACE ) {
                MTOKINC( p2 );
            }
            nested->substituting_parms = true;
            if( macro_parms != NULL ) {
                mtok = BuildMTokenList( macro_parms[parmno].arg, NULL );
                /* NB: mtok is now NULL if macro arg was empty */
            } else {
                mtok = BuildAToken( T_WHITE_SPACE, "" );
            }
            if( MTOK( p2 ) != T_MACRO_SHARP_SHARP && prev_token != T_MACRO_SHARP_SHARP ) {
                if( mtok != NULL ) {
                    mtok = AppendToken( mtok, T_NULL, "P-<end of parm>" );
                    mtok = ExpandNestedMacros( mtok, false );
                }
            } else if( mtok == NULL ) {
                mtok = BuildAToken( T_NULL, "P-<placemarker>" );
            }
            nested->substituting_parms = false;
            break;
        case T_MACRO_VAR_PARM:
            parmno = MTOKPARM( p );
            MTOKPARMINC( p );
            p2 = p;
            while( MTOK( p2 ) == T_WHITE_SPACE )
                MTOKINC( p2 );
            nested->substituting_parms = true;
            if( macro_parms != NULL ) {
                if( macro_parms[parmno].arg != NULL ) {
                    mtok = BuildMTokenList( macro_parms[parmno].arg, NULL );
                } else {
                    if( prev_token == T_MACRO_SHARP_SHARP || MTOK( p2 ) == T_MACRO_SHARP_SHARP ) {
                        mtok = BuildAToken( T_MACRO_EMPTY_VAR_PARM, "" );
                    } else {
                        mtok = BuildAToken( T_WHITE_SPACE, "" );
                    }
                }
            } else {
                mtok = BuildAToken( T_WHITE_SPACE, "" );
            }
            if( MTOK( p2 ) != T_MACRO_SHARP_SHARP && prev_token != T_MACRO_SHARP_SHARP ) {
                if( mtok != NULL ) {
                    mtok = AppendToken( mtok, T_NULL, "P-<end of parm>" );
                    mtok = ExpandNestedMacros( mtok, false );
                }
            }
            nested->substituting_parms = false;
            break;
        default:
            mtok = BuildAToken( tok, Tokens[tok] );
            break;
        }
        if( mtok != NULL ) {
            if( mtok->token != T_WHITE_SPACE ) {
                prev_token = mtok->token;
            }
            *lnk = mtok;
            while( *lnk != NULL ) {
                lnk = &(*lnk)->next;
            }
        }
    }
    head = GlueTokens( head );
    return( head );
}

static MACRO_TOKEN *MacroExpansion( bool rescanning )
{
    MEPTR           mentry;
    MACRO_ARG       *macro_parms;
    MACRO_TOKEN     *head;
    MACRO_TOKEN     *mtok;
    NESTED_MACRO    *nested;
    size_t          len;
    TOKEN           tok;

    mentry = NextMacro;
    nested = (NESTED_MACRO *)CMemAlloc( sizeof( NESTED_MACRO ) );
    nested->mentry = mentry;
    nested->rescanning = rescanning;
    nested->substituting_parms = false;
    nested->macro_parms = NULL;
    if( mentry->macro_defn == 0 ) {     /* if special macro */
        tok = SpecialMacro( (special_macros)mentry->parm_count );
        head = BuildAToken( tok, Buffer );
        nested->next = NestedMacros;
        NestedMacros = nested;
    } else {
        macro_parms = CollectParms();
        nested->next = NestedMacros;
        NestedMacros = nested;
        nested->macro_parms = macro_parms;
        head = BuildMTokenList( (char *)mentry + mentry->macro_defn, macro_parms );
        for( mtok = head; mtok != NULL; mtok = mtok->next ) {
            if( mtok->token == T_ID ) {
                len = strlen( mtok->data ) + 1;
                for( nested = NestedMacros; nested != NULL; nested = nested->next ) {
                    if( memcmp( nested->mentry->macro_name, mtok->data, len ) == 0 ) {
                        if( !nested->substituting_parms ) {
                            // change token so it won't be considered a
                            // candidate as a macro
                            mtok->token = T_UNEXPANDABLE_ID;
                            break;
                        }
                    }
                }
            }
        }
    }
    head = AppendToken( head, T_NULL, "Z-<end of macro>" );
    return( head );
}

static MACRO_TOKEN *ExpandNestedMacros( MACRO_TOKEN *head, bool rescanning )
{
    MACRO_TOKEN *mtok;
    MACRO_TOKEN *toklist;
    MACRO_TOKEN *prev_tok;
    MACRO_TOKEN *old_tokenlist;
    int         i;
    size_t      len;
    char        *buf;

    mtok = head;
    ++MacroDepth;
    prev_tok = NULL;
    for( ; mtok != NULL; ) {
        toklist = NULL;
        if( mtok->token == T_ID ) {
            // if macro and not being expanded, then expand it
            // only tokens available for expansion are those in mtok list
            buf = Buffer;
            len = 0;
            while( (buf[len] = mtok->data[len]) != '\0' )
                len++;
            CalcHash( buf, len );
            if( IdLookup( buf, len ) == T_MACRO ) {
                if( rescanning ) {
                    if( MacroBeingExpanded( NextMacro ) ) {
                        mtok->token = T_UNEXPANDABLE_ID;
                    } else {
                        toklist = mtok;
                        while( toklist->next != NULL ) {
                            toklist = toklist->next;
                        }
                        toklist->next = TokenList;
                        i = Expandable( mtok->next, false );
                        switch( i ) {
                        case 0:                 // macro is currently not expandable
                            mtok->token = T_MACRO;
                            toklist->next = NULL;
                            toklist = NULL;
                            break;
                        case 1:                 // macro is expandable
                            TokenList = mtok->next;
                            if( head == mtok ) {
                                head = NULL;
                                prev_tok = NULL;
                            }
                            CMemFree( mtok );
                            toklist = MacroExpansion( rescanning );
                            mtok = TokenList;
                            TokenList = NULL;
                            break;
                        case 2:                 // we skipped over some white space
                            mtok->token = T_UNEXPANDABLE_ID;
                            toklist->next = NULL;
                            toklist = BuildAToken( T_WHITE_SPACE, " " );
                            toklist->next = mtok->next;
                            mtok->next = toklist;
                            toklist = NULL;
                            break;
                        }
                    }
                } else {                        // expanding a macro parm
                    if( Expandable( mtok->next, true ) ) {
                        old_tokenlist = TokenList;
                        TokenList = mtok->next;
                        if( head == mtok ) {
                            head = NULL;
                            prev_tok = NULL;
                        }
                        CMemFree( mtok );
                        toklist = ExpandNestedMacros( MacroExpansion( false ), rescanning );
                        mtok = TokenList;
                        TokenList = old_tokenlist;
                    } else {
                        prev_tok = mtok;
                        mtok = mtok->next;      // advance onto next token
                    }
                }
            } else {
                mtok->token = T_SAVED_ID;       // avoid rechecking this ID
                prev_tok = mtok;
                mtok = mtok->next;              // advance onto next token
            }
        } else if( mtok->token == T_NULL ) {
            toklist = mtok->next;
            if( mtok->data[0] == 'Z' ) {        // end of a macro
                rescanning = NestedMacros->rescanning;
                DeleteNestedMacro();
                CMemFree( mtok );
                mtok = toklist;
            } else {                            // end of a macro parm
                if( toklist != NULL ) {
                    TokenList = toklist;
                }
                CMemFree( mtok );
                mtok = NULL;                    // will cause us to exit
            }
            toklist = NULL;
        } else {                                // advance onto next token
            prev_tok = mtok;
            mtok = mtok->next;
        }
        if( toklist != NULL ) {                 // new tokens to insert
            if( prev_tok == NULL ) {
                head = toklist;
            } else {
                prev_tok->next = toklist;
            }
            if( mtok != NULL ) {
                while( toklist->next != NULL )
                    toklist = toklist->next;
                toklist->next = mtok;
            }
            if( prev_tok == NULL ) {
                mtok = head;
            } else {
                mtok = prev_tok->next;
            }
        } else {
            // either no change, or tokens were deleted
            if( prev_tok == NULL ) {
                head = mtok;
            } else {
                prev_tok->next = mtok;
            }
        }
    }
    for( mtok = head; mtok != NULL; mtok = mtok->next ) {
        // change a temporarily unexpandable ID into an ID because it
        // could become expandable in a later rescanning phase
        if( mtok->token == T_MACRO ) {
            mtok->token = T_ID;
        }
    }
    --MacroDepth;
    return( head );
}

void DoMacroExpansion( void )               // called from cscan
{
    MacroDepth = 0;
    TokenList = ExpandNestedMacros( MacroExpansion( false ), true );
    // GetMacroToken will feed back tokens from the TokenList
    // when the TokenList is exhausted, then revert back to normal scanning
    if( TokenList == NULL ) {
        MacroPtr = NULL;
    } else {
        MacroPtr = "";
    }
}
