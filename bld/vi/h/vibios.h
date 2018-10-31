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
* Description:  BIOS Low-level function prototypes for vi.
*
****************************************************************************/


// screen color/attrib functions
extern void             BIOSSetColorRegister( unsigned short, unsigned char, unsigned char, unsigned char );
extern uint_32          BIOSGetColorRegister( unsigned short );
extern void             BIOSGetColorPalette( void _FAR * );
extern void             BIOSSetBlinkAttr( void );
extern void             BIOSSetNoBlinkAttr( void );
// screen cursor functions
extern unsigned short   BIOSGetCursor( unsigned char );
extern void             BIOSSetCursor( unsigned char, unsigned char, unsigned char );
extern void             BIOSNewCursor( unsigned char top, unsigned char bottom );
// screen info functions
extern unsigned char    BIOSGetRowCount( void );
extern uint_32          BIOSGetVideoMode( void );
// screen update functions
extern void             BIOSUpdateScreen( size_t, unsigned );
// keyboard functions
extern int              BIOSKeyboardInit( void );
extern unsigned         BIOSTestKeyboard( void );
extern unsigned         BIOSGetKeyboard( unsigned * );
extern bool             BIOSKeyboardHit( void );

