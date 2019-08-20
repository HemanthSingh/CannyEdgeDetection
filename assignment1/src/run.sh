#!/bin/bash

size=$1

if [ -z "$size" ] #if size are not specified
then
    size=128 #default size
fi

/home/root/powercycle.sh
./helloDSPgpp helloDSP.out $size 2>&1

