#!/bin/bash

succes=1;

for i in `seq 1 128`;
	do
	echo "Testing for $i"
	./run.sh $i > output
	if grep -Fxq "Failure" output
	then
    	echo "Failed for $i"
		succes=0;
	fi
done

if [ "$succes" -eq 1 ]; then
	echo "Succes"
else
	echo "Failure"
fi

