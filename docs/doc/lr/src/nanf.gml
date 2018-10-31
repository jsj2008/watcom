.func nan
.synop begin
#include <math.h>
float nanf( const char *str );
double nan( const char *str );
long double nanl( const char *str );
.ixfunc2 '&Math' &funcb
.synop end
.desc begin
The
.id &funcb.
function returns not-a-number, or NAN.  The argument
.arg str
is ignored.
.desc end
.return begin
The proper not-a-number value.
.return end
.exmp begin
#include <stdio.h>
#include <math.h>

void main()
  {
    printf( "%f\n", nan("") );
  }
.exmp output
nan
.exmp end
.class ISO C99
.system
