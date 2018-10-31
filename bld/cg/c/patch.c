/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2018 The Open Watcom Contributors. All Rights Reserved.
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


#include "_cgstd.h"
#include "coderep.h"
#include "addrname.h"
#include "tree.h"
#include "cgmem.h"
#include "types.h"
#include "addrfold.h"
#include "makeins.h"
#include "namelist.h"
#include "patch.h"
#include "procdef.h"
#include "makeblk.h"
#include "typemap.h"


patch *BGNewPatch( void )
{
    patch   *p;

    p = CGAlloc( sizeof( patch ) );
    p->in_tree = false;
    p->patched = false;
#ifndef NDEBUG
    p->useinfo.hdltype = NO_HANDLE;
    p->useinfo.used = false;
#endif
    return( p );
}

an      TNPatch( tn node )
{
    patch               *p;
    an                  addr;
    type_class_def      tipe;

    p = (patch *)node->u.handle;
    p->in_tree = false;
    tipe = TypeClass( node->tipe );
    addr = AddrName( AllocTemp( tipe ), node->tipe );
    p->u.ins = MakeMove( NULL, addr->u.n.name, tipe );
    p->u.ins->num_operands = 0;
    AddIns( p->u.ins );
    return( addr );
}

cg_name BGPatchNode( patch *hdl, type_def *tipe )
{
    hdl->patched = true;
    hdl->in_tree = true;
    hdl->u.node = TGPatch( hdl, tipe );
    return( hdl->u.node );
}

void    BGPatchInteger( patch *hdl, signed_32 value )
{
    tn                  node;
    name                *c;

    if( hdl->patched ) {
        c = AllocS32Const( value );
        if( hdl->in_tree ) {
            node = hdl->u.node;
            node->class = TN_CONS;
            node->u.name = c;
        } else {
            hdl->u.ins->operands[0] = c;
            hdl->u.ins->num_operands = 1;
        }
    }
}

void    BGFiniPatch( patch *hdl )
{
    CGFree( hdl );
}
