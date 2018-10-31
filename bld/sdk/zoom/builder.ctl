# ZOOM Builder Control file
# ============================

set PROJNAME=wzoom

set PROJDIR=<CWD>

[ INCLUDE <OWROOT>/build/prolog.ctl ]

[ INCLUDE <OWROOT>/build/defrule.ctl ]

[ BLOCK <1> rel ]
#================
    cdsay <PROJDIR>

[ BLOCK <1> rel cprel ]
#======================
    <CCCMD> wini86/wzoom.exe <OWRELROOT>/binw/
    <CCCMD> nt386/wzoom.exe  <OWRELROOT>/binnt/
    <CCCMD> ntaxp/wzoom.exe  <OWRELROOT>/axpnt/

    <CCCMD> ntx64/wzoom.exe  <OWRELROOT>/binnt64/

[ BLOCK . . ]

[ INCLUDE <OWROOT>/build/epilog.ctl ]
