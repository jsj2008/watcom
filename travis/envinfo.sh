#!/bin/sh
# *****************************************************************
# envinfo.sh - display environment variables
# *****************************************************************
#

envinfo_proc()
{
    if [ "$OWTRAVIS_DEBUG" = "1" ]; then
        env | sed -n -e '/^TRAVIS/p' -e '/^OW/p' -e '/^COVERITY/p' | sort
    fi
    
    return 0
}

envinfo_proc $*
