@echo %verbose% off
echo # ===========================
echo # Multiple Dependents Test
echo # ===========================
if .%1 == . goto usage

set OWVERBOSE=0
echo # ---------------------------
echo # TEST 1
echo # ---------------------------
%1 -h -f create
echo. >err1.out
%1 -h -f maketst1 -l err1.out > tst1.out
diff -b err1.out err1.chk
if errorlevel 1 goto tst1err
diff -b tst1.out tst1.chk > nul
if not errorlevel 1 goto testok
diff -b tst1.out tst1a.chk > nul
if errorlevel 1 goto tst1err
:testok
    echo # Multiple Dependents Test successful
    del err1.out
    del tst1.out
    goto done
:tst1err
    echo ## TEST1 ##  >> %2
    echo Error: Multiple Dependents Test Unsuccessful | tee -a %2
:done
    if not .%verbose% == . goto end
    del *.obj
    del *.exe
    del main.*
    del foo*.c
    del maketst1
goto end
:usage
echo usage: %0 prgname errorfile
:end
