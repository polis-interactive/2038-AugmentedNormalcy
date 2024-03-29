#!/bin/bash

cd /usr/local/lib

sudo wget -O boost_1_81_0.tar.gz https://boostorg.jfrog.io/artifactory/main/release/1.81.0/source/boost_1_81_0.tar.gz
sudo tar -xvzf boost_1_81_0.tar.gz

cd boost_1_81_0
sudo ./bootstrap.sh gcc
sudo ./b2 --with-system --with-thread --with-date_time --with-regex --with-serialization stage
