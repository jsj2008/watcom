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
* Description:  UI internal functions used directly by OW Debugger
*
****************************************************************************/


#define UIDBG

extern void UIDBG _uifinicursor( void );
extern void UIDBG _uiinitcursor( void );
extern void UIDBG _uiswapcursor( void );
extern void UIDBG _uisetcursor( ORD row, ORD col, CURSOR_TYPE typ, CATTR attr );
extern void UIDBG _uioffcursor( void );
extern void UIDBG _uioncursor( void );
extern void UIDBG _uigetcursor( ORD *row, ORD *col, CURSOR_TYPE *typ, CATTR *attr );

extern void UIDBG _uirefresh( void );
