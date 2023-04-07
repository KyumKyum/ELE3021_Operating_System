#!/bin/bash

set -e #exit when command fails
make clean
make
make fs.img
