#!/bin/sh
gcc -Wall -Wno-psabi -Wno-sign-compare -D__LINUX_ALSA__ -DAVOID_TIMESTAMPING -O3 -fPIC -Wno-unused-variable *.cpp -o PotSeq -lm -ldl -lstdc++ -lasound -lpthread
rm ./*.o
