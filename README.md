---
title: Sobel Filter
author: Samuel Hon & Otavio Ribas
---

Descirption of how to use the code

# Usage
Firstly ensure that you are running the code on the polytechnique server.

The run the following command to ensure that the environments variables are set correctly.
```
$source set_env.sh
```
Now you should be able to run make with 
```
$make
```
There should be 2 warnings regarding reallocarray, in the gif_lib.h file. The can be safely ignored.

In order to run the code on a single gif, you can use the following command
```
$salloc -n i -N j mpirun ./sobelf input_filename output_filename
```
where 
- i, is the number of processes
- j, is the number of machines to launch these processes
- input_filename is the name of the input file in gif format
- output_filename is the name of the output file in gif format

Alternatively you could run the code on all the images saved in the images/original folder with
```
$./run_test.sh
```
and remove the output files with
```
$./clean_test.sh
```