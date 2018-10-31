/* sed.h -- types and constants for the stream editor */

/* data area sizes used by both modules */
#define MAXBUF          8192    /* current line buffer size */
#define MAXAPPENDS      20      /* maximum number of appends */
#define MAXTAGS         9       /* tagged patterns are \1 to \9 */

/* constants for compiled-command representation */
#define EQCMD   0x01    /* = -- print current line number               */
#define ACMD    0x02    /* a -- append text after current line          */
#define BCMD    0x03    /* b -- branch to label                         */
#define CCMD    0x04    /* c -- change current line                     */
#define DCMD    0x05    /* d -- delete all of pattern space             */
#define CDCMD   0x06    /* D -- delete first line of pattern space      */
#define GCMD    0x07    /* g -- copy hold space to pattern space        */
#define CGCMD   0x08    /* G -- append hold space to pattern space      */
#define HCMD    0x09    /* h -- copy pattern space to hold space        */
#define CHCMD   0x0A    /* H -- append hold space to pattern space      */
#define ICMD    0x0B    /* i -- insert text before current line         */
#define LCMD    0x0C    /* l -- print pattern space in escaped form     */
#define NCMD    0x0D    /* n -- get next line into pattern space        */
#define CNCMD   0x0E    /* N -- append next line to pattern space       */
#define PCMD    0x0F    /* p -- print pattern space to output           */
#define CPCMD   0x10    /* P -- print first line of pattern space       */
#define QCMD    0x11    /* q -- exit the stream editor                  */
#define RCMD    0x12    /* r -- read in a file after current line       */
#define SCMD    0x13    /* s -- regular-expression substitute           */
#define TCMD    0x14    /* t -- branch on last substitute successful    */
#define CTCMD   0x15    /* T -- branch on last substitute failed        */
#define WCMD    0x16    /* w -- write pattern space to file             */
#define CWCMD   0x17    /* W -- write first line of pattern space       */
#define XCMD    0x18    /* x -- exhange pattern and hold spaces         */
#define YCMD    0x19    /* y -- transliterate text                      */

#define CHARSETSIZE     (256 / 8)

#define SETCHARSET(a,c)     (a[(unsigned char)c >> 3] |= bits[c & 7])
#define TESTCHARSET(a,c)    (a[(unsigned char)c >> 3] & bits[c & 7])

typedef struct cmd_t    sedcmd;         /* use this name for declarations */
struct cmd_t {                          /* compiled-command representation */
    char                *addr1;         /* first address for command */
    char                *addr2;         /* second address for command */
    union {
        char            *lhs;           /* s command lhs */
        sedcmd          *link;          /* label link */
    } u;
    unsigned char       command;        /* command code */
    char                *rhs;           /* s command replacement string */
    FILE                *fout;          /* associated output file descriptor */
    struct {
        unsigned        nthone  :11;    /* if !0 only nth replace */
        unsigned        allbut  : 1;    /* was negation specified? */
        unsigned        global  : 1;    /* was g postfix specified? */
        unsigned        print   : 2;    /* was p postfix specified? */
        unsigned        inrange : 1;    /* in an address range? */
    } flags;
};

#define BAD     ((char *)-1LL)          /* guaranteed not a string ptr */

/* address and regular expression compiled-form markers */
#define STAR    0x01    /* marker for Kleene star */
#define CCHR    0x02    /* non-newline character to be matched follows */
#define CDOT    0x04    /* dot wild-card marker */
#define CCL     0x06    /* character class follows */
#define CNL     0x08    /* match line start */
#define CDOL    0x0A    /* match line end */
#define CBRA    0x0C    /* tagged pattern start marker */
#define CKET    0x0E    /* tagged pattern end marker */
#define CBACK   0x10    /* backslash-digit pair marker */
#define CLNUM   0x12    /* numeric-address index follows */
#define CEND    0x14    /* symbol for end-of-source */
#define CEOF    0x16    /* end-of-field mark */
#define MTYPE   0x20    /* multiple counts |'d into RE      \{...\}    */

extern void     execute( const char *file ); /* In sed.c */

/* The following are in sedcomp.c */
                                        /* main data areas */
extern char             linebuf[];      /* current-line buffer */
extern sedcmd           cmds[];         /* hold compiled commands */
extern long             linenum[];      /* numeric-addresses table */

                                        /* miscellaneous shared variables */
extern bool                 nflag;      /* -n option flag */
extern int                  eargc;      /* scratch copy of argument count */
extern unsigned char const  bits[];     /* the bits table */

/* sed.h ends here */

