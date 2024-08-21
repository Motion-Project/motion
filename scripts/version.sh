#!/bin/sh
BASE_VERSION="4.7.0"
if [ -d .git ]; then
    if test "`git diff --name-only`" = "" ; then
        GIT_COMMIT="git"
    else
        GIT_COMMIT="dirty"
    fi
    GIT_COMMIT=$GIT_COMMIT`git show -s --date=format:'%Y%m%d' --format=%cd-%h` 2>/dev/null
    if [ $? -ne 0 ]; then
        GIT_COMMIT=$GIT_COMMIT`git show -s --format=%h`
    fi
else
    GIT_COMMIT="gitUNKNOWN"
fi
printf "$BASE_VERSION"
#printf "$BASE_VERSION+$GIT_COMMIT"

