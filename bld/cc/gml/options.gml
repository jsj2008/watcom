:cmt
:cmt GML Macros used:
:cmt
:cmt    :chain. <option> <usage text>           options that start with <option>
:cmt                                            can be chained together i.e.,
:cmt                                            -oa -ox -ot => -oaxt
:cmt    :option. <option> <synonym> ...         define an option
:cmt    :target. <arch1> <arch2> ...            valid for these architectures
:cmt    :ntarget. <arch1> <arch2> ...           not valid for these architectures
:cmt    :immediate. <fn>                        <fn> is called when option parsed
:cmt    :code. <source-code>                    <source-code> is executed when option parsed
:cmt    :enumerate. <field> [<value>]           option is one value in <name> enumeration
:cmt    :number. [<fn>] [<default>]             =<n> allowed; call <fn> to check
:cmt    :id. [<fn>]                             =<id> req'd; call <fn> to check
:cmt    :char.[<fn>]                            =<char> req'd; call <fn> to check
:cmt    :file.                                  =<file> req'd
:cmt    :path.                                  =<path> req'd
:cmt    :special. <fn> [<arg_usage_text>]       call <fn> to parse option
:cmt    :optional.                              value is optional
:cmt    :noequal.                               args can't have option '='
:cmt    :argequal. <char>                       args use <char> instead of '='
:cmt    :internal.                              option is undocumented
:cmt    :prefix.                                prefix of a :special. option
:cmt    :usagegrp. <option> <usage text>        group of options that start with <option>
:cmt                                            they are chained together in usage text only
:cmt    :usage. <text>                          English usage text
:cmt    :jusage. <text>                         Japanese usage text
:cmt    :title.                                 English title usage text
:cmt    :jtitle.                                Japanese title usage text
:cmt    :page.                                  text for paging usage message
:cmt    :nochain.                               option isn't chained with other options
:cmt    :timestamp.                             kludge to record "when" an option
:cmt                                            is set so that dependencies
:cmt                                            between options can be simulated
:cmt
:cmt Global macros
:cmt
:cmt    :noequal.                               args can't have option '='
:cmt    :argequal. <char>                       args use <char> instead of '='
:cmt

:cmt    where:
:cmt        <arch>:     i86, 386, axp, any, dbg, qnx, ppc, linux, sparc, haiku

:cmt    Translations are required for the :jtitle. and :jusage. tags
:cmt    if there is no text associated with the tag.


:title. Usage: wccaxp [options] file [options]
:jtitle. �g�p���@: wccaxp [options] file [options]
:target. axp

:title. Usage: wccppc [options] file [options]
:jtitle. �g�p���@: wccppc [options] file [options]
:target. ppc

:title. Usage: wccmps [options] file [options]
:jtitle. �g�p���@: wccmps [options] file [options]
:target. mps

:title. Usage: wcc386 [options] file [options]
:jtitle. �g�p���@: wcc386 [options] file [options]
:target. 386

:title. Usage: wcc [options] file [options]
:jtitle. �g�p���@: wcc [options] file [options]
:target. i86

:title. Options:
:jtitle. �I�v�V����:
:target. bsd linux osx qnx haiku

:title.  :          ( /option is also accepted )
:jtitle. :          ( /option���g�p�ł��܂� )
:target. any
:ntarget. bsd linux osx qnx haiku

:page. (Press return to continue)
:jusage. (���^�[���������ƁC���s���܂�)

:chain. p Preprocess source file
:jusage. p �\�[�X�t�@�C����O�������܂�
:chain. o Optimization
:jusage. o �œK��

:option. h ?
:target. any
:nochain.
:usage. print this message
:jusage. ���̃��b�Z�[�W��\�����܂�

:option. 0
:target. i86
:enumerate. arch_i86
:usage. 8086 instructions
:jusage. 8086 ����

:option. 1
:target. i86
:enumerate. arch_i86
:usage. 186 instructions
:jusage. 186 ����

:option. 2
:target. i86
:enumerate. arch_i86
:usage. 286 instructions
:jusage. 286 ����

:option. 3
:target. i86
:enumerate. arch_i86
:usage. 386 instructions
:jusage. 386 ����

:option. 4
:target. i86
:enumerate. arch_i86
:usage. 386 instructions, optimize for 486
:jusage. 386 ����, 486�p�œK��

:option. 5
:target. i86
:enumerate. arch_i86
:usage. 386 instructions, optimize for Pentium
:jusage. 386 ����, Pentium�p�œK��

:option. 6
:target. i86
:enumerate. arch_i86
:usage. 386 instructions, optimize for Pentium Pro
:jusage. 386 ����, Pentium Pro�p�œK��

:option. 3r 3
:target. 386
:enumerate. arch_386
:usage. 386 register calling conventions
:jusage. 386 ���W�X�^�Ăяo���K��

:option. 3s
:target. 386
:enumerate. arch_386
:usage. 386 stack calling conventions
:jusage. 386 �X�^�b�N�Ăяo���K��

:option. 4r 4
:target. 386
:enumerate. arch_386
:usage. 486 register calling conventions
:jusage. 486 ���W�X�^�Ăяo���K��

:option. 4s
:target. 386
:enumerate. arch_386
:usage. 486 stack calling conventions
:jusage. 486 �X�^�b�N�Ăяo���K��

:option. 5r 5
:target. 386
:enumerate. arch_386
:usage. Pentium register calling conventions
:jusage. Pentium ���W�X�^�Ăяo���K��

:option. 5s
:target. 386
:enumerate. arch_386
:usage. Pentium stack calling conventions
:jusage. Pentium �X�^�b�N�Ăяo���K��

:option. 6r 6
:target. 386
:enumerate. arch_386
:usage. Pentium Pro register calling conventions
:jusage. Pentium Pro ���W�X�^�Ăяo���K��

:option. 6s
:target. 386
:enumerate. arch_386
:usage. Pentium Pro stack calling conventions
:jusage. Pentium Pro �X�^�b�N�Ăяo���K��

:option. aa
:target. any
:usage. allow non const initializers for local aggregates or unions
:jusage.

:usagegrp. ad Make Dependency Information

:option. ad
:target. any
:file.
:optional.
:usage. generate make style automatic dependency file
:jusage.

:option. adbs
:target. any
:usage. force path separators to '\\' in auto-depend file
:jusage.

:option. add
:target. any
:file.
:optional.
:usage. specify first dependency in make style auto-depend file
:jusage.

:option. adfs
:target. any
:usage. force path separators to '/' in auto-depend file
:jusage.

:option. adhp
:target. any
:file.
:optional.
:usage. specify default path for headers without one
:jusage.

:option. adt
:target. any
:file.
:optional.
:usage. specify target in make style auto-depend file
:jusage.

:option. ai
:target. any
:usage. turn off type checking on static initialization
:jusage.

:option. aq
:target. any
:usage. turn off qualifier mismatch warning for const/volatile
:jusage.

:option. as
:target. axp
:usage. assume short integers are aligned
:jusage. short ���������񂵂Ă���Ɖ��肵�܂�

:usagegrp. b Application type

:option. bc
:target. any
:usage. build target is a console application
:jusage. �\�z�^�[�Q�b�g�̓R���\�[����A�v���P�[�V�����ł�

:option. bd
:target. any
:usage. build target is a dynamic link library (DLL)
:jusage. �\�z�^�[�Q�b�g�̓_�C�i�~�b�N������N����C�u�����ł�(DLL)

:option. bg
:target. any
:usage. build target is a GUI application
:jusage. �\�z�^�[�Q�b�g��GUI�A�v���P�[�V�����ł�

:option. bm
:target. any
:usage. build target is a multi-thread environment
:jusage. �\�z�^�[�Q�b�g�̓}���`�X���b�h���ł�

:option. br
:target. 386 axp ppc
:usage. build target uses DLL version of C/C++ run-time library
:jusage. �\�z�^�[�Q�b�g��DLL�ł�C/C++���s�����C�u�������g�p���܂�

:option. bt
:target. any
:nochain.
:id.
:optional.
:usage. build target is operating system <id>
:jusage. �\�z�^�[�Q�b�g�̓I�y���[�e�B���O��V�X�e�� <id>

:option. bw
:target. any
:usage. build target is a default windowing application
:jusage. �\�z�^�[�Q�b�g�̓f�t�H���g��E�B���h�E��A�v���P�[�V�����ł�

:usagegrp. d Debugging Information

:option. d0
:target. any
:enumerate. debug_info
:timestamp.
:usage. none
:jusage. �f�o�b�O���͂���܂���

:option. d1
:target. any
:enumerate. debug_info
:timestamp.
:usage. only line numbers
:jusage. �s�ԍ��f�o�b�O���

:option. d1+
:target. any
:enumerate. debug_info
:timestamp.
:usage. only line numbers
:jusage. �s�ԍ��f�o�b�O���

:option. d2
:target. any
:enumerate. debug_info
:timestamp.
:usage. symbolic information
:jusage. ���S�V���{���f�o�b�O���

:option. d2~
:target. any
:enumerate. debug_info
:timestamp.
:usage. -d2 but without type names
:jusage. �^���Ȃ��̊��S�V���{���f�o�b�O���

:option. d3
:target. any
:enumerate. debug_info
:timestamp.
:usage. symbolic information with unreferenced type names
:jusage. �Q�Ƃ���Ă��Ȃ��^�����܂ފ��S�V���{���f�o�b�O���

:option. d9
:target. any
:enumerate. debug_info
:timestamp.
:usage. full symbolic information
:jusage.

:option. d+
:target. any
:nochain.
:special. scanDefinePlus
:usage. allow extended -d macro definitions
:jusage. �g�����ꂽ -d �}�N����`�������܂�

:option. db
:target. any
:nochain.
:prefix.
:usage. generate browsing information
:jusage. �u���E�Y���𐶐����܂�

:option. d
:target. any
:nochain.
:special. scanDefine <name>[=text]
:usage. same as #define name [text] before compilation
:jusage. �R���p�C���O�� #define name [text] �Ɠ���

:option. ec
:target. any
:nochain.
:internal.
:usage. emit code coverage gear
:jusage.

:usagegrp. ec Default calling convention

:option. ecc
:target. i86 386
:enumerate. intel_call_conv
:usage. __cdecl
:jusage.

:option. ecd
:target. i86 386
:enumerate. intel_call_conv
:usage. __stdcall
:jusage.

:option. ecf
:target. i86 386
:enumerate. intel_call_conv
:usage. __fastcall
:jusage.

:option. eco
:target. i86 386
:enumerate. intel_call_conv
:internal.
:usage. _Optlink
:jusage.

:option. ecp
:target. i86 386
:enumerate. intel_call_conv
:usage. __pascal
:jusage.

:option. ecr
:target. i86 386
:enumerate. intel_call_conv
:usage. __fortran
:jusage.

:option. ecs
:target. i86 386
:enumerate. intel_call_conv
:usage. __syscall
:jusage.

:option. ecw
:target. i86 386
:enumerate. intel_call_conv
:usage. __watcall (default)
:jusage.

:option. eb
:target. axp ppc mps
:usage. emit big-endian object files
:jusage.

:option. el
:target. axp ppc mps
:usage. emit little-endian object files
:jusage.

:option. ee
:target. i86 386
:usage. call epilogue hook routine
:jusage. �G�s���[�O��t�b�N���[�`�����Ăяo���܂�

:option. ef
:target. any
:usage. use full path names in error messages
:jusage. �G���[���b�Z�[�W�Ɋ��S�p�X�����g�p���܂�

:option. ei
:target. any
:enumerate. enum_size
:usage. force enum base type to use at least an int
:jusage. enum�^�̃x�[�X�^�Ƃ���int�^�ȏ�̑傫�����g�p���܂�

:option. em
:target. any
:enumerate. enum_size
:usage. force enum base type to use minimum integral type
:jusage. enum�^�̃x�[�X�^�Ƃ��čŏ��̐����^���g�p���܂�

:option. en
:target. any
:usage. emit routine names in the code segment
:jusage. ���[�`�������R�[�h�Z�O�����g�ɏo�͂��܂�

:option. eoc
:target. axp ppc mps
:usage. emit COFF object files
:jusage.

:option. eoe
:target. axp ppc mps
:usage. emit ELF object files
:jusage.

:option. eoo
:internal.
:usage. emit OMF object files
:jusage.

:option. ep
:target. any
:number. checkPrologSize 0
:usage. call prologue hook routine with <num> stack bytes available
:jusage. <num>�o�C�g�̃X�^�b�N���g�p����v�����[�O��t�b�N����[�`�����Ăяo���܂�

:option. eq
:target. any
:immediate. handleOptionEQ
:usage. do not display error messages (but still write to .err file)
:jusage. �G���[���b�Z�[�W��\�����܂���(������.err�t�@�C���ɂ͏������݂܂�)

:option. et
:target. 386
:usage. emit Pentium profiling code
:jusage. Pentium�v���t�@�C�����O��R�[�h�𐶐����܂�

:option. ez
:target. 386
:usage. generate PharLap EZ-OMF object files
:jusage. PharLap EZ-OMF�I�u�W�F�N�g��t�@�C���𐶐����܂�

:option. e
:target. any
:number. checkErrorLimit
:usage. set limit on number of error messages
:jusage. �G���[���b�Z�[�W���̐�����ݒ肵�܂�

:option. fh
:target. any
:file.
:optional.
:timestamp.
:usage. use pre-compiled header (PCH) file
:jusage. �v���R���p�C����w�b�_�[(PCH)���g�p���܂�

:option. fhq
:target. any
:file.
:optional.
:timestamp.
:usage. do not display pre-compiled header activity warnings
:jusage.

:option. fi
:target. any
:file.
:usage. force <file> to be included
:jusage.

:option. fip
:target. any
:file.
:optional.
:usage. automatic inclusion of <file> instead of _preincl.h (default)
:jusage.

:option. fo
:target. any
:file.
:optional.
:usage. set object file name
:jusage. �I�u�W�F�N�g�t�@�C����ݒ肵�܂�

:option. fr
:target. any
:file.
:optional.
:usage. set error file name
:jusage. �G���[��t�@�C������ݒ肵�܂�

:option. ft
:target. any
:usage. check for truncated versions of file names
:jusage. �؂�l�߂��t�@�C�������`�F�b�N���܂�

:option. fti
:target. any
:usage. print informational message when opening include file
:jusage.

:option. fx
:target. any
:usage. do not check for truncated versions of file names
:jusage. �؂�l�߂��t�@�C�������`�F�b�N���܂���

:usagegrp. fp Generate Floating-point code

:option. fpc
:target. i86 386
:nochain.
:enumerate. intel_fpu_model
:usage. calls to floating-point library
:jusage. ���������_���C�u�������Ăяo���܂�

:option. fpi
:target. i86 386
:nochain.
:enumerate. intel_fpu_model
:usage. inline 80x87 instructions with emulation
:jusage. �G�~�����[�V�����t���C�����C��80x87����

:option. fpi87
:target. i86 386
:nochain.
:enumerate. intel_fpu_model
:usage. inline 80x87 instructions
:jusage. �C�����C��80x87����

:option. fp2 fp287
:target. i86 386
:enumerate. intel_fpu_level
:usage. 80287 FPU code
:jusage. 80287���������_�R�[�h�𐶐����܂�

:option. fp3 fp387
:target. i86 386
:enumerate. intel_fpu_level
:usage. 80387 FPU code
:jusage. 80387���������_�R�[�h�𐶐����܂�

:option. fp5
:target. i86 386
:enumerate. intel_fpu_level
:usage. 80387 FPU code optimize for Pentium
:jusage.

:option. fp6
:target. i86 386
:enumerate. intel_fpu_level
:usage. 80387 FPU code optimize for Pentium Pro
:jusage.

:option. fpr
:target. i86 386
:nochain.
:usage. generate backward compatible 80x87 code
:jusage. �o�[�W����9.0�ȑO�ƌ݊���80x87�R�[�h�𐶐����܂�

:option. fpd
:target. i86 386
:nochain.
:usage. enable Pentium FDIV bug check
:jusage. Pentium FDIV�`�F�b�N�����܂�

:option. g
:target. i86 386
:id.
:usage. set code group name
:jusage. �R�[�h��O���[�v����ݒ肵�܂�

:usagegrp. h Debugging Information format

:option. hw
:target. i86 386 
:enumerate. dbg_output
:usage. generate Watcom debugging information
:jusage. Watcom�f�o�b�O���𐶐����܂�

:option. hd
:target. any
:enumerate. dbg_output
:usage. generate DWARF debugging information
:jusage. DWARF�f�o�b�O���𐶐����܂�

:option. hc
:target. any
:enumerate. dbg_output
:usage. generate Codeview debugging information
:jusage. Codeview�f�o�b�O���𐶐����܂�

:option. i
:target. any
:path.
:usage. add directory to list of include directories
:jusage. �C���N���[�h�E�f�B���N�g���̃��X�g��ǉ����܂�

:option. j
:target. any
:usage. change char default from unsigned to signed
:jusage. char�^�̃f�t�H���g��unsigned����signed�ɕύX���܂�

:usagegrp. m Memory model
:jusage. �������E���f��

:option. mc
:target. i86 386
:enumerate. mem_model
:usage. compact - small code/large data
:jusage. �R���p�N�g�����������f��(�X���[����R�[�h/���[�W��f�[�^)

:option. mf
:target. 386
:enumerate. mem_model
:usage. flat - small code/small data assuming CS=DS=SS=ES
:jusage. �t���b�g�����������f��(�X���[����R�[�h/CS=DS=SS=ES�����肵���X���[����f�[�^)

:option. mh
:target. i86
:enumerate. mem_model
:usage. huge - large code/huge data
:jusage. �q���[�W�����������f��(���[�W��R�[�h/�q���[�W��f�[�^)

:option. ml
:target. i86 386
:enumerate. mem_model
:usage. large - large code/large data
:jusage. ���[�W�����������f��(���[�W��R�[�h/���[�W��f�[�^)

:option. mm
:target. i86 386
:enumerate. mem_model
:usage. medium - large code/small data
:jusage. �~�f�B�A�������������f��(���[�W��R�[�h/�X���[����f�[�^)

:option. ms
:target. i86 386
:enumerate. mem_model
:usage. small - small code/small data (defaul)
:jusage. �X���[�������������f��(�X���[����R�[�h/�X���[����f�[�^)

:option. nc
:target. i86 386
:id.
:usage. set code class name
:jusage. �R�[�h��N���X����ݒ肵�܂�

:option. nd
:target. i86 386
:id.
:usage. set data segment name
:jusage. �f�[�^��Z�O�����g����ݒ肵�܂�

:option. nm
:target. any
:file.
:usage. set module name
:jusage. ���W���[������ݒ肵�܂�

:option. nt
:target. i86 386
:id.
:usage. set name of text segment
:jusage. �e�L�X�g��Z�O�����g����ݒ肵�܂�

:option. oa
:target. any
:usage. relax aliasing constraints
:jusage. �G�C���A�X�̐�����ɘa���܂�

:option. ob
:target. any
:usage. enable branch prediction
:jusage. ����\���ɂ������R�[�h�𐶐����܂�

:option. oc
:target. i86 386
:usage. disable <call followed by return> to <jump> optimization
:jusage. <call followed by return>����<jump>�̍œK���𖳌��ɂ��܂�

:option. od
:target. any
:enumerate. opt_level
:timestamp.
:usage. disable all optimizations
:jusage. ���ׂĂ̍œK���𖳌��ɂ��܂�

:option. oe
:target. any
:number. checkOENumber 100
:usage. expand user functions inline (<num> controls max size)
:jusage. ���[�U�֐����C�����C���W�J���܂�(<num>�͍ő廲�ނ𐧌䂵�܂�)

:option. of
:target. i86 386
:usage. generate traceable stack frames as needed
:jusage. �K�v�ɉ����ăg���[�X�\�ȃX�^�b�N��t���[���𐶐����܂�

:option. of+
:target. i86 386
:usage. always generate traceable stack frames
:jusage. ��Ƀg���[�X�\�ȃX�^�b�N��t���[���𐶐����܂�

:option. oh
:target. any
:usage. enable expensive optimizations (longer compiles)
:jusage. �œK�����J��Ԃ��܂�(�R���p�C���������Ȃ�܂�)

:option. oi
:target. any
:usage. expand intrinsic functions inline
:jusage. �g���݊֐����C�����C���W�J���܂�

:option. ok
:target. any
:usage. include prologue/epilogue in flow graph
:jusage. �v�����[�O�ƃG�s���[�O���t���[����\�ɂ��܂�

:option. ol
:target. any
:usage. enable loop optimizations
:jusage. ���[�v�œK�����\�ɂ��܂�

:option. ol+
:target. any
:usage. enable loop unrolling optimizations
:jusage. ���[�v�E�A�����[�����O�Ń��[�v�œK�����\�ɂ��܂�

:option. om
:target. i86 386
:usage. generate inline code for math functions
:jusage. �Z�p�֐����C�����C����80x87�R�[�h�œW�J���Đ������܂�

:option. on
:target. any
:usage. allow numerically unstable optimizations
:jusage. ���l�I�ɂ��s���m�ɂȂ邪��荂���ȍœK�����\�ɂ��܂�

:option. oo
:target. any
:usage. continue compilation if low on memory
:jusage. ������������Ȃ��Ȃ��Ă��R���p�C�����p�����܂�

:option. op
:target. any
:usage. generate consistent floating-point results
:jusage. ��т������������_�v�Z�̌��ʂ𐶐����܂�

:option. or
:target. any
:usage. reorder instructions for best pipeline usage
:jusage. �œK�ȃp�C�v���C�����g�p���邽�߂ɖ��߂���בւ��܂�

:option. os
:target. any
:enumerate. opt_size_time
:timestamp.
:usage. favor code size over execution time in optimizations
:jusage. ���s���Ԃ��R�[�h�T�C�Y�̍œK����D�悵�܂�

:option. ot
:target. any
:enumerate. opt_size_time
:timestamp.
:usage. favor execution time over code size in optimizations
:jusage. �R�[�h�T�C�Y�����s���Ԃ̍œK����D�悵�܂�

:option. ou
:target. any
:usage. all functions must have unique addresses
:jusage. ���ׂĂ̊֐��͂��ꂼ��ŗL�̃A�h���X��K�������܂�

:option. ox
:target. i86 386
:enumerate. opt_level
:timestamp.
:usage. equivalent to -obmiler -s
:jusage. -obmiler -s�Ɠ���

:option. ox
:target. axp ppc mps
:enumerate. opt_level
:timestamp.
:usage. equivalent to -obiler -s
:jusage. -obiler -s�Ɠ���

:option. oz
:target. any
:usage. NULL points to valid memory in the target environment
:jusage. NULL�́A�^�[�Q�b�g�����̗L���ȃ��������w���܂�

:option. pil
:target. any
:nochain.
:usage. preprocessor ignores #line directives
:jusage.

:option. p
:target. any
:char.
:internal.
:usage. set preprocessor delimiter to something other than '#'
:jusage. �v���v���Z�b�T�̋�؂�L����'#'�ȊO�̉����ɐݒ肵�܂�

:option. pl
:target. any
:usage. insert #line directives
:jusage. #line�[�����߂�}�����܂�

:option. pc
:target. any
:usage. preserve comments
:jusage. �R�����g���c���܂�

:option. pw
:target. any
:number. checkPPWidth
:usage. wrap output lines at <num> columns. Zero means no wrap.
:jusage. �o�͍s��<num>���Ő܂�Ԃ��܂�. 0�͐܂�Ԃ��܂���.

:option. q
:target. any
:usage. operate quietly (display only error messages)
:jusage. �����b�Z�[�W���[�h�œ��삵�܂�(�G���[���b�Z�[�W�̂ݕ\������܂�)

:option. r
:target. i86 386
:usage. save/restore segment registers across calls
:jusage. �֐��Ăяo���̑O��ŃZ�O�����g���W�X�^��ޔ�/���X�g�A���܂�

:option. ri
:target. i86 386
:usage. return chars and shorts as ints
:jusage. �S�Ă̊֐��̈����Ɩ߂�l��int�^�ɕϊ����܂�

:option. s
:target. any
:usage. remove stack overflow checks
:jusage. �X�^�b�N�I�[�o�t���[�E�`�F�b�N���폜���܂�

:option. sg
:target. i86 386
:usage. generate calls to grow the stack
:jusage. �X�^�b�N�𑝉�����Ăяo���𐶐����܂�

:option. si
:target. axp
:usage. generate calls to initialize local storage
:jusage. ���[�J���������������������Ăяo���𐶐����܂�

:option. st
:target. i86 386
:usage. touch stack through SS first
:jusage. �܂��ŏ���SS��ʂ��ăX�^�b�N�E�^�b�`���܂�

:option. tp
:target. any
:id.
:usage. set #pragma on( <id> )
:jusage. #pragma on( <id> )��ݒ肵�܂�

:option. u
:target. any
:special. scanUndefine [=<name>]
:usage. undefine macro name
:jusage. �}�N�����𖢒�`�ɂ��܂�

:option. v
:target. any
:usage. output function declarations to .def file
:jusage. .def�t�@�C���Ɋ֐��錾���o�͂��܂�

:option. vcap
:target. 386 axp
:usage. VC++ compatibility: alloca allowed in argument lists
:jusage. VC++ �݊���: �������X�g�̒���alloca���g�p�ł��܂�

:usagegrp. w Warning control

:option. w
:target. any
:enumerate. warn_level
:number. checkWarnLevel
:usage. set warning level number
:jusage. �x�����x���ԍ���ݒ肵�܂�

:option. wcd
:target. any
:number.
:multiple.
:usage. disable warning message <num>
:jusage. �x������: �x�����b�Z�[�W<num>���֎~���܂�

:option. wce
:target. any
:number.
:multiple.
:usage. enable warning message <num>
:jusage. �x������: �x�����b�Z�[�W <num> �̕\�������܂�

:option. we
:target. any
:usage. treat all warnings as errors
:jusage. ���ׂĂ̌x�����G���[�Ƃ��Ĉ����܂�

:option. wo
:target. i86
:usage. warn about problems with overlaid code
:jusage.

:option. wpx
:target. any
:nochain.
:internal.
:usage. warn about global prototypes not defined in header file
:jusage.

:option. wx
:target. any
:enumerate. warn_level
:usage. set warning level to maximum setting
:jusage. �x�����x�����ő�ݒ�ɂ��܂�

:option. x
:target. any
:nochain.
:usage. ignore all ..INCLUDE environment variables
:jusage. ignore all ..INCLUDE environment variables

:option. xgv
:target. 386
:nochain.
:internal.
:usage. indexed global variables
:jusage. �C���f�b�N�X�t���O���[�o���ϐ�

:option. xbsa
:target. any
:nochain.
:internal.
:usage. do not align segments if at all possible
:jusage. 

:option. xd
:target. axp
:internal.
:enumerate. exc_level
:usage. use default exception handler
:jusage.

:option. xx
:target. any
:nochain.
:usage. ignore default directories for file search (.,../h,../c,...)
:jusage. ignore default directories for file search (.,../h,../c,...)

:option. z\a
:target. any
:enumerate. iso
:usage. disable extensions (i.e., accept only ISO/ANSI C)
:jusage. �g���@�\���g�p�s�ɂ��܂�(�܂�, ISO/ANSI C�̂ݎ󂯕t���܂�)

:option. z\A
:target. any
:enumerate. iso
:usage. disable all extensions (strict ISO/ANSI C)
:jusage. �g���@�\���g�p�s�ɂ��܂�(�܂�, ISO/ANSI C�̂ݎ󂯕t���܂�)

:option. za99
:target. any
:internal.
:enumerate. iso
:usage. disable extensions (i.e., accept only ISO/ANSI C99)
:jusage. �g���@�\���g�p�s�ɂ��܂�(�܂�, ISO/ANSI C99�̂ݎ󂯕t���܂�)

:option. zam
:target. any
:usage. disable all predefined non-ISO extension macros
:jusage. disable all predefined non-ISO extension macros

:option. zc
:target. i86 386
:usage. place const data into the code segment
:jusage. ���e������������R�[�h�Z�O�����g�ɓ���܂�

:option. zdf
:target. i86 386
:enumerate. ds_peg
:usage. DS floats (i.e. not fixed to DGROUP)
:jusage. DS�𕂓��ɂ��܂�(�܂�DGROUP�ɌŒ肵�܂���)

:option. zdp
:target. i86 386
:enumerate. ds_peg
:usage. DS is pegged to DGROUP
:jusage. DS��DGROUP�ɌŒ肵�܂�

:option. zdl
:target. 386
:usage. load DS directly from DGROUP
:jusage. DGROUP����DS�ɒ��ڃ��[�h���܂�

:option. ze
:target. any
:enumerate. iso
:usage. enable extensions (i.e., near, far, export, etc.)
:jusage. �g���@�\���g�p�\�ɂ��܂�(�܂�, near, far, export, ��.)

:option. zev
:target. any
:enumerate. iso
:usage. enable arithmetic on void derived types
:jusage.

:option. zfw
:target. i86
:usage. generate FWAIT instructions on 386 and later
:jusage.

:option. zfw
:target. 386
:usage. generate FWAIT instructions
:jusage.

:option. zff
:target. i86 386
:enumerate. fs_peg
:usage. FS floats (i.e. not fixed to a segment)
:jusage. FS�𕂓��ɂ��܂�(�܂�, 1�̃Z�O�����g�ɌŒ肵�܂���)

:option. zfp
:target. i86 386
:enumerate. fs_peg
:usage. FS is pegged to a segment
:jusage. FS��1�̃Z�O�����g�ɌŒ肵�܂�

:option. zgf
:target. i86 386
:enumerate. gs_peg
:usage. GS floats (i.e. not fixed to a segment)
:jusage. GS�𕂓��ɂ��܂�(�܂�, 1�̃Z�O�����g�ɌŒ肵�܂���)

:option. zgp
:target. i86 386
:enumerate. gs_peg
:usage. GS is pegged to a segment
:jusage. GS��1�̃Z�O�����g�ɌŒ肵�܂�

:option. zg
:target. any
:usage. generate function prototypes using base types
:jusage. ��{�^���g�p�����֐��v���g�^�C�v�𐶐����܂�

:usagegrp. zk Multi-byte/Unicode character support

:option. zk0 zk
:target. any
:enumerate. char_set
:usage. Kanji
:jusage. 2�o�C�g�����T�|�[�g: ���{��

:option. zk1
:target. any
:enumerate. char_set
:usage. Chinese/Taiwanese
:jusage. 2�o�C�g�����T�|�[�g: ������/��p��

:option. zk2
:target. any
:enumerate. char_set
:usage. Korean
:jusage. 2�o�C�g�����T�|�[�g: �؍���

:option. zk0u
:target. any
:enumerate. char_set
:usage. translate double-byte Kanji to Unicode
:jusage. 2�o�C�g������Unicode�ɕϊ����܂�

:option. zkl
:target. any
:enumerate. char_set
:usage. local installed language
:jusage. 2�o�C�g�����T�|�[�g: ���[�J���ɃC���X�g�[�����ꂽ����

:option. zku8
:target. any
:enumerate. char_set
:usage. Unicode UTF-8
:jusage.

:option. zku
:target. any
:enumerate. char_set
:number.
:usage. load Unicode translate table for specified code page
:jusage. �w�肵���R�[�h�y�[�W��Unicode�ϊ��e�[�u�������[�h���܂�

:option. zl
:target. any
:usage. remove default library information
:jusage. �f�t�H���g����C�u���������폜���܂�

:option. zld
:target. any
:usage. remove file dependency information
:jusage. �t�@�C���ˑ������폜���܂�

:option. zlf
:target. any
:usage. always generate default library information
:jusage. �f�t�H���g����C�u����������ɐ������܂�

:option. zls
:target. any
:usage. remove automatically inserted symbols
:jusage.

:option. zm
:target. any
:usage. emit functions in separate segments
:jusage. �e�֐���ʂ̃Z�O�����g�ɓ���܂�

:option. zp
:target. any
:number. checkPacking
:usage. pack structure members with alignment {1,2,4,8,16}
:jusage. �\���̃����o�[��{1,2,4,8,16}�ɐ��񂵂ăp�b�N���܂�

:option. zpw
:target. any
:usage. output warning when padding is added in a class
:jusage. �N���X�Ƀp�f�B���O���ǉ����ꂽ�Ƃ��Ɍx�����܂�

:option. zps
:target. axp
:usage. always align structs on qword boundaries
:jusage.

:option. zq
:target. any
:usage. operate quietly (display only error messages)
:jusage. �����b�Z�[�W���[�h�œ��삵�܂�(�G���[���b�Z�[�W�̂ݕ\������܂�)

:option. zro
:target. any
:usage. omit floating point rounding calls (non ANSI)
:jusage.

:option. zri
:target. 386
:usage. inline floating point rounding calls
:jusage.

:option. zs
:target. any
:usage. syntax check only
:jusage. �\���`�F�b�N�݂̂��s���܂�

:option. zt
:target. i86 386
:number. CmdX86CheckThreshold 256
:usage. far data threshold (i.e., larger objects go in far memory)
:jusage. far�f�[�^�~���l(�܂�, �~���l���傫���I�u�W�F�N�g��far�������ɒu���܂�)

:option. zu
:target. i86 386
:usage. SS != DGROUP (i.e., do not assume stack is in data segment)
:jusage. SS != DGROUP (�܂�, �X�^�b�N���f�[�^�Z�O�����g�ɂ���Ɖ��肵�܂���)

:option. z\w
:target. i86
:enumerate. win
:usage. generate code for Microsoft Windows
:jusage. Microsoft Windows�p�̃R�[�h�𐶐����܂�

:option. z\W
:target. i86
:enumerate. win
:usage. more efficient Microsoft Windows entry sequences
:jusage. �����ʓI��Microsoft Windows�G���g���R�[�h��𐶐����܂�

:option. zw
:target. 386
:enumerate. win
:usage. generate code for Microsoft Windows
:jusage. Microsoft Windows�p�̃R�[�h�𐶐����܂�

:option. z\ws
:target. i86
:enumerate. win
:usage. generate code for Microsoft Windows with smart callbacks
:jusage. �X�}�[�g��R�[���o�b�N������Microsoft Windows�p�R�[�h�𐶐����܂�

:option. z\Ws
:target. i86
:enumerate. win
:usage. generate code for Microsoft Windows with smart callbacks
:jusage. �X�}�[�g��R�[���o�b�N������Microsoft Windows�p�R�[�h�𐶐����܂�

:option. zz
:target. 386
:usage. remove "@size" from __stdcall function names (10.0 compatible)
:jusage. "@size"��__stdcall�֐�������폜���܂�(10.0�Ƃ̌݊���)
