#!/bin/bash

start=$(date +%s)

/home/polis/build/2038-AugmentedNormalcy/bin/app_server

end=$(date +%s)

duration=$((end - start))
echo "The process ran for $duration seconds"
