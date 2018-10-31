/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2015-2016 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  File grep dialogs and framework.
*
****************************************************************************/


#include "vi.h"
#include "posix.h"
#include <fcntl.h>
#include "walloca.h"
#include "rxsupp.h"
#include "win.h"
#ifdef __WIN__
    #include "filelist.h"
    #include "font.h"
    #include "loadcc.h"
    #ifdef __NT__
        #undef _WIN32_IE
        #define _WIN32_IE   0x0400
        #include <commctrl.h>
    #endif
    #include "wclbproc.h"
#endif
#include <assert.h>

#include "clibext.h"


/* Local Windows CALLBACK function prototypes */
#ifdef __WIN__
WINEXPORT INT_PTR CALLBACK GrepListDlgProc( HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam );
#ifdef __NT__
WINEXPORT INT_PTR CALLBACK GrepListDlgProc95( HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam );
#endif
#endif

#define isEOL(x)        ((x == CR) || (x == LF) || (x == CTRLZ))

#define MAXBYTECNT  4096
#define MAX_DISP    60

static void fileGrep( const char *, char **, int *, window_id );
static vi_rc fSearch( const char *, char * );
static vi_rc eSearch( const char *, char * );
static vi_rc doGREP( const char * );

static regexp       *cRx;
static char         *searchString;
static const char   *origString;
static char         *cTable;
static bool         isFgrep, caseIgn;

/*
 * DoFGREP - do a fast grep
 */
vi_rc DoFGREP( const char *dirlist, const char *string, bool ci )
{
    char        table[256];
    int         i;
    vi_rc       rc;

    origString = string;
    searchString = DupString( string );
    isFgrep = true;
    caseIgn = ci;
    for( i = 0; i < sizeof( table ); i++ ) {
        table[i] = i;
    }
    cTable = table;
    if( ci ) {
        for( i = 'A'; i <= 'Z'; i++) {
            table[i] = i - 'A' + 'a';
        }
        strlwr( searchString );
    }
    rc = doGREP( dirlist );
    MemFree( searchString );
    return( rc );

} /* DoFGREP */

/*
 * DoEGREP - do an extended grep
 */
vi_rc DoEGREP( const char *dirlist, const char *string )
{
    vi_rc   rc;

    cRx = RegComp( string );
    if( RegExpError ) {
        return( RegExpError );
    }
    searchString = DupString( string );
    origString = string;
    isFgrep = false;
    rc = doGREP( dirlist );
    MemFree( searchString );
    MemFree( cRx );
    return( rc );

} /* DoEGREP */

static vi_rc getFile( const char *fname )
{
    char            dir[MAX_STR];
    char            *dirptr;
    char            ch;
    vi_rc           rc;
    history_data    *h;

    GetNextWord1( fname, dir );
    rc = EditFile( dir, false );
    if( rc != ERR_NO_ERR ) {
        return( rc );
    }
    dirptr = dir;
    if( isFgrep ) {
        if( caseIgn ) {
            ch = '~';
        } else {
            ch = '@';
        }
        dirptr += 2;
        strcpy( dirptr, origString );
        MakeExpressionNonRegular( dirptr );
        dirptr--;
        *dirptr = ch;
        if( IsMagicCharRegular( ch ) ) {
            dirptr--;
            *dirptr = '\\';
        }
    } else {
        strcpy( dir, origString );
    }
    h = &EditVars.Hist[HIST_FIND];
    ReplaceString( h->data + ( h->curr % h->max ), origString );
    h->curr += 1;
    ColorFind( dirptr, FINDFL_NOERROR );
    SetLastFind( origString );
    return( rc );
}

static int initList( window_id wid, const char *dirlist, char **list )
{
    char        dir[MAX_STR];
    int         clist;
    size_t      len;

#ifdef __WIN__
    InitGrepDialog();
#endif

    /*
     * go after each directory given on the command line
     */
    clist = 0;
    EditFlags.WatchForBreak = true;
    dirlist = GetNextWord1( dirlist, dir );
    if( *dir == '\0' ) {
        fileGrep( EditVars.GrepDefault, list, &clist, wid );
    } else {
        do {
            len = strlen( dir ) - 1;
            if( dir[len] == FILE_SEP ) {
#ifdef __UNIX__
                if( len > 0 ) {
#else
                if( len > 0 && ( len != 2 || dir[1] != DRV_SEP ) ) {
#endif
                        dir[len] = '\0';
                }
            }
            if( IsDirectory( dir ) ) {
                strcat( dir, FILE_SEP_STR );
                strcat( dir, EditVars.GrepDefault );
            }
            fileGrep( dir, list, &clist, wid );
            if( EditFlags.BreakPressed ) {
                break;
            }
            dirlist = GetNextWord1( dirlist, dir );
        } while( *dir != '\0' );
    }
    if( EditFlags.BreakPressed ) {
#ifdef __WIN__
        EditFlags.BreakPressed = false;
#else
        ClearBreak();
#endif
    }
    EditFlags.WatchForBreak = false;
#ifdef __WIN__
    FiniGrepDialog();
#endif
    return( clist );
}

#ifdef __WIN__

static void getOneFile( HWND dlg, char **files, int *count, bool leave )
{
    int         i, j;
    HWND        list_box;
  #ifdef __NT__
    LVITEM      lvi;
  #endif

    list_box = GetDlgItem( dlg, ID_FILE_LIST );
  #ifdef __NT__
    if( IsCommCtrlLoaded() ) {
        i = (int)SendMessage( list_box, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED );
    } else {
  #endif
        i = (int)SendMessage( list_box, LB_GETCURSEL, 0, 0L );
  #ifdef __NT__
    }
  #endif
    if( i == -1 ) {
    } else {
        getFile( files[i] );
        if( leave ) {
            EndDialog( dlg, ERR_NO_ERR );
        } else {
            /* remove it from the list box */
      #ifdef __NT__
            if( IsCommCtrlLoaded() ) {
                SendMessage( list_box, LVM_DELETEITEM, i, 0L );
                lvi.stateMask = LVIS_SELECTED;
                lvi.state = LVIS_SELECTED;
                if( !SendMessage( list_box, LVM_SETITEMSTATE, i, (LPARAM)&lvi ) ) {
                    SendMessage( list_box, LVM_SETITEMSTATE, i - 1, (LPARAM)&lvi );
                }
            } else {
      #endif
                j = SendMessage( list_box, LB_DELETESTRING, i, 0L );
                assert( (j + 1) == (*count) );
                if( SendMessage( list_box, LB_SETCURSEL, i, 0L ) == LB_ERR ) {
                    SendMessage( list_box, LB_SETCURSEL, i - 1, 0L );
                }
      #ifdef __NT__
            }
      #endif
            MemFree( files[i] );
            for( j = i; j < *count; j++ ) {
                files[j] = files[j + 1];
            }
            (*count)--;
            if( *count == 0 ) {
                EndDialog( dlg, ERR_NO_ERR );
            }
        }
    }
}

static void getAllFiles( HWND dlg, char **files, int *count )
{
    int         i;

    for( i = 0; i < *count; i++ ) {
        getFile( files[i] );
    }
    EndDialog( dlg, ERR_NO_ERR );

} /* editFiles */

WINEXPORT INT_PTR CALLBACK GrepListDlgProc( HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam )
{
    static char         **fileList;
    static int          fileCount;
    HWND                list_box;
    char                tmp[MAX_STR];
    WORD                cmd;

    switch( msg ) {
    case WM_INITDIALOG:
        list_box = GetDlgItem( dlg, ID_FILE_LIST );
        SendMessage( list_box, WM_SETFONT, (WPARAM)FontHandle( dirw_info.text_style.font ), 0L );
        MySprintf( tmp, "Files Containing \"%s\"", searchString );
        SetWindowText( dlg, tmp );
        fileList = (char **)MemAlloc( sizeof( char * ) * MAX_FILES );
        fileCount = initList( list_box, (const char *)lparam, fileList );
        if( fileCount == 0 ) {
            /* tell him that there are no matches and close down? */
            Message1( "String \"%s\" not found", searchString );
            EndDialog( dlg, DO_NOT_CLEAR_MESSAGE_WINDOW );
        } else {
            SendMessage( list_box, LB_SETCURSEL, 0, 0L );
            BringWindowToTop( dlg );
            SetFocus( dlg );
        }
        break;
    case WM_COMMAND:
        cmd = LOWORD( wparam );
        switch( cmd ) {
        case ID_FILE_LIST:
            if( GET_WM_COMMAND_CMD( wparam, lparam ) == LBN_DBLCLK ) {
                getOneFile( dlg, fileList, &fileCount, true );
            }
            break;
        case ID_EDIT:
        case ID_GOTO:
            getOneFile( dlg, fileList, &fileCount, cmd == ID_GOTO );
            break;
        case ID_GETALL:
            getAllFiles( dlg, fileList, &fileCount );
            break;
        case IDCANCEL:
            EndDialog( dlg, ERR_NO_ERR );
            return( TRUE );
        }
        break;
    case WM_DESTROY:
        MemFreeList( fileCount, fileList );
        break;
    }
    return( FALSE );

} /* GrepListDlgProc */

  #ifdef __NT__

WINEXPORT INT_PTR CALLBACK GrepListDlgProc95( HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam )
{
    static char         **fileList;
    static int          fileCount;
    HWND                list_box;
    char                tmp[MAX_STR];
    WORD                cmd;
    LVCOLUMN            lvc;
    LVITEM              lvi;
    RECT                rc;
    LPNMITEMACTIVATE    x;

    switch( msg ) {
    case WM_INITDIALOG:
        list_box = GetDlgItem( dlg, ID_FILE_LIST );
        SendMessage( list_box, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );
        SendMessage( list_box, WM_SETFONT, (WPARAM)FontHandle( dirw_info.text_style.font ), 0L );
        MySprintf( tmp, "Files Containing \"%s\"", searchString );
        SetWindowText( dlg, tmp );
        rc.left = 0;
        rc.right = 70;
        MapDialogRect( dlg, &rc );
        lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        lvc.cx = rc.right;
        lvc.pszText = "File Name";
        lvc.iSubItem = 0;
        SendMessage( list_box, LVM_INSERTCOLUMN, 0, (LPARAM)&lvc );
        lvc.cx = rc.right * 3;
        lvc.pszText = "Line";
        lvc.iSubItem = 1;
        SendMessage( list_box, LVM_INSERTCOLUMN, 1, (LPARAM)&lvc );
        fileList = (char **)MemAlloc( sizeof( char * ) * MAX_FILES );
        fileCount = initList( list_box, (const char *)lparam, fileList );
        if( fileCount == 0 ) {
            Message1( "String \"%s\" not found", searchString );
            EndDialog( dlg, DO_NOT_CLEAR_MESSAGE_WINDOW );
        } else {
            lvi.stateMask = LVIS_SELECTED;
            lvi.state = LVIS_SELECTED;
            SendMessage( list_box, LVM_SETITEMSTATE, 0, (LPARAM)&lvi );
            BringWindowToTop( dlg );
            SetFocus( dlg );
        }
        break;
    case WM_COMMAND:
        cmd = LOWORD( wparam );
        switch( cmd ) {
        case ID_EDIT:
        case ID_GOTO:
            getOneFile( dlg, fileList, &fileCount, cmd == ID_GOTO );
            break;
        case ID_GETALL:
            getAllFiles( dlg, fileList, &fileCount );
            break;
        case IDCANCEL:
            EndDialog( dlg, ERR_NO_ERR );
            return( TRUE );
        }
        break;
    case WM_NOTIFY:
        x = (LPNMITEMACTIVATE)lparam;
        if( x->hdr.code == NM_DBLCLK ) {
            getOneFile( dlg, fileList, &fileCount, true );
        }
        break;
    case WM_DESTROY:
        MemFreeList( fileCount, fileList );
        break;
    }
    return( FALSE );

} /* GrepListDlgProc95 */

  #endif

static vi_rc doGREP( const char *dirlist )
{
    DLGPROC     dlgproc;
    vi_rc       rc;

  #ifdef __NT__
    if( LoadCommCtrl() ) {
        dlgproc = MakeProcInstance_DLG( GrepListDlgProc95, InstanceHandle );
        rc = DialogBoxParam( InstanceHandle, "GREPLIST95", root_window_id, dlgproc, (LPARAM)dirlist );
    } else {
  #endif
        dlgproc = MakeProcInstance_DLG( GrepListDlgProc, InstanceHandle );
        rc = DialogBoxParam( InstanceHandle, "GREPLIST", root_window_id, dlgproc, (LPARAM)dirlist );
  #ifdef __NT__
    }
  #endif
    FreeProcInstance_DLG( dlgproc );
    return( rc );
}
#else

/* edit options */
static const char *editOpts[] = {
    (const char *)"<F1> Go To",
    (const char *)"<F2> Edit",
    (const char *)"<F3> Get All"
};

static const vi_key     editopts_evlist[] = {
    VI_KEY( F1 ),
    VI_KEY( F2 ),
    VI_KEY( F3 ),
    VI_KEY( DUMMY )
};

/*
 * doGREP - perform GREP on a specified file
 */
static vi_rc doGREP( const char *dirlist )
{
    int         i;
    int         clist;
    int         n = 0;
    window_id   wid;
    char        **list;
    window_info wi_disp, wi_opts;
    int         s, e, cnt;
    bool        show_lineno;
    selectitem  si;
    vi_rc       rc;

    /*
     * prepare list array
     */
    clist = 0;
    list = (char **)MemAlloc( sizeof( char * ) * MAX_FILES );

    /*
     * create info. window
     */
    rc = NewWindow( &wid, dirw_info.area.x1, dirw_info.area.y1 + 4, dirw_info.area.x2,
        dirw_info.area.y1 + 6, true, dirw_info.border_color1, dirw_info.border_color2,
        &dirw_info.text_style );

    if( rc == ERR_NO_ERR ) {
        WindowTitle( wid, "File Being Searched" );
        clist = initList( wid, dirlist, list );
        /*
         * got list of matches, so lets select an item, shall we?
         */
        CloseAWindow( wid );
        if( clist ) {
            /*
             * define display window dimensions
             */
            memcpy( &wi_disp, &dirw_info, sizeof( window_info ) );
            wi_disp.area.x1 = 14;
            wi_disp.area.x2 = EditVars.WindMaxWidth - 2;
            i = wi_disp.area.y2 - wi_disp.area.y1 + 1;
            if( wi_disp.has_border ) {
                i -= 2;
            }
            if( clist < i ) {
                wi_disp.area.y2 -= ( i - clist );
            }
            show_lineno = ( clist > i );

            /*
             * build options window
             */
            memcpy( &wi_opts, &extraw_info, sizeof( window_info ) );
            wi_opts.area.x1 = 0;
            wi_opts.area.x2 = 13;
            rc = DisplayExtraInfo( &wi_opts, &wid, editOpts, sizeof( editOpts ) / sizeof( editOpts[0] ) );
            if( rc == ERR_NO_ERR ) {
                /*
                 * process selections
                 */
                for( ;; ) {
                    if( n + 1 > clist ) {
                        n = clist - 1;
                    }
                    memset( &si, 0, sizeof( si ) );
                    si.wi = &wi_disp;
                    si.title = "Files With Matches";
                    si.list = list;
                    si.maxlist = clist;
                    si.num = n;
                    si.retevents = editopts_evlist;
                    si.event = VI_KEY( DUMMY );
                    si.show_lineno = show_lineno;
                    si.cln = n + 1;
                    si.eiw = wid;

                    rc = SelectItem( &si );
                    n = si.num;

                    if( rc != ERR_NO_ERR || n < 0 ) {
                        break;
                    }
                    if( si.event == VI_KEY( F3 ) ) {
                        s = 0;
                        e = clist - 1;
                    } else {
                        s = e = n;
                    }
                    for( cnt = s; cnt <= e; cnt++ ) {
                        rc = getFile( list[cnt] );
                        if( rc != ERR_NO_ERR ) {
                            break;
                        }
                    }
                    if( rc != ERR_NO_ERR || si.event == VI_KEY( DUMMY ) ||
                        si.event == VI_KEY( F1 ) || si.event == VI_KEY( F3 ) ) {
                        break;
                    }
                    MemFree( list[n] );
                    for( i = n; i < clist - 1; i++ ) {
                        list[i] = list[i + 1];
                    }
                    clist--;
                    if( clist == 0 ) {
                        break;
                    }
                    MoveWindowToFrontDammit( wid, false );
                }
                CloseAWindow( wid );
            }
        } else if( rc == ERR_NO_ERR ) {
            Message1( "String \"%s\" not found", searchString );
            rc = DO_NOT_CLEAR_MESSAGE_WINDOW;
        }
    }

    /*
     * cleanup
     */
    MemFreeList( clist, list );
    return( rc );

} /* DoFGREP */
#endif

/*
 * fileGrep - search a single dir and build list of files
 */
static void fileGrep( const char *dir, char **list, int *clist, window_id wid )
{
    char        fn[FILENAME_MAX], data[FILENAME_MAX], ts[FILENAME_MAX];
    char        path[FILENAME_MAX];
    char        drive[_MAX_DRIVE], directory[_MAX_DIR], name[_MAX_FNAME];
    char        ext[_MAX_EXT];
    int         i;
#if defined( __WIN__ ) && defined( __NT__ )
    LVITEM      lvi;
#endif
    vi_rc       rc;

    /*
     * get file path prefix
     */
    _splitpath( dir, drive, directory, name, ext );
    strcpy( path, drive );
    strcat( path, directory );
//    _makepath( path, drive, directory, NULL,NULL );

    /*
     * run through each entry and search it; building a list of matches
     */
    rc = GetSortDir( dir, false );
    if( rc != ERR_NO_ERR ) {
        return;
    }
    for( i = 0; i < DirFileCount; i++ ) {
        if( !IS_SUBDIR( DirFiles[i] ) ) {

            strcpy( fn, path );
            strcat( fn, DirFiles[i]->name );
#ifdef __WIN__
            EditFlags.BreakPressed = SetGrepDialogFile( fn );
#else
            DisplayLineInWindow( wid, 1, fn );
#endif
            if( EditFlags.BreakPressed ) {
                return;
            }
            if( isFgrep ) {
                rc = fSearch( fn, ts );
            } else {
                rc = eSearch( fn, ts );
            }
            if( rc == FGREP_FOUND_STRING ) {

                ExpandTabsInABuffer( ts, strlen( ts ), data, MAX_DISP + 1 );
                strcpy( ts, data );
                MySprintf( data, "%X \"%s\"", fn, ts );
#ifdef __WIN__
                /*
                 * for windows - the handle passed in is the list box
                 * and the entire string is added to it but only the file
                 * name is added to the list
                 */
  #ifdef __NT__
                if( IsCommCtrlLoaded() ) {
                    lvi.mask = LVIF_TEXT;
                    lvi.iItem = (int)SendMessage( wid, LVM_GETITEMCOUNT, 0, 0L );
                    lvi.iSubItem = 0;
                    lvi.pszText = fn;
                    SendMessage( wid, LVM_INSERTITEM, 0, (LPARAM)&lvi );
                    lvi.iSubItem = 1;
                    lvi.pszText = ts;
                    SendMessage( wid, LVM_SETITEM, 0, (LPARAM)&lvi );
                } else {
  #endif
                    SendMessage( wid, LB_ADDSTRING, 0, (LPARAM)(LPSTR)data );
                    MySprintf( data, "%X", fn );
  #ifdef __NT__
                }
  #endif
#endif
                list[*clist] = DupString( data );
                (*clist)++;

            } else if( rc != ERR_NO_ERR ) {
                return;
            }
        }
    }

} /* fileGrep */

/*
 * eSearch - scan a file for a search string (extended)
 */
static vi_rc eSearch( const char *fn, char *res )
{
    int         i;
    char        *buff;
    FILE        *fp;
    vi_rc       rc;

    /*
     * init for file i/o
     */
    fp = fopen( fn, "r" );
    if( fp == NULL ) {
        return( ERR_FILE_NOT_FOUND );
    }

    /*
     * read lines from the file, and search through them
     */
    rc = ERR_NO_MEMORY;
    buff = StaticAlloc();
    if( buff != NULL ) {
        rc = ERR_NO_ERR;
        while( fgets( buff, EditVars.MaxLine, fp ) != NULL ) {
            for( i = strlen( buff ); i && isEOL( buff[i - 1] ); --i ) {
                buff[i - 1] = '\0';
            }
            i = RegExec( cRx, buff, true );
            if( RegExpError != ERR_NO_ERR ) {
                rc = RegExpError;
                break;
            }
            if( i ) {
                for( i = 0; i < MAX_DISP; i++ ) {
                    res[i] = buff[i];
                }
                res[i] = '\0';
                rc = FGREP_FOUND_STRING;
                break;
            }
        }
        StaticFree( buff );
    }
    fclose( fp );
    return( rc );

} /* eSearch */

/*
 * fSearch - scan a file for a search string (fast)
 */
static vi_rc fSearch( const char *fn, char *r )
{
    int         handle, j;
    int         bytes, bcnt;
    char        *buffloc, *strloc, *buff, *res;
    char        context_display[MAX_DISP];
    vi_rc       rc;

    /*
     * init for file i/o
     */
    rc = FileOpen( fn, false, O_BINARY | O_RDONLY, 0, &handle );
    if( rc != ERR_NO_ERR ) {
        return( rc );
    }
    rc = ERR_NO_MEMORY;
    buff = MemAlloc( MAXBYTECNT );
    if( buff != NULL ) {
        /*
         * read in buffers from the file, and search through them
         */
        rc = ERR_NO_ERR;
        strloc = searchString;  // don't reset at start of new block - could span blocks
        for( ;; ) {
            bcnt = bytes = read( handle, buff, MAXBYTECNT );
            buffloc = buff;
            while( bytes ) {
                if( *strloc == cTable[*(unsigned char *)buffloc] ) {
                    buffloc++;
                    bytes--;
                    strloc++;
                    if( *strloc == '\0' ) {
                        j = 0;
                        if( buffloc - strlen( searchString ) < buff ) {
                            // match spans blocks - see context_display
                            res = context_display + MAX_DISP - 1;
                            if( *res == LF ) {
                                r[j] = '\0';
                            } else {
                                for( ; res != context_display; res-- ) {
                                    if( *res == LF ) {
                                        res++;
                                        break;
                                    }
                                }
                                // copy the part of the string NOT in buff
                                while( j < MAX_DISP && *res != CR && *res != LF ) {
                                    r[j++] = *res;
                                    res++;
                                }
                                r[j] = '\0';
                            }
                            res = buff;
                        } else {
                            res = buffloc - strlen( searchString );
                            for( ; res != buff; res-- ) {
                                if( *res == LF ) {
                                    res++;
                                    break;
                                }
                            }
                        }
                        // now copy the string ( all that is in buff )
                        while( j < MAX_DISP && res != buff + MAXBYTECNT && *res != CR && *res != LF ) {
                            r[j++] = *res;
                            res++;
                        }
                        r[j] = '\0';
                        rc = FGREP_FOUND_STRING;
                        break;
                    }
                } else {
                    if( strloc == searchString ) {
                        buffloc++;
                        bytes--;
                    } else {
                        strloc = searchString;
                    }
                }

            }
            if( bcnt != MAXBYTECNT || rc == FGREP_FOUND_STRING ) {
                break;
            }
            if( strloc != searchString ) {
                // partial match -- keep the last bunch of text as context
                strncpy( context_display, buffloc - MAX_DISP, MAX_DISP );
            }
        }
        MemFree( buff );
    }
    close( handle );
    return( rc );

} /* fSearch */
