#!/bin/bash

cd dsp
make clean
make && make send
cd ..
cd gpp
make clean
make && make send
