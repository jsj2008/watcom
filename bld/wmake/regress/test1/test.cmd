@echo off
echo # ===========================
echo # Multiple Dependents Test
echo # ===========================
if .%1 == . goto usage

set OWVERBOSE=0
echo # ---------------------------
echo # TEST 1
echo # ---------------------------
%1 -h -f create
if exist err1.out del err1.out
echo. >err1.out
%1 -h -f maketst1 -l err1.out > tst1.out
diff -b tst1.out tst1.chk
if errorlevel 1 goto tst1err
diff -b err1.out err1.chk
if errorlevel 1 goto tst1err
    echo # Multiple Dependents Test successful
    goto done
:tst1err
    pause
    echo ## TEST1 ##  >> %2
    echo Error: Multiple Dependents Test Unsuccessful | tee -a %2
:done
    del *.obj
    del *.exe
    del *.out
    del main.*
    del foo*.c
    del maketst1
goto end
:usage
echo usage: %0 prgname errorfile
:end
