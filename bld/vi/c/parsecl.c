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


#include "vi.h"
#include "walloca.h"
#include "parsecl.h"
#include "ex.h"

static char pDelims[] = " /!";
static char pkwDelims[] = " /";

/*
 * ParseCommandLine - parse a command line
 */
vi_rc ParseCommandLine( const char *cmdl, linenum *n1, bool *n1flag, linenum *n2, bool *n2flag, int *token, const char **data )
{
    char        *tres, *tmp;
    int         j;
    linenum     l;
    vi_rc       rc;

    /*
     * set up for parse
     */
    tres = alloca( MAX_INPUT_LINE );
    tmp = alloca( MAX_INPUT_LINE );
    if( tmp == NULL || tres == NULL ) {
        return( ERR_NO_STACK );
    }
    *n1flag = false;
    *n2flag = false;
    *data = "";

    /*
     * change null command to '.'
     */
    cmdl = SkipLeadingSpaces( cmdl );
    if( *cmdl == '\0' ) {
        cmdl = ".";
    }

    /*
     * check for magic '%' - all lines
     */
    if( *cmdl == '%' ) {
        *n1flag = true;
        *n2flag = true;
        *n1 = 1;
        rc = CFindLastLine( n2 );
        if( rc != ERR_NO_ERR ) {
            return( rc );
        }
        cmdl = SkipLeadingSpaces( cmdl + 1 );
    /*
     * check for magic '#' - selected region
     */
    } else if( *cmdl == '#' || *cmdl == '@' ) {
        if( !SelRgn.selected ) {
            if( *cmdl == '#' ) {
                return( ERR_NO_SELECTION );
            } else {
                // use @ in scripts (eg mcsel.vi) when
                // we KNOW something was just selected
                SelRgn.selected = true;
            }
        }
        *n1flag = true;
        *n2flag = true;
        if( SelRgn.start.line > SelRgn.end.line ) {
            *n1 = SelRgn.end.line;
            *n2 = SelRgn.start.line;
        } else {
            *n1 = SelRgn.start.line;
            *n2 = SelRgn.end.line;
        }
        cmdl = SkipLeadingSpaces( cmdl + 1 );
    /*
     * try to get line range
     */
    } else {
        rc = GetAddress( &cmdl, &l );
        if( rc > ERR_NO_ERR || rc == DO_NOT_CLEAR_MESSAGE_WINDOW ) {
            return( rc );
        }
        if( rc == ERR_NO_ERR ) {
            *n1flag = true;
            *n1 = l;
            cmdl = SkipLeadingSpaces( cmdl );
            if( *cmdl == ',' ) {
                cmdl = SkipLeadingSpaces( cmdl + 1 );
                rc = GetAddress( &cmdl, &l );
                if( rc > ERR_NO_ERR ) {
                    return( rc );
                }
                if( rc != ERR_NO_ERR ) {
                    return( ERR_INVALID_COMMAND );
                }
                *n2flag = true;
                /*
                 * swap order (if start > end)
                 */
                if( *n1 > l ) {
                    *n2 = *n1;
                    *n1 = l;
                } else {
                    *n2 = l;
                }
            }
        }
    }

    /*
     * check for system token
     */
    if( *cmdl == '!' ) {
        *data = cmdl + 1;
        *token = PCL_T_SYSTEM;
        return( ERR_NO_ERR );
    }

    /*
     * get token and data
     */
    cmdl = GetNextWord( cmdl, tres, pkwDelims );
    if( *tres == '\0' ) {
        return( ERR_NO_ERR );
    }
    if( CheckAlias( tres, tmp ) == ERR_NO_ERR ) {
        strcat( tmp, cmdl );
        cmdl = GetNextWord( tmp, tres, pDelims );
        if( *tres == '\0' ) {
            return( ERR_NO_ERR );
        }
    }

    j = Tokenize( CmdLineTokens, tres, false );
    if( j == TOK_INVALID ) {
        j = Tokenize( ExCmdTokens, tres, false );
        if( j != TOK_INVALID ) {
            j += 1000;
        }
    }
    *token = j;
    *data = cmdl;
    return( ERR_NO_ERR );

} /* ParseCommandLine */

#define NUM_STACK_SIZE  30

/*
 * GetAddress - parse to obtain line number
 */
vi_rc GetAddress( const char **buffp, linenum *num  )
{
    linenum     numstack[NUM_STACK_SIZE];
    char        currnum[NUM_STACK_SIZE];
    linenum     sum;
    int         numptr;
    int         nument, csign, numsign;
    int         i, j;
    bool        numinprog, stopnum, endparse;
    char        c;
    char        *tmp, st[2];
    int         len;
    find_type   fl;
    i_mark      pos;
    vi_rc       rc;
    const char  *buff;

    /*
     * check if we have a numeric type thingy here
     */
    buff = *buffp;
    c = *buff;
    if( !(c == '/' || c == '?' || c == '+' || c == '-' || c == '\'' ||
        c == '.' || c == '$' || (c >= '0' && c <= '9')) ) {
        return( NO_NUMBER );
    }
    if( c == '+' || c == '-' ) {
        sum = CurrentPos.line;
    } else {
        sum = 0;
    }
    numptr = nument = 0;
    csign = numsign = 1;
    numinprog = stopnum = false;

    endparse = false;
    while( !endparse ) {
        c = *buff;
        switch( c ) {
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
            currnum[numptr++] = c;
            numinprog = true;
            break;
        case '/':
        case '?':
            if( numinprog ) {
                return( NO_NUMBER );
            }
            tmp = StaticAlloc();
            st[0] = c;
            st[1] = '\0';
            buff = GetNextWord( buff, tmp, st );
            if( c == '?' ) {
                fl = FINDFL_BACKWARDS | FINDFL_NEXTLINE;
            } else {
                fl = FINDFL_FORWARD | FINDFL_NEXTLINE;
            }
            rc = GetFind( tmp, &pos, &len, fl );
            numstack[nument] = pos.line;
            stopnum = true;
            StaticFree( tmp );
            if( rc != ERR_NO_ERR ) {
                return( rc );
            }
            if( *buff == '\0' )
                --buff;
            break;
        case '\'':
            if( numinprog ) {
                return( NO_NUMBER );
            }
            j = buff[1] - 'a';
            rc = VerifyMark( j + 1, true );
            if( rc != ERR_NO_ERR ) {
                return( rc );
            }
            numstack[nument] = MarkList[j].p.line;
            stopnum = true;
            ++buff;
            break;
        case '+':
            csign = 1;
            if( numinprog ) {
                stopnum = true;
            } else {
                numsign = 1;
            }
            break;
        case '-':
            if( numinprog ) {
                stopnum = true;
                csign = -1;
            } else {
                numsign = -1;
                csign = 1;
            }
            break;
        case '.':
            if( numinprog ) {
                return( NO_NUMBER );
            }
            numstack[nument] = CurrentPos.line;
            stopnum = true;
            break;
        case '$':
            if( numinprog ) {
                return( NO_NUMBER );
            }
            rc = CFindLastLine( &numstack[nument] );
            if( rc != ERR_NO_ERR ) {
                return( rc );
            }
            stopnum = true;
            break;
        default:
            --buff;
            endparse = true;
            if( numinprog ) {
                stopnum = true;
            }
            break;
        }
        ++buff;

        /*
         * check if a number was being scanned
         */
        if( stopnum && numinprog ) {
            currnum[numptr] = '\0';
            numptr = 0;
            numstack[nument] = atol( currnum );
            numinprog = false;
        }
        if( stopnum ) {
            numstack[nument] *= numsign;
            nument++;
            numsign = csign;
            csign = 1;
            stopnum = false;
        }
    }
    *buffp = buff;
    for( i = 0; i < nument; i++ ) {
        sum += numstack[i];
    }
    *num = sum;
    return( ERR_NO_ERR );

} /* GetAddress */
