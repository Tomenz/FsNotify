#!/bin/bash

ls -l /home/thomas/Downloads > /home/thomas/c++/DirNotify/build/ls.out
sleep 1

for (( i = 0; i < 20; i++ )); do
  echo `date`" Erzeugt" > /tmp/dbgout
  touch /home/thomas/Downloads/core.$(( $RANDOM % 9000 + 1000 ))
  sleep $(( $RANDOM % 10 ))
done

sleep 1

rm /home/thomas/Downloads/core.*
ls -l /home/thomas/Downloads > /home/thomas/c++/DirNotify/build/ls.out
