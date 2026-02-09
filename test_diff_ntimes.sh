#!/bin/bash

test_script="$1"

if [ ! -f "$test_script" ]; then
	echo "Error: file not found: $test_script"
	exit 1
fi

if [ ! -x "$test_script" ]; then
	echo "Error: file is not executable: $test_script"
	exit 1
fi

for ((i = 0; i < 10000; i++)); do
	if (( i % 1 == 0 )); then
		echo $i;
	fi
	bash "$test_script"
done

