#!/bin/bash

ls -l /home/thomas/Downloads > /home/thomas/c++/DirNotify/build/ls.out
sleep 1

for (( i = 0; i < 20; i++ )); do
  PID=$(( $RANDOM % 9000 + 1000 ))
  echo `date`" core.$PID erzeugt" > /tmp/dbgout
  touch /home/thomas/Downloads/core.$PID
  sleep $(( $RANDOM % 10 ))
done

sleep 1

rm /home/thomas/Downloads/core.*
ls -l /home/thomas/Downloads > /home/thomas/c++/DirNotify/build/ls.out
