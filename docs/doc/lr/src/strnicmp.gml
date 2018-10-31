.func strnicmp _strnicmp _fstrnicmp _wcsnicmp _mbsnicmp _fmbsnicmp
.synop begin
#include <string.h>
int strnicmp( const char *s1,
              const char *s2,
              size_t len );
.ixfunc2 '&String' &funcb
.ixfunc2 '&Compare' &funcb
.if &'length(&_func.) ne 0 .do begin
int _strnicmp( const char *s1,
               const char *s2,
               size_t len );
.ixfunc2 '&String' &_func
.ixfunc2 '&Compare' &_func
.do end
.if &farfnc eq 1 .do begin
int _fstrnicmp( const char __far *s1,
                const char __far *s2,
                size_t len );
.ixfunc2 '&String' &ffunc
.ixfunc2 '&Compare' &ffunc
.do end
.if &'length(&wfunc.) ne 0 .do begin
#include <wchar.h>
int _wcsnicmp( const wchar_t *s1,
               const wchar_t *s2,
               size_t len );
.ixfunc2 '&String' &wfunc
.ixfunc2 '&Compare' &wfunc
.ixfunc2 '&Wide' &wfunc
.do end
.if &'length(&mfunc.) ne 0 .do begin
#include <mbstring.h>
int _mbsnicmp( const unsigned char *s1,
               const unsigned char *s2,
               size_t n );
.ixfunc2 '&String' &mfunc
.ixfunc2 '&Compare' &mfunc
.ixfunc2 '&Multibyte' &mfunc
.do end
.if &'length(&fmfunc.) ne 0 .do begin
int _fmbsnicmp( const unsigned char __far *s1,
                const unsigned char __far *s2,
                size_t n );
.ixfunc2 '&String' &fmfunc
.ixfunc2 '&Compare' &fmfunc
.ixfunc2 '&Multibyte' &fmfunc
.do end
.synop end
.desc begin
The
.id &funcb.
function compares, without case sensitivity, the string
pointed to by
.arg s1
to the string pointed to by
.arg s2
.ct , for at most
.arg len
characters.
.im ansiconf
.im farparm
.im widefun1
.im mbsffunc
.desc end
.return begin
The
.id &funcb.
function returns an integer less than, equal to, or greater
than zero, indicating that the string pointed to by
.arg s1
is less than, equal to, or greater than the string pointed to by
.arg s2
.ct .li .
.return end
.see begin
.seelist strnicmp strcmp stricmp strncmp
.see end
.exmp begin
#include <stdio.h>
#include <string.h>

void main()
  {
    printf( "%d\n", strnicmp( "abcdef", "ABCXXX", 10 ) );
    printf( "%d\n", strnicmp( "abcdef", "ABCXXX",  6 ) );
    printf( "%d\n", strnicmp( "abcdef", "ABCXXX",  3 ) );
    printf( "%d\n", strnicmp( "abcdef", "ABCXXX",  0 ) );
  }
.exmp output
-20
-20
0
0
.exmp end
.class WATCOM
.system
