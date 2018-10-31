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


#include "guiwind.h"
#include <string.h>
#include "guicombo.h"
#include "guicontr.h"
//#include "guixhook.h"
#include "ctl3dcvr.h"
#include "wclbproc.h"
#include "guixwind.h"


typedef struct {
    bool        success;
    WPI_WNDPROC old;
} enum_info;

extern  controls_struct GUIControls[];

/* Local Window callback functions prototypes */
WINEXPORT BOOL CALLBACK GUISubClassEditComboboxEnumFunc( HWND hwnd, WPI_PARAM2 lparam );

BOOL CALLBACK GUISubClassEditComboboxEnumFunc( HWND hwnd, WPI_PARAM2 lparam )
{
    char        buff[5];
    enum_info   *info;
    int         len;

    info = ( enum_info * )lparam;
    if( info == NULL ) {
        return( FALSE );
    }
    if( info->success == true ) {
        return( TRUE );
    }
    len = GetClassName( hwnd, buff, sizeof( buff ) );
    buff[len] = '\0';
#ifndef __OS2_PM__
    //if( strcmp( buff, "#6" ) == 0 ) {
    if( strcmp( buff, GUIControls[GUI_EDIT].classname ) == 0 ) {
        info->success = true;
        info->old = GUIDoSubClass( hwnd, GUI_EDIT );
        //CvrCtl3dSubclassCtl( hwnd );
    }
#endif
    return( TRUE );
}

WPI_WNDPROC GUISubClassEditCombobox( HWND hwnd )
{
    enum_info           e_info;
    WPI_ENUMPROC        enumproc;

    e_info.success = false;
    enumproc = _wpi_makeenumprocinstance( GUISubClassEditComboboxEnumFunc, GUIMainHInst );
    _wpi_enumchildwindows( hwnd, enumproc, (LPARAM)&e_info );
    _wpi_freeenumprocinstance( enumproc );
    if( e_info.success ) {
        return( e_info.old );
    }
    return( NULL );
}
