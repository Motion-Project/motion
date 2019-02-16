#!/bin/bash

SUMMARY=""
RESULT="OK"
STARS="*****************************"

##################################################################################################
OPTION=" --with-developer-flags"
RESULT_CONFIG="Failed"
RESULT_MAKE="Failed"
printf "\n\n$STARS Testing $OPTION $STARS\n\n"
if ./configure $OPTION; then
  RESULT_CONFIG="OK"
  if make; then
    RESULT_MAKE="OK"
  else
    RESULT="FAIL"
  fi
else
  RESULT="FAIL"
fi
SUMMARY=$SUMMARY"\n Option(s): "$OPTION" Config: "$RESULT_CONFIG" make: "$RESULT_MAKE

##################################################################################################
OPTION=" --with-developer-flags --without-ffmpeg "
RESULT_CONFIG="Failed"
RESULT_MAKE="Failed"
printf "\n\n$STARS Testing $OPTION $STARS\n\n"
if ./configure $OPTION; then
  RESULT_CONFIG="OK"
  if make; then
    RESULT_MAKE="OK"
  else
    RESULT="FAIL"
  fi
else
  RESULT="FAIL"
fi
SUMMARY=$SUMMARY"\n Option(s): "$OPTION" Config: "$RESULT_CONFIG" make: "$RESULT_MAKE

##################################################################################################
OPTION=" --with-developer-flags --without-mysql "
RESULT_CONFIG="Failed"
RESULT_MAKE="Failed"
printf "\n\n$STARS Testing $OPTION $STARS\n\n"
if ./configure $OPTION; then
  RESULT_CONFIG="OK"
  if make; then
    RESULT_MAKE="OK"
  else
    RESULT="FAIL"
  fi
else
  RESULT="FAIL"
fi
SUMMARY=$SUMMARY"\n Option(s): "$OPTION" Config: "$RESULT_CONFIG" make: "$RESULT_MAKE

##################################################################################################
OPTION=" --with-developer-flags --without-sqlite3 "
RESULT_CONFIG="Failed"
RESULT_MAKE="Failed"
printf "\n\n$STARS Testing $OPTION $STARS\n\n"
if ./configure $OPTION; then
  RESULT_CONFIG="OK"
  if make; then
    RESULT_MAKE="OK"
  else
    RESULT="FAIL"
  fi
else
  RESULT="FAIL"
fi
SUMMARY=$SUMMARY"\n Option(s): "$OPTION" Config: "$RESULT_CONFIG" make: "$RESULT_MAKE

##################################################################################################
OPTION=" --with-developer-flags --without-pgsql "
RESULT_CONFIG="Failed"
RESULT_MAKE="Failed"
printf "\n\n$STARS Testing $OPTION $STARS\n\n"
if ./configure $OPTION; then
  RESULT_CONFIG="OK"
  if make; then
    RESULT_MAKE="OK"
  else
    RESULT="FAIL"
  fi
else
  RESULT="FAIL"
fi
SUMMARY=$SUMMARY"\n Option(s): "$OPTION" Config: "$RESULT_CONFIG" make: "$RESULT_MAKE

##################################################################################################
OPTION=" --with-developer-flags --without-v4l2 "
RESULT_CONFIG="Failed"
RESULT_MAKE="Failed"
printf "\n\n$STARS Testing $OPTION $STARS\n\n"
if ./configure $OPTION; then
  RESULT_CONFIG="OK"
  if make; then
    RESULT_MAKE="OK"
  else
    RESULT="FAIL"
  fi
else
  RESULT="FAIL"
fi
SUMMARY=$SUMMARY"\n Option(s): "$OPTION" Config: "$RESULT_CONFIG" make: "$RESULT_MAKE

##################################################################################################
OPTION=" --with-developer-flags --without-webp "
RESULT_CONFIG="Failed"
RESULT_MAKE="Failed"
printf "\n\n$STARS Testing $OPTION $STARS\n\n"
if ./configure $OPTION; then
  RESULT_CONFIG="OK"
  if make; then
    RESULT_MAKE="OK"
  else
    RESULT="FAIL"
  fi
else
  RESULT="FAIL"
fi
SUMMARY=$SUMMARY"\n Option(s): "$OPTION" Config: "$RESULT_CONFIG" make: "$RESULT_MAKE

##################################################################################################
OPTION=" --with-developer-flags --without-mysql --without-sqlite3 --without-pgsql "
RESULT_CONFIG="Failed"
RESULT_MAKE="Failed"
printf "\n\n$STARS Testing $OPTION $STARS\n\n"
if ./configure $OPTION; then
  RESULT_CONFIG="OK"
  if make; then
    RESULT_MAKE="OK"
  else
    RESULT="FAIL"
  fi
else
  RESULT="FAIL"
fi
SUMMARY=$SUMMARY"\n Option(s): "$OPTION" Config: "$RESULT_CONFIG" make: "$RESULT_MAKE

##################################################################################################

printf "\n\n$STARS Test Build Results$STARS\n"
printf "$SUMMARY\n"
if test $RESULT = "FAIL"; then
  printf " Test builds failed\n"
  printf "\n\n$STARS$STARS\n"
  exit 1
else
  printf " Test builds OK\n"
  printf "\n\n$STARS$STARS\n"
  exit 0
fi
