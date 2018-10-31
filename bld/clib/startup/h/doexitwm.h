/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2018-2018 The Open Watcom Contributors. All Rights Reserved.
*
*  ========================================================================
*
* Description:  DOS exit with message function prototype.
*
****************************************************************************/


// ASM interface
// - always uses register calling convention
// - this function is only called from the C implementation for DOS
//   of __exit_with_msg
extern _WCNORETURN void __do_exit_with_msg( char _WCI86FAR *, unsigned );
#if defined( _M_I86 )
    #pragma aux __do_exit_with_msg "*_" parm caller [ax dx] [bx]
#elif defined( _M_IX86 )
    #pragma aux __do_exit_with_msg "*_" parm caller [eax] [edx]
#endif
