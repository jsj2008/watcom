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
* Description:  Public interface to wres library.
*
****************************************************************************/


#ifndef WRESSET2_INCLUDED
#define WRESSET2_INCLUDED

#ifdef WIN_GUI
# include <windows.h>
#endif

typedef struct handle_info {
    FILE        *fp;
    int         status;
#ifdef WIN_GUI
    HINSTANCE   inst;
#endif
} HANDLE_INFO, *PHANDLE_INFO;

#ifndef WIN_GUI
typedef unsigned int    UINT;
#endif

#ifdef _M_I86
typedef char        __far *lpstr;
typedef const char  __far *lpcstr;
#else
typedef char        *lpstr;
typedef const char  *lpcstr;
#endif

#if defined( __cplusplus )
extern "C" {
#endif

extern bool         OpenResFile( PHANDLE_INFO hinfo, const char *filename );
extern bool         OpenResFileX( PHANDLE_INFO hinfo, const char *filename, bool res_file );
extern bool         CloseResFile( PHANDLE_INFO hinfo );
extern bool         FindResources( PHANDLE_INFO hinfo );
extern bool         FindResourcesX( PHANDLE_INFO hinfo, bool res_file );
extern bool         InitResources( PHANDLE_INFO hinfo );
extern bool         FiniResources( PHANDLE_INFO hinfo );
extern int          WResLoadString( PHANDLE_INFO hinfo, UINT idResource, lpstr lpszBuffer, int nBufferMax );
extern int          WResLoadResource( PHANDLE_INFO hinfo, UINT idType, UINT idResource, lpstr *lpszBuffer, size_t *bufferSize );
extern int          WResLoadResourceX( PHANDLE_INFO hinfo, lpcstr idType, lpcstr idResource, lpstr *lpszBuffer, size_t *bufferSize );

#if defined( __cplusplus )
}
#endif

#endif
