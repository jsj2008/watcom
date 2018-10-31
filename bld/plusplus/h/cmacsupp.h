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
* Description:  Macro support routines.
*
****************************************************************************/


#ifndef _CMACSUPP_H
#define _CMACSUPP_H

void MacroOffsetAddChar(        // MacroOffset: ADD A CHARACTER
    size_t *mlen,               // - Macro Offset Length
    char chr )                  // - character to be added
;
void MacroOffsetAddToken(       // MacroOffset: ADD A TOKEN
    size_t *mlen,               // - Macro Offset Length
    TOKEN token )               // - token to be added
;
void MacroOffsetAddMem(         // MacroOffset: ADD A SEQUENCE OF BYTES
    size_t *mlen,               // - Macro Offset Length
    const char *buff,           // - bytes to be added
    size_t len )                // - number of bytes
;
void MacroOffsetAddMemNoCopy(   // As above, but no copy from old macro
    size_t *mlen,               // - Macro Offset Length
    const char *buff,           // - bytes to be added
    size_t len )                // - number of bytes
;
#endif
