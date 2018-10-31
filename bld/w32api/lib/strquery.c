/***************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2004-2013 The Open Watcom Contributors. All Rights Reserved.
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
*  Description: Implementation of the strquery.lib library.
*
**************************************************************************/

#include <windows.h>

/* strquery.h / structuredquery.h */
const IID      __cdecl IID_IQueryParser =
    { 0x2EBDEE67, 0x3505, 0x43F8, { 0x99, 0x46, 0xEA, 0x44, 0xAB, 0xC8, 0xE5, 0xB0 } };
const IID      __cdecl IID_IQuerySolution =
    { 0xD6EBC66B, 0x8921, 0x4193, { 0xAF, 0xDD, 0xA1, 0x78, 0x9F, 0xB7, 0xFF, 0x57 } };
const IID      __cdecl IID_IConditionFactory =
    { 0xA5EFE073, 0xB16F, 0x474F, { 0x9F, 0x3E, 0x9F, 0x8B, 0x49, 0x7A, 0x3E, 0x08 } };
const IID      __cdecl IID_IConditionFactory2 =
    { 0x71D222E1, 0x432F, 0x429E, { 0x8C, 0x13, 0xB6, 0xDA, 0xFD, 0xE5, 0x07, 0x7A } };
const IID      __cdecl IID_IConditionGenerator =
    { 0x92D2CC58, 0x4386, 0x45A3, { 0xB9, 0x8C, 0x7E, 0x0C, 0xE6, 0x4A, 0x41, 0x17 } };
const IID      __cdecl IID_IInterval =
    { 0x6BF0A714, 0x3C18, 0x430B, { 0x8B, 0x5D, 0x83, 0xB1, 0xC2, 0x34, 0xD3, 0xDB } };
const IID      __cdecl IID_IMetaData =
    { 0x780102B0, 0xC43B, 0x4876, { 0xBC, 0x7B, 0x5E, 0x9B, 0xA5, 0xC8, 0x87, 0x94 } };
const IID      __cdecl IID_IEntity =
    { 0x24264891, 0xE80B, 0x4FD3, { 0xB7, 0xCE, 0x4F, 0xF2, 0xFA, 0xE8, 0x93, 0x1F } };
const IID      __cdecl IID_IRelationship =
    { 0x2769280B, 0x5108, 0x498C, { 0x9C, 0x7F, 0xA5, 0x12, 0x39, 0xB6, 0x31, 0x47 } };
const IID      __cdecl IID_INamedEntity =
    { 0xABDBD0B1, 0x7D54, 0x49FB, { 0xAB, 0x5C, 0xBF, 0xF4, 0x13, 0x00, 0x04, 0xCD } };
const IID      __cdecl IID_ISchemaProvider =
    { 0x8CF89BCB, 0x394C, 0x49B2, { 0xAE, 0x28, 0xA5, 0x9D, 0xD4, 0xED, 0x7F, 0x68 } };
const IID      __cdecl IID_ITokenCollection =
    { 0x22D8B4F2, 0xF577, 0x4ADB, { 0xA3, 0x35, 0xC2, 0xAE, 0x88, 0x41, 0x6F, 0xAB } };
const IID      __cdecl IID_INamedEntityCollector =
    { 0xAF2440F6, 0x8AFC, 0x47D0, { 0x9A, 0x7F, 0x39, 0x6A, 0x0A, 0xCF, 0xB4, 0x3D } };
const IID      __cdecl IID_ISchemaLocalizerSupport =
    { 0xCA3FDCA2, 0xBFBE, 0x4EED, { 0x90, 0xD7, 0x0C, 0xAE, 0xF0, 0xA1, 0xBD, 0xA1 } };
const IID      __cdecl IID_IQueryParserManager =
    { 0xA879E3C4, 0xAF77, 0x44FB, { 0x8F, 0x37, 0xEB, 0xD1, 0x48, 0x7C, 0xF9, 0x20 } };
const IID      __cdecl LIBID_StructuredQuery1 =
    { 0x1352FA67, 0x2022, 0x41DF, { 0x9D, 0x6F, 0x94, 0x3A, 0x5E, 0xE9, 0x7C, 0x9F } };
const CLSID    __cdecl CLSID_QueryParser =
    { 0xB72F8FD8, 0x0FAB, 0x4DD9, { 0xBD, 0xBF, 0x24, 0x5A, 0x6C, 0xE1, 0x48, 0x5B } };
const CLSID    __cdecl CLSID_NegationCondition =
    { 0x8DE9C74C, 0x605A, 0x4ACD, { 0xBE, 0xE3, 0x2B, 0x22, 0x2A, 0xA2, 0xD2, 0x3D } };
const CLSID    __cdecl CLSID_CompoundCondition =
    { 0x116F8D13, 0x101E, 0x4FA5, { 0x84, 0xD4, 0xFF, 0x82, 0x79, 0x38, 0x19, 0x35 } };
const CLSID    __cdecl CLSID_LeafCondition =
    { 0x52F15C89, 0x5A17, 0x48E1, { 0xBB, 0xCD, 0x46, 0xA3, 0xF8, 0x9C, 0x7C, 0xC2 } };
const CLSID    __cdecl CLSID_ConditionFactory =
    { 0xE03E85B0, 0x7BE3, 0x4000, { 0xBA, 0x98, 0x6C, 0x13, 0xDE, 0x9F, 0xA4, 0x86 } };
const CLSID    __cdecl CLSID_Interval =
    { 0xD957171F, 0x4BF9, 0x4DE2, { 0xBC, 0xD5, 0xC7, 0x0A, 0x7C, 0xA5, 0x58, 0x36 } };
const CLSID    __cdecl CLSID_QueryParserManager =
    { 0x5088B39A, 0x29B4, 0x4D9D, { 0x82, 0x45, 0x4E, 0xE2, 0x89, 0x22, 0x2F, 0x66 } };

/* strqcond.h / structuredquerycondition.h */
const IID      __cdecl IID_IRichChunk =
    { 0x4FDEF69C, 0xDBC9, 0x454E, { 0x99, 0x10, 0xB3, 0x4F, 0x3C, 0x64, 0xB5, 0x10 } };
const IID      __cdecl IID_ICondition =
    { 0x0FC988D4, 0xC935, 0x4B97, { 0xA9, 0x73, 0x46, 0x28, 0x2E, 0xA1, 0x75, 0xC8 } };
const IID      __cdecl IID_ICondition2 =
    { 0x0DB8851D, 0x2E5B, 0x47EB, { 0x92, 0x08, 0xD2, 0x8C, 0x32, 0x5A, 0x01, 0xD7 } };
