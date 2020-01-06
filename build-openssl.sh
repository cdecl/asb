#!/bin/bash

tar zxvf OpenSSL_1_0_2u.tar.gz
mv openssl-OpenSSL_1_0_2u openssl-1_0_2u
cd openssl-1_0_2u

./config 
make -j4 


