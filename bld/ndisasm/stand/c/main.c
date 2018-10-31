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
* Description:  Standalone disassembler mainline.
*
****************************************************************************/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dis.h"
#include "global.h"
#include "main.h"
#include "init.h"
#include "fini.h"
#include "externs.h"
#include "pass1.h"
#include "pass2.h"
#include "publics.h"
#include "buffer.h"
#include "formasm.h"
#include "hashtabl.h"
#include "pdata.h"
#include "groups.h"
#include "memfuncs.h"
#include "demangle.h"

#include "clibext.h"


#define SMALL_STRING_LEN        8
#define TMP_TABLE_SIZE          29

wd_options      Options;
char            LabelChar = 0;
char            QuoteChar = '\\';

int             OutputDest;

char *          ObjFileName = NULL;
char *          ListFileName = NULL;

hash_table      HandleToSectionTable;
hash_table      HandleToLabelListTable;
hash_table      HandleToRefListTable;
hash_table      SymbolToLabelTable;
hash_table      NameRecognitionTable;
hash_table      SkipRefTable = NULL;

orl_handle              ORLHnd;
orl_file_handle         ObjFileHnd = ORL_NULL_HANDLE;
orl_sec_handle          debugHnd = ORL_NULL_HANDLE;
dis_handle              DHnd;
dis_format_flags        DFormat;

section_list_struct     Sections;
publics_struct          Publics;

static int              flatModel = 0;

extern char             *SourceFileInObject;

static void printUnixHeader( section_ptr section )
{
    orl_sec_alignment   alignment;
    orl_sec_flags       flags;
    orl_sec_type        type;
    char                attributes[10];
    char *              ca;

    alignment = ORLSecGetAlignment( section->shnd );
    type = ORLSecGetType( section->shnd );
    flags = ORLSecGetFlags( section->shnd );

    ca = attributes;
    if( (Options & METAWARE_COMPATIBLE) == 0 ) {
        if( (flags & ORL_SEC_FLAG_EXEC) || section->type == SECTION_TYPE_TEXT ) {
            *ca++ = 'c';
        }
        if( (flags & ORL_SEC_FLAG_INITIALIZED_DATA) || section->type == SECTION_TYPE_DATA || section->type == SECTION_TYPE_PDATA ) {
            *ca++ = 'd';
        }
        if( (flags & ORL_SEC_FLAG_UNINITIALIZED_DATA) || section->type == SECTION_TYPE_BSS ) {
            *ca++ = 'u';
        }
        if( (type == ORL_SEC_TYPE_NOTE) || section->type == SECTION_TYPE_DRECTVE ) {
            *ca++ = 'i';
        }
        if( flags & ORL_SEC_FLAG_DISCARDABLE ) {
            *ca++ = 'n';
        }
        if( flags & ORL_SEC_FLAG_REMOVE ) {
            *ca++ = 'R';
        }
        if( flags & ORL_SEC_FLAG_READ_PERMISSION ) {
            *ca++ = 'r';
        }
        if( flags & ORL_SEC_FLAG_WRITE_PERMISSION ) {
            *ca++ = 'w';
        }
        if( flags & ORL_SEC_FLAG_EXECUTE_PERMISSION ) {
            *ca++ = 'x';
        }
        if( flags & ORL_SEC_FLAG_SHARED ) {
            *ca++ = 's';
        }
        *ca++ = '0' + (char)alignment;
        *ca = '\0';

        if( (DFormat & DFF_ASM) == 0 ) {
            BufferConcat("\t\t\t\t");
        }
        BufferStore(".new_section %s, \"%s\"", section->name, attributes );
    } else {
        if( (flags & ORL_SEC_FLAG_REMOVE) == 0 ) {
            *ca++ = 'a';
        }
        if( flags & ORL_SEC_FLAG_EXEC ) {
            *ca++ = 'x';
        }
        if( flags & ORL_SEC_FLAG_WRITE_PERMISSION ) {
            *ca++ = 'w';
        }
        *ca++ = '\0';
        if( (DFormat & DFF_ASM) == 0 ) {
            BufferConcat("\t\t\t\t");
        }
        BufferStore(".section %s, \"%s\"", section->name, attributes );
        BufferConcatNL();
        if( (DFormat & DFF_ASM) == 0 ) {
            BufferConcat("\t\t\t\t");
        }
        BufferStore(".align %d", (int)alignment );
    }

    BufferConcatNL();
    BufferPrint();
}

static char *getAlignment( orl_sec_alignment alignment )
{
    switch( alignment ) {
    case 0:
        return( "BYTE" );
    case 1:
        return( "WORD" );
    case 2:
        return( "DWORD" );
    case 4:
        return( "PARA" );
    case 8:
    case 12:
        return( "PAGE" );
    default:
        return( "" );
    }
}


static char *getUse( orl_sec_flags flags )
{
    if( flags & ORL_SEC_FLAG_USE_32 ) {
        return( "USE32" );
    } else {
        return( "USE16" );
    }
}


static char *getAlloc( orl_sec_combine combine )
{
    switch( combine & ORL_SEC_COMBINE_COMDAT_ALLOC_MASK ) {
    case ORL_SEC_COMBINE_COMDAT_ALLOC_EXPLIC:
        return( "EXPLICIT" );
    case ORL_SEC_COMBINE_COMDAT_ALLOC_CODE16:
        return( "CODE16" );
    case ORL_SEC_COMBINE_COMDAT_ALLOC_CODE32:
        return( "CODE32" );
    case ORL_SEC_COMBINE_COMDAT_ALLOC_DATA16:
        return( "DATA16" );
    case ORL_SEC_COMBINE_COMDAT_ALLOC_DATA32:
        return( "DATA32" );
    default:
        return( "" );
    }
}


static char *getPick( orl_sec_combine combine )
{
    switch( combine & ORL_SEC_COMBINE_COMDAT_PICK_MASK ) {
    case ORL_SEC_COMBINE_COMDAT_PICK_NONE:
        return( "NONE" );
    case ORL_SEC_COMBINE_COMDAT_PICK_ANY:
        return( "ANY" );
    case ORL_SEC_COMBINE_COMDAT_PICK_SAME:
        return( "SAME" );
    case ORL_SEC_COMBINE_COMDAT_PICK_EXACT:
        return( "EXACT" );
    default:
        return( "" );
    }
}


static char *getCombine( orl_sec_combine combine )
{
    switch( combine & ORL_SEC_COMBINE_MASK ) {
    case ORL_SEC_COMBINE_PRIVATE:
        return( "PRIVATE" );
    case ORL_SEC_COMBINE_PUBLIC:
        return( "PUBLIC" );
    case ORL_SEC_COMBINE_STACK:
        return( "STACK" );
    case ORL_SEC_COMBINE_COMMON:
        return( "COMMON" );
    default:
        return( "" );
    }
}


static void printMasmHeader( section_ptr section )
{
    orl_sec_alignment   alignment;
    orl_sec_flags       flags;
    orl_sec_type        type;
    orl_sec_frame       frame;
    orl_sec_combine     combine;
    dis_sec_size        size;
    const char          *name;
    const char          *class;
    const char          *gname;
    char                *astr;
    orl_sec_handle      shnd;
    orl_group_handle    grp;

    size = ORLSecGetSize( section->shnd );

    // Load all necessary information
    name = section->name;
    if( name == NULL ) {
        name = "";
    }
    type = ORLSecGetType( section->shnd );
    flags = ORLSecGetFlags( section->shnd );
    frame = ORLSecGetAbsFrame( section->shnd );

    if( DFormat & DFF_ASM ) {
        class = ORLSecGetClassName( section->shnd );
        if( class == NULL ) {
            class = "";
        }
        if( flags & ORL_SEC_FLAG_COMDAT ) {
            BufferConcat( "; ERROR: Comdat symbols cannot be assembled." );
        } else if( frame == ORL_SEC_NO_ABS_FRAME ) {
            alignment = ORLSecGetAlignment( section->shnd );
            combine = ORLSecGetCombine( section->shnd );
            BufferQuoteName( name );
            BufferStore( "\t\tSEGMENT\t%s %s %s '%s'",
                         getAlignment( alignment ), getCombine( combine ),
                         getUse( flags ), class );
        } else {
            BufferQuoteName( name );
            BufferStore( "\t\tSEGMENT\t at %08X %s '%s'", (unsigned)frame << 4,
                         getUse( flags ), class );
        }
    } else {
        if( flags & ORL_SEC_FLAG_COMDAT ) {
            char    *comname;
            size_t  len;

            if( Options & NODEMANGLE_NAMES ) {
                len = strlen( name );
                comname = MemAlloc( len + 1 );
                memcpy( comname, name, len );
                comname[len] = '\0';
            } else {
                len = __demangle_l( name, 0, NULL, 0 );
                comname = MemAlloc( len + 1 );
                __demangle_l( name, 0, comname, len + 1 );
            }
            combine = ORLSecGetCombine( section->shnd );
            if( (combine & ORL_SEC_COMBINE_COMDAT_ALLOC_MASK) == ORL_SEC_COMBINE_COMDAT_ALLOC_EXPLIC ) {
                shnd = ORLSecGetAssociated( section->shnd );
                grp = ORLSecGetGroup( section->shnd );
                astr = "SEGMENT";
            } else {
                shnd = ORL_NULL_HANDLE;
                grp = ORL_NULL_HANDLE;
                alignment = ORLSecGetAlignment( section->shnd );
                astr = getAlignment( alignment );
            }
            if( shnd != ORL_NULL_HANDLE || grp != ORL_NULL_HANDLE ) {
                name = NULL;
                gname = NULL;
                if( grp != ORL_NULL_HANDLE ) {
                    gname = ORLGroupName( grp );
                }
                if( shnd != ORL_NULL_HANDLE ) {
                    name = ORLSecGetName( shnd );
                    if( name != NULL ) {
                        if( gname != NULL ) {
                            name = gname;
                            gname = NULL;
                        } else {
                            name = "";
                        }
                    }
                } else {
                    name = gname;
                    gname = NULL;
                }
                if( gname != NULL ) {
                    BufferStore( "Comdat: %s %s %s '%s:%s' %08X bytes", comname,
                                 astr, getPick( combine ), gname, name, size );
                } else {
                    BufferStore( "Comdat: %s %s %s '%s' %08X bytes", comname,
                                 astr, getPick( combine ), name, size );
                }
                MemFree( comname );
            } else {
                BufferStore( "Comdat: %s %s %s %s %08X bytes", name, astr,
                         getPick( combine ), getAlloc( combine ), size );
            }
        } else if( frame == ORL_SEC_NO_ABS_FRAME ) {
            alignment = ORLSecGetAlignment( section->shnd );
            BufferStore( "Segment: %s %s %s %08X bytes", name,
                         getAlignment( alignment ), getUse( flags ), size );
        } else {
            BufferStore( "Segment: %s at %08X %s %08X bytes", name, frame << 4,
                         getUse( flags ), size );
        }
    }
    BufferConcatNL();
    BufferPrint();
}


void PrintAssumeHeader( section_ptr section )
{
    orl_group_handle    grp;
    const char          *name;

    if( IsMasmOutput() && (DFormat & DFF_ASM) ) {
        grp = ORLSecGetGroup( section->shnd );
        if( grp != ORL_NULL_HANDLE ) {
            name = ORLGroupName( grp );
        } else {
            name = section->name;
        }
        BufferStore( "\t\tASSUME CS:%s, DS:DGROUP, SS:DGROUP", name );
        BufferConcatNL();
        BufferPrint();
    }
}


void PrintHeader( section_ptr section )
{
    if( IsMasmOutput() ) {
        printMasmHeader( section );
    } else {
        printUnixHeader( section );
    }
}


void PrintTail( section_ptr section )
{
    const char      *name;

    if( IsMasmOutput() && (DFormat & DFF_ASM) ) {
        name = section->name;
        if( name == NULL ) {
            name = "";
        }
        BufferQuoteName( name );
        BufferStore( "\t\tENDS", name );
        BufferConcatNL();
        BufferPrint();
    }
}


void PrintLinePrefixAddress( dis_sec_offset off, bool is32bit )
{
    if( is32bit ) {
        BufferStore( "%08X", off );
    } else {
        BufferStore( "%04X", off );
    }
}


void PrintLinePrefixData( unsigned_8 *data, dis_sec_offset off, dis_sec_offset total, unsigned item_size, unsigned len )
{
    unsigned    done;
    union ptr {
        unsigned_8      u8;
        unsigned_16     u16;
        unsigned_32     u32;
    }           *p;

    p = (union ptr *)( data + off );
    total -= off;
    BufferConcat( " " );
    for( done = 0; done < len; done += item_size ) {
        BufferConcat( " " );
        if( done < total ) {
            if( item_size == 1 ) {
                BufferStore( "%02X", p->u8 );
            } else if( item_size == 2 ) {
                BufferStore( "%04X", p->u16 );
            } else if( item_size == 4 ) {
                BufferStore( "%08X", p->u32 );
            }
            p = (union ptr *)( (char *)p + item_size );
        } else {
            if( item_size == 1 ) {
                BufferConcat( "  " );
            } else if( item_size == 2 ) {
                BufferConcat( "    " );
            } else if( item_size == 4 ) {
                BufferConcat( "        " );
            }
        }
    }
}


static return_val disassembleSection( section_ptr section, unsigned_8 *contents,
                dis_sec_size size, unsigned pass )
{
    hash_data           *h_data;
    label_list          sec_label_list;
    ref_list            sec_ref_list;
    return_val          error;
    externs             sec_externs;
    num_errors          disassembly_errors;
    hash_key            h_key;

    h_key.u.sec_handle = section->shnd;
    h_data = HashTableQuery( HandleToLabelListTable, h_key );
    if( h_data != NULL ) {
        sec_label_list = h_data->u.sec_label_list;
    } else {
        sec_label_list = NULL;
    }
    h_data = HashTableQuery( HandleToRefListTable, h_key );
    if( h_data != NULL ) {
        sec_ref_list = h_data->u.sec_ref_list;
    } else {
        sec_ref_list = NULL;
    }
    if( pass == 1 ) {
        error = DoPass1( section->shnd, contents, size, sec_ref_list, section->scan );
        if( error != RC_OKAY ) {
            PrintErrorMsg( error, WHERE_PASS_ONE );
        }
    } else {
        error = RC_OKAY;
        disassembly_errors = DoPass2( section, contents, size, sec_label_list, sec_ref_list );
        if( (DFormat & DFF_ASM) == 0 ) {
            if( disassembly_errors > 0 ) {
                BufferStore( "%d ", (int)disassembly_errors );
                BufferMsg( DIS_ERRORS );
            } else {
                BufferMsg( NO_DIS_ERRORS );
            }
            BufferConcatNL();
            BufferConcatNL();
            BufferPrint();
            if( Options & PRINT_EXTERNS ) {
                sec_externs = CreateExterns( sec_ref_list );
                if( sec_externs ) {
                    PrintExterns( sec_externs );
                    FreeExterns( sec_externs );
                } else {
                    error = RC_OUT_OF_MEMORY;
                    PrintErrorMsg( error, WHERE_GEN_EXTERNS );
                }
            }
        }
    }
    return( error );
}

static label_entry dumpLabel( label_entry l_entry, section_ptr section,
                              dis_sec_offset loop, dis_sec_offset end )
{
    bool    is32bit;

    is32bit = ( end >= 0x10000 );
    for( ; l_entry != NULL && ( l_entry->type == LTYP_ABSOLUTE || l_entry->offset <= loop ); l_entry = l_entry->next ) {
        switch( l_entry->type ){
        case LTYP_ABSOLUTE:
            break;
        case LTYP_UNNAMED:
            PrintLinePrefixAddress( loop, is32bit );
            BufferAlignToTab( PREFIX_SIZE_TABS );
            if( loop != l_entry->offset ) {
                BufferStore( "%c$%d equ $-%d", LabelChar, l_entry->label.number, (int)( loop - l_entry->offset ) );
            } else {
                BufferStore( "%c$%d:", LabelChar, l_entry->label.number );
            }
            BufferConcatNL();
            break;
        case LTYP_SECTION:
        case LTYP_NAMED:
            if( strcmp( l_entry->label.name, section->name ) == 0 )
                break;
            /* fall down */
        default:
            PrintLinePrefixAddress( loop, is32bit );
            BufferAlignToTab( PREFIX_SIZE_TABS );
            if( loop != l_entry->offset ) {
                BufferStore( "%s equ $-%d", l_entry->label.name, (int)( loop - l_entry->offset ) );
            } else {
                BufferStore( "%s:", l_entry->label.name );
            }
            BufferConcatNL();
            break;
        }
    }
    return( l_entry );
}

static dis_sec_offset checkForDupLines( unsigned_8 *contents, dis_sec_offset loop,
                                        dis_sec_size size, label_entry l_entry,
                                        ref_entry r_entry )
{
    unsigned_8                  *cmp;
    dis_sec_offset              d;
    unsigned int                lines;

    cmp = &contents[loop - 16];
    if( l_entry != NULL && ( l_entry->offset < size ) )
        size = l_entry->offset;
    if( r_entry != NULL && ( r_entry->offset < size ) )
        size = r_entry->offset;
    if( ( size - loop ) < ( 16 * MIN_DUP_LINES ) )
        return( 0 );

    for( d = loop; d < ( size - 16 ); d += 16 ) {
        if( memcmp( cmp, &contents[d], 16 ) ) {
            break;
        }
    }
    d -= loop;
    lines = d / 16;
    if( lines < MIN_DUP_LINES )
        return( 0 );
    BufferConcatNL();
    BufferStore( "\t--- Above line repeats %u times ---", lines );
    return( d );
}

void DumpDataFromSection( unsigned_8 *contents, dis_sec_offset start,
                          dis_sec_offset end, label_entry *lab_entry,
                          ref_entry *reference_entry, section_ptr section )
{
    dis_sec_offset              loop;
    unsigned                    loop2;
    unsigned                    amount;
    label_entry                 l_entry;
    ref_entry                   r_entry;
    bool                        is32bit;

    is32bit = ( end >= 0x10000 );

    l_entry = *lab_entry;
    r_entry = *reference_entry;

    for( loop = start; loop < end; ) {
        /* Print a label if required */
        l_entry = dumpLabel( l_entry, section, loop, end );

        if( l_entry != NULL ) {
            amount = l_entry->offset - loop;
            if( amount > 16 ) {
                amount = 16;
            }
        } else {
            amount = 16;
        }
        if( (loop + amount) > end )
            amount = end - loop;
        /* Skip over pair relocs */
        for( ; r_entry != NULL; r_entry = r_entry->next ) {
            if( r_entry->type != ORL_RELOC_TYPE_PAIR && r_entry->offset >= loop ) {
                if( r_entry->offset < loop + amount ) {
                    if( r_entry->offset == loop ) {
                        amount = RelocSize( r_entry );
                    } else {
                        amount = r_entry->offset - loop;
                    }
                }
                break;
            }
        }

        /* This is a bit fake.  We want to print a full 16 columns,
           but we only want the amount of data specified, up to 16. */
        PrintLinePrefixAddress( loop, is32bit );
        PrintLinePrefixData( contents, loop, loop + amount, 1, 16 );
        BufferConcat( " " );
        if( r_entry != NULL && r_entry->offset == loop ) {
            HandleRefInData( r_entry, contents + loop, true );
            loop += amount;
        } else {
            for( loop2 = 0; loop2 < amount; loop2++, loop++ ) {
                if( contents[loop] >= ' ' && contents[loop] <= '~' ) {
                    BufferStore( "%c", contents[loop] );
                } else {
                    BufferConcat( "." );
                }
            }

            // We don't want to display a lot of duplicate lines
            // So we check this here.  We only do this is we have a
            // full 16 bytes displayed per line.
            if( amount == 16 ) {
                loop += checkForDupLines( contents, loop, end, l_entry, r_entry );
            }
        }
        BufferConcatNL();
        BufferPrint();
    }

    *lab_entry = l_entry;
    *reference_entry = r_entry;
}

static void dumpSection( section_ptr section, unsigned_8 *contents, dis_sec_size size, unsigned pass )
{
    hash_data                   *h_data;
    label_list                  sec_label_list;
    label_entry                 l_entry;
    ref_list                    sec_ref_list;
    ref_entry                   r_entry;
    hash_key                    h_key;

    /* Obtain the Symbol Table */
    h_key.u.sec_handle = section->shnd;
    h_data = HashTableQuery( HandleToLabelListTable, h_key );
    if( h_data != NULL ) {
        sec_label_list = h_data->u.sec_label_list;
        l_entry = sec_label_list->first;
    } else {
        sec_label_list = NULL;
        l_entry = NULL;
    }

    /* Obtain the reloc table */
    h_data = HashTableQuery( HandleToRefListTable, h_key );
    r_entry = NULL;
    if( h_data != NULL ) {
        sec_ref_list = h_data->u.sec_ref_list;
        if( sec_ref_list != NULL ) {
            r_entry = sec_ref_list->first;
        }
    }

    if( pass == 1 ) {
        DoPass1Relocs( contents, r_entry, 0, size );
        return;
    }

    PrintHeader( section );

    DumpDataFromSection( contents, 0, size, &l_entry, &r_entry, section );

    if( size > 0 ) {
        l_entry = dumpLabel( l_entry, section, size, size );
    }

    BufferConcatNL();
    BufferPrint();
}

static void bssSection( section_ptr section, dis_sec_size size, unsigned pass )
{
    hash_data           *h_data;
    label_list          sec_label_list;
    label_entry         l_entry;
    bool                is32bit;
    hash_key            h_key;

    if( pass == 1 )
        return;

    is32bit = ( size >= 0x10000 );

    /* Obtain the Symbol Table */
    h_key.u.sec_handle = section->shnd;
    h_data = HashTableQuery( HandleToLabelListTable, h_key );
    if( h_data != NULL ) {
        sec_label_list = h_data->u.sec_label_list;
    } else {
        sec_label_list = NULL;
    }

    PrintHeader( section );

    for( l_entry = sec_label_list->first; l_entry != NULL; l_entry = l_entry->next ) {
        switch( l_entry->type ) {
        case LTYP_UNNAMED:
            PrintLinePrefixAddress( l_entry->offset, is32bit );
            BufferAlignToTab( PREFIX_SIZE_TABS );
            BufferStore("%c$%d:\n", LabelChar, l_entry->label.number );
            break;
        case LTYP_SECTION:
            if( strcmp( l_entry->label.name, section->name ) == 0 )
                break;
            /* Fall through */
        case LTYP_NAMED:
            PrintLinePrefixAddress( l_entry->offset, is32bit );
            BufferAlignToTab( PREFIX_SIZE_TABS );
            BufferStore("%s:\n", l_entry->label.name );
            break;
        }
        BufferPrint();
    }
    BufferConcatNL();
    BufferMsg( BSS_SIZE );
    BufferStore(" %d ", size );
    BufferMsg( BYTES );
    BufferConcatNL();
    BufferConcatNL();
    BufferPrint();
}

static return_val DealWithSection( section_ptr section, unsigned pass )
{
    dis_sec_size        size;
    unsigned_8          *contents;
    return_val          error = RC_OKAY;

    switch( section->type ) {
    case SECTION_TYPE_TEXT:
        ORLSecGetContents( section->shnd, &contents );
        size = ORLSecGetSize( section->shnd );
        error = disassembleSection( section, contents, size, pass );
        break;
    case SECTION_TYPE_DRECTVE:
    case SECTION_TYPE_DATA:
        ORLSecGetContents( section->shnd, &contents );
        size = ORLSecGetSize( section->shnd );
        if( DFormat & DFF_ASM ) {
            error = DumpASMSection( section, contents, size, pass );
        } else {
            dumpSection( section, contents, size, pass );
        }
        break;
    case SECTION_TYPE_BSS:
        size = ORLSecGetSize( section->shnd );
        if( DFormat & DFF_ASM ) {
            error = BssASMSection( section, size, pass );
        } else {
            bssSection( section, size, pass );
        }
        break;
    case SECTION_TYPE_PDATA:
        ORLSecGetContents( section->shnd, &contents );
        size = ORLSecGetSize( section->shnd );
        error = DumpPDataSection( section, contents, size, pass );
        break;
    }
    return( error );
}

static void numberUnnamedLabels( label_entry l_entry )
{
    static label_number labNum = 1;

    for( ; l_entry != NULL; l_entry = l_entry->next ) {
        if( l_entry->type == LTYP_UNNAMED ) {
            l_entry->label.number = labNum;
            labNum++;
        }
    }
}

static hash_table emitGlobls( void )
{
    section_ptr         section;
    hash_data           *h_data;
    label_list          sec_label_list;
    label_entry         l_entry;
    char                *globl;
    hash_table          hash;
    char                *name;
    hash_entry_data     key_entry;

    hash = HashTableCreate( TMP_TABLE_SIZE, HASH_STRING );
    if( hash == NULL )
        return( NULL );

    if( IsMasmOutput() ) {
        globl = "\t\tPUBLIC\t";
    } else {
        globl = ".globl\t\t";
    }

    for( section = Sections.first; section != NULL; section = section->next ) {
        key_entry.key.u.sec_handle = section->shnd;
        h_data = HashTableQuery( HandleToLabelListTable, key_entry.key );
        if( h_data != NULL ) {
            sec_label_list = h_data->u.sec_label_list;
            if( sec_label_list != NULL ) {
                for( l_entry = sec_label_list->first; l_entry != NULL; l_entry = l_entry->next ) {
                    name = l_entry->label.name;
                    if( ( l_entry->binding != ORL_SYM_BINDING_LOCAL ) && (l_entry->type == LTYP_NAMED) && strcmp( name, section->name ) ) {
                        BufferConcat( globl );
                        BufferConcat( name );
                        BufferConcatNL();
                        BufferPrint();
                        key_entry.key.u.string = name;
                        key_entry.data.u.string = name;
                        HashTableInsert( hash, &key_entry );
                    }
                }
            }
        }
    }
    return( hash );
}

static void emitExtrns( hash_table hash )
{
    section_ptr         section;
    hash_data           *h_data;
    ref_list            r_list;
    ref_entry           r_entry;
    label_entry         l_entry;
    label_list          l_list;
    char                *extrn;
    char                *name;
    hash_entry_data     key_entry;

    if( hash == NULL ) {
        hash = HashTableCreate( TMP_TABLE_SIZE, HASH_STRING );
    }

    if( IsMasmOutput() ) {
        extrn = "\t\tEXTRN\t%s:BYTE";
    } else {
        extrn = ".extern\t\t%s";
    }

    for( section = Sections.first; section != NULL; section = section->next ) {
        key_entry.key.u.sec_handle = section->shnd;
        h_data = HashTableQuery( HandleToRefListTable, key_entry.key );
        if( h_data != NULL ) {
            r_list = h_data->u.sec_ref_list;
            if( r_list != NULL ) {
                for( r_entry = r_list->first; r_entry != NULL; r_entry = r_entry->next ) {
                    if( r_entry->label->shnd == ORL_NULL_HANDLE ) {
                        name = r_entry->label->label.name;
                        if( name == NULL )
                            continue;
                        key_entry.key.u.string = name;
                        h_data = HashTableQuery( hash, key_entry.key );
                        if( h_data == NULL ) {
                            key_entry.data.u.string = name;
                            HashTableInsert( hash, &key_entry );
                            if( ( r_entry->label->type != LTYP_GROUP ) && (r_entry->label->binding != ORL_SYM_BINDING_LOCAL) ) {
                                BufferStore( extrn, name );
                                BufferConcatNL();
                                BufferPrint();
                            }
                        }
                    }
                }
            }
        }
    }

    /* emit all externs not used but defined
     */
    key_entry.key.u.string = NULL;
    h_data = HashTableQuery( HandleToLabelListTable, key_entry.key );
    if( h_data != NULL ) {
        l_list = h_data->u.sec_label_list;
        if( l_list != NULL ) {
            for( l_entry = l_list->first; l_entry != NULL; l_entry = l_entry->next ) {
                name = l_entry->label.name;
                key_entry.key.u.string = name;
                h_data = HashTableQuery( hash, key_entry.key );
                if( h_data == NULL ) {
                    key_entry.data.u.string = name;
                    HashTableInsert( hash, &key_entry );
                    if( ( l_entry->binding != ORL_SYM_BINDING_LOCAL ) &&
                        ( l_entry->type == LTYP_EXTERNAL_NAMED ) ) {
                        BufferStore( extrn, name );
                        BufferConcatNL();
                        BufferPrint();
                    }
                }
            }
        }
    }
    HashTableFree( hash );
}

static orl_return       groupWalker( orl_group_handle grp )
{
    const char          *name;
    orl_table_index     size;
    orl_table_index     idx;

    name = ORLGroupName( grp );
    size = ORLGroupSize( grp );
    if( name == NULL || ( size < 1 ) )
        return( ORL_OKAY );
    DumpASMGroupName( name, (DFormat & DFF_ASM) );
    for( idx = 0; idx < size; idx++ ) {
        name = ORLGroupMember( grp, idx );
        if( name != NULL ) {
            DumpASMGroupMember( name );
        }
    }
    DumpASMGroupFini();

    return( ORL_OKAY );
}

void UseFlatModel( void )
{
    if( !flatModel && ( GetMachineType() == ORL_MACHINE_TYPE_I386 ) ) {
        flatModel = 1;
    }
}

static void doPrologue( void )
{
    int         masm;

    masm = IsMasmOutput();

    /* output the listing */
    if( masm ) {
        if( DFormat & DFF_ASM ) {
            BufferConcat( ".387" );
            BufferConcatNL();
            BufferPrint();
            if( GetMachineType() == ORL_MACHINE_TYPE_I386 ) {
                BufferConcat( ".386p" );
                BufferConcatNL();
                BufferPrint();
                if( flatModel ) {
                    BufferConcat( ".model flat" );
                    BufferConcatNL();
                    BufferPrint();
                }
            }
        } else if( SourceFileInObject ) {
            BufferStore( "Module: %s", SourceFileInObject );
            BufferConcatNL();
            BufferPrint();
        }
    }

    if( DFormat & DFF_ASM ) {
        emitExtrns( emitGlobls() );
    }

    if( masm ) {
        ORLGroupsScan( ObjFileHnd, groupWalker );
        if( (DFormat & DFF_ASM) == 0 ) {
            BufferConcatNL();
        }
    }
}

static void    doEpilogue( void )
{
    if( IsMasmOutput() ) {
        if( DFormat & DFF_ASM ) {
            BufferConcat( "\t\tEND" );
            BufferConcatNL();
            BufferPrint();
        }
    }
}

int main( int argc, char *argv[] )
{
    section_ptr         section;
    return_val          error;
    hash_data           *h_data;
    label_list          sec_label_list;
    hash_key            h_key;

#if !defined( __WATCOMC__ )
    _argv = argv;
    _argc = argc;
#endif

    error = Init();
    if( error == RC_OKAY ) {
        /* build the symbol table */
        for( section = Sections.first; section != NULL; section = section->next ) {
            error = DealWithSection( section, 1 );
            if( error != RC_OKAY ) {
                break;
            }
        }
        if( error == RC_OKAY ) {
            /* number all the anonymous labels */
            for( section = Sections.first; section != NULL; section = section->next ) {
                h_key.u.sec_handle = section->shnd;
                h_data = HashTableQuery( HandleToLabelListTable, h_key );
                if( h_data != NULL ) {
                    sec_label_list = h_data->u.sec_label_list;
                    if( sec_label_list != NULL ) {
                        numberUnnamedLabels( sec_label_list->first );
                    }
                }
            }
            doPrologue();
            for( section = Sections.first; section != NULL; section = section->next ) {
                error = DealWithSection( section, 2 );
                if( error != RC_OKAY ) {
                    break;
                }
            }
            if( error == RC_OKAY ) {
                doEpilogue();
                if( Options & PRINT_PUBLICS ) {
                    PrintPublics();
                }
            }
        }
    }
    Fini();
    if( error != RC_OKAY )
        return( EXIT_FAILURE );
    return( EXIT_SUCCESS );
}
