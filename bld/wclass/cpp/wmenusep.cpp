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


#include "wwindow.hpp"
#include "wmenusep.hpp"
#include "wpopmenu.hpp"


WEXPORT WMenuSeparator::WMenuSeparator()
        : WMenuItem( NULL, NULL, NULL ) {
/***************************************/

}


void WMenuSeparator::attachMenu( WWindow *win, int position )
/***********************************************************/
{
    gui_menu_struct     menu_item;

    menu_item.label = NULL;
    menu_item.id = menuId();
    menu_item.style = (gui_menu_styles)(GUI_STYLE_MENU_ENABLED | GUI_STYLE_MENU_SEPARATOR);
    menu_item.hinttext = NULL;
    menu_item.child_num_items = 0;
    menu_item.child = NULL;
    if( parent()->isFloatingMain() ) {
        // appending separator to top level floating popup menu
        GUIInsertMenuByIdx( win->handle(), position, &menu_item, true );
    } else {
        // appending separator to popup menu
        GUIInsertMenuToPopup( win->handle(), parent()->menuId(), position,
                              &menu_item, parent()->isFloatingPopup() );
    }
    setOwner( win );
}


void WMenuSeparator::detachMenu() {
/*********************************/

    if( owner() ) {
        if( owner()->handle() ) {
            if( parent()->isFloatingPopup() ) {
                GUIDeleteMenuItem( owner()->handle(), menuId(), true );
            }
            GUIDeleteMenuItem( owner()->handle(), menuId(), false );
        }
    }
    setOwner( NULL );
}

#ifdef __WATCOMC__
// Complain about defining trivial destructor inside class
// definition only for warning levels above 8
#pragma warning 656 9
#endif

WEXPORT WMenuSeparator::~WMenuSeparator() {
/*****************************************/

}
