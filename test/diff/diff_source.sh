#!/bin/bash

$(pwd)/msolve -f input_files/$file.ms -o test/diff/$file.res \
      -P 2 -l 2 -t 1
if [ $? -gt 0 ]; then
    echo "fail1"
    exit 1
fi

diff test/diff/$file.res output_files/$file.res
if [ $? -gt 0 ]; then
    echo "fail2"
    exit 2
fi

$(pwd)/msolve -f input_files/$file.ms -o test/diff/$file.res \
      -P 2 -l 2 -t 1
if [ $? -gt 0 ]; then
    echo "fail3"
    exit 101
fi

diff test/diff/$file.res output_files/$file.res
if [ $? -gt 0 ]; then
    echo "fail4"
    exit 201
fi

$(pwd)/msolve -f input_files/$file.ms -o test/diff/$file.res \
      -P 2 -l 2 -t 2
if [ $? -gt 0 ]; then
    echo "fail5"
    exit 21
fi

diff test/diff/$file.res output_files/$file.res
if [ $? -gt 0 ]; then
    echo "fail6"
    exit 22
fi

$(pwd)/msolve -f input_files/$file.ms -o test/diff/$file.res \
      -P 2 -l 2 -t 2
if [ $? -gt 0 ]; then
    echo "fail7"
    exit 211
fi

diff test/diff/$file.res output_files/$file.res
if [ $? -gt 0 ]; then
    echo "fail8"
    exit 221
fi

$(pwd)/msolve -f input_files/$file.ms -o test/diff/$file.res \
      -P 2 -l 44 -t 1
if [ $? -gt 0 ]; then
    echo "fail9"
    exit 41
fi

diff test/diff/$file.res output_files/$file.res
if [ $? -gt 0 ]; then
    echo "fail10"
    exit 42
fi

$(pwd)/msolve -f input_files/$file.ms -o test/diff/$file.res \
      -P 2 -l 44 -t 1
if [ $? -gt 0 ]; then
    echo "fail11"
    exit 411
fi

diff test/diff/$file.res output_files/$file.res
if [ $? -gt 0 ]; then
    echo "fail12"
    exit 421
fi

$(pwd)/msolve -f input_files/$file.ms -o test/diff/$file.res \
      -P 2 -l 44 -t 2
if [ $? -gt 0 ]; then
    echo "fail13"
    exit 61
fi

diff test/diff/$file.res output_files/$file.res
if [ $? -gt 0 ]; then
    echo "fail14"
    exit 62
fi

$(pwd)/msolve -f input_files/$file.ms -o test/diff/$file.res \
      -P 2 -l 44 -t 2
if [ $? -gt 0 ]; then
    echo "fail15"
    exit 611
fi

diff test/diff/$file.res output_files/$file.res
if [ $? -gt 0 ]; then
    echo "fail16"
    exit 621
fi

rm test/diff/$file.res
