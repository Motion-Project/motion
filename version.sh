#!/bin/sh
BASE_VERSION="4.0"
if [ -d .git ]; then
	GIT_COMMIT=`git show -s --format=%h`
	echo -n "$BASE_VERSION+git$GIT_COMMIT"
else
	echo -n "$BASE_VERSION"
fi
