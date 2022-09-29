#!/bin/bash

cd trickle_proj
mkdir bin obj output
make
./bin/server_udp res/A2_small_file.txt&
pid=$!
./bin/clientmgr_udp -o output 5
kill -15 $pid
md5 output/outfile_*
