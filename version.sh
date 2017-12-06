#!/bin/sh
BASE_VERSION="4.1"
if [ -d .git ]; then
    GIT_COMMIT=`git show -s --date=format:'%Y%m%d' --format=%cd-%h` 2>/dev/null
    if [ $? -ne 0 ]; then
        GIT_COMMIT=`git show -s --format=%h`
    fi
    #printf "$BASE_VERSION"
    printf "$BASE_VERSION+git$GIT_COMMIT"
else
    #printf "$BASE_VERSION"
    printf "$BASE_VERSION+gitUNKNOWN"
fi
