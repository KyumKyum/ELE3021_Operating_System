#!/bin/bash

set -e #exit when command fails
make -j
make fs.img
