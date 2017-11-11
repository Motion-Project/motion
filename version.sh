#!/bin/sh
BASE_VERSION="4.1"
if [ -d .git ]; then
    GIT_COMMIT=`git show -s --date=format:'%Y%m%d' --format=%cd-%h`
    #printf "$BASE_VERSION"
    printf "$BASE_VERSION+git$GIT_COMMIT"
else
    #printf "$BASE_VERSION"
    printf "$BASE_VERSION+gitUNKNOWN"
fi
