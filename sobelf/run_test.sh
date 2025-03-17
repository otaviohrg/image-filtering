#!/bin/bash

make

INPUT_DIR=images/original
OUTPUT_DIR=images/processed
mkdir $OUTPUT_DIR 2>/dev/null

for i in $INPUT_DIR/*gif ; do
    DEST=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
    echo "Running test on $i -> $DEST"

    OMP_NUM_THREADS=1 salloc -n 4 -N 1 mpirun ./sobelf $i $DEST
    ##if N ~ n, the overhead from sharing info is quite expensive
done
