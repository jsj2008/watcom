.func jstrspn _fjstrspn
.synop begin
#include <jstring.h>
size_t jstrspn( const JCHAR *str, const JCHAR *charset );
.ixfunc2 '&Jstring' &funcb
.ixfunc2 '&Jsearch' &funcb
.if &farfnc eq 1 .do begin
size_t __far _fjstrspn( const JCHAR __far *str,
                        const JCHAR __far *charset );
.ixfunc2 '&Jstring' &ffunc
.ixfunc2 '&Jsearch' &ffunc
.do end
.synop end
.desc begin
.if &farfnc eq 0 .do begin
The
.id &funcb.
function computes
.do end
.el .do begin
The
.id &funcb.
and
.id &ffunc.
functions compute
.do end
the length of the initial segment of the Kanji string pointed to by
.arg str
which consists of single- and double-byte characters from the Kanji
string pointed to by
.arg charset
.ct .li .
The terminating null character is not considered to be part of
.arg charset
.ct .li .
.im ffarparm
.desc end
.return begin
.if &farfnc eq 0 .do begin
The
.id &funcb.
function returns
.do end
.el .do begin
The
.id &funcb.
and
.id &ffunc.
functions return
.do end
the length of the segment.
.return end
.see begin
.seelist jstrspn jstrcspn strspn strcspn
.see end
.exmp begin
#include <stdio.h>
#include <jstring.h>

void main()
  {
    printf( "%d\n", jstrspn( "out to lunch", "aeiou" ) );
    printf( "%d\n", jstrspn( "out to lunch", "xyz" ) );
  }
.exmp output
2
0
.exmp end
.class WATCOM
.system
