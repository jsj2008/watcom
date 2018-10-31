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


//
// AUXLOOK      : auxiliary information lookup
//

#include "ftnstd.h"
#include <string.h>
#include "global.h"
#include "wf77auxd.h"
#include "wf77aux.h"
#include "iflookup.h"
#include "cpopt.h"
#include "rtconst.h"
#include "auxlook.h"
#include "fcrtns.h"

#include "clibext.h"


aux_info    *AuxLookupName( char *name, uint name_len ) {
//======================================================

    aux_info    *aux;

    aux = AuxInfo;
    for(;;) {
        if( aux == NULL ) break;
        if( aux->sym_len == name_len ) {
            if( memicmp( name, aux->sym_name, name_len ) == 0 ) break;
        }
        aux = aux->link;
    }
    return( aux );
}


aux_info    *AuxLookupAdd( char *name, uint name_len ) {
//=====================================================

    aux_info    *aux;

    aux = AuxLookupName( name, name_len );
    if( aux == NULL ) {
        aux = NewAuxEntry( name, name_len );
        CopyAuxInfo( aux, &FortranInfo );
    }
    return( aux );
}


aux_info    *AuxLookup( sym_id sym ) {
//====================================

    aux_info    *info;

    if( sym == NULL ) return( &FortranInfo );
    if( (sym->u.ns.flags & SY_CLASS) == SY_SUBPROGRAM ) {
        if( sym->u.ns.flags & SY_INTRINSIC ) {
            if( IFVarArgs( sym->u.ns.si.fi.index ) ) {
                return( &IFVarInfo );
            // check for character arguments must come first so that
            // IF@xxx gets generated for intrinsic functions with character
            // arguments (instead of XF@xxxx)
            } else if( IFArgType( sym->u.ns.si.fi.index ) == FT_CHAR ) {
                if( sym->u.ns.flags & SY_IF_ARGUMENT ) {
                    if( (Options & OPT_DESCRIPTOR) == 0 ) {
                        return( &IFChar2Info );
                    }
                }
                return( &IFCharInfo );
            } else if( sym->u.ns.flags & SY_IF_ARGUMENT ) {
                return( &IFXInfo );
            }
            return( &IFInfo );
        } else if( sym->u.ns.flags & SY_RT_ROUTINE ) {
            return( RTAuxInfo( sym ) );
        } else if( (sym->u.ns.flags & SY_SUBPROG_TYPE) == SY_PROGRAM ) {
            return( &ProgramInfo );
        } else {
            info = AuxLookupName( sym->u.ns.name, sym->u.ns.u2.name_len );
            if( info == NULL ) return( &FortranInfo );
            return( info );
        }
    } else {
        info = AuxLookupName( sym->u.ns.name, sym->u.ns.u2.name_len );
        if( info == NULL ) return( &FortranInfo );
        return( info );
    }
}
