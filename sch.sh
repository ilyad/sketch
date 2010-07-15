#!/bin/sh

function run()
{
  SCH="elk ./sketch"
  for sch in $(echo $SCH) ; do
    echo "*** $sch: ***"
    echo $* | $sch
  done
}

if [ -z "$*" ] ; then
  while true ; do
    echo -n '>> '
    read X
    run $X
  done
else
  run "$*"
fi
