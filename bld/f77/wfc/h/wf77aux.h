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
* Description:  Auxiliary information processing.
*
****************************************************************************/


extern  aux_info        DefaultInfo;
extern  aux_info        IFVarInfo;
extern  aux_info        IFCharInfo;
extern  aux_info        IFChar2Info;
extern  aux_info        IFXInfo;
extern  aux_info        IFInfo;
extern  aux_info        FortranInfo;
extern  aux_info        ProgramInfo;
extern  aux_info        *AuxInfo;

extern  aux_info        RtRtnInfo;
extern  aux_info        RtVarInfo;
extern  aux_info        RtStopInfo;
extern  aux_info        CoRtnInfo;

extern void            InitAuxInfo( void );
extern void            FiniAuxInfo( void );
extern void            SubAuxInit( void );
extern void            SubAuxFini( void );
extern void            AddDependencyInfo( source_t *fi );
extern void            DefaultLibInfo( void );
extern aux_info        *NewAuxEntry( char *name, int name_len );
extern void            DoPragma( char *ptr );
extern void            ProcPragma( char *ptr );
extern void            CopyAuxInfo( aux_info *dst, aux_info *src );
