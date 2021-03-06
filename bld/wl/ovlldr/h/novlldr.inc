;*****************************************************************************
;*
;*                            Open Watcom Project
;*
;*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
;*
;*  ========================================================================
;*
;*    This file contains Original Code and/or Modifications of Original
;*    Code as defined in and that are subject to the Sybase Open Watcom
;*    Public License version 1.0 (the 'License'). You may not use this file
;*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
;*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
;*    provided with the Original Code and Modifications, and is also
;*    available at www.sybase.com/developer/opensource.
;*
;*    The Original Code and all software distributed under the License are
;*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
;*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
;*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
;*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
;*    NON-INFRINGEMENT. Please see the License for the specific language
;*    governing rights and limitations under the License.
;*
;*  ========================================================================
;*
;* Description:  New Overlay Manager definitions
;*
;*****************************************************************************

;   !!!!!!!!!   must correspond to declaration in novlldr.h  !!!!!!!!!

        include ovltab.inc

;
;       See novlldr.h for docs on these structures.
;

ifdef OVL_MULTITHREAD

TRAP_ENTRY STRUC
    te_stack_trap       dw      ?
    te_ret_list         dw      ?
    te_ret_offset       dw      ?
    te_context          dw      ?
TRAP_ENTRY ENDS

RET_TRAP STRUC
    rt_call_far         db      ?
    rt_entry            dd      ?
    rt_pad              db      ?
    rt_old_code_handle  dw      ?
    rt_traps            TRAP_ENTRY <?>
RET_TRAP ENDS

TASK_LIST STRUC
    tl_saved_bp         dw      ?
    tl_saved_sp         dw      ?
    tl_next             dw      ?
TASK_LIST ENDS

else

RET_TRAP STRUC
    rt_call_far         db      ?
    rt_entry            dd      ?
    rt_pad              db      ?
    rt_ret_offset       dw      ?
    rt_old_code_handle  dw      ?
    rt_ret_list         dw      ?
    rt_stack_trap       dw      ?
RET_TRAP ENDS

endif

CALL_FAR                equ     9aH

FREE_BLOCK STRUC
    fb_next             dw      ?
    fb_prev             dw      ?
    fb_num_paras        dw      ?
FREE_BLOCK ENDS

AREA_LIST STRUC
    al_fblk             FREE_BLOCK <?>
    al_next             dw      ?
    al_size             dw      ?
    al_free_paras       dw      ?
AREA_LIST ENDS
