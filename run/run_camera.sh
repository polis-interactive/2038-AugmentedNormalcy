#!/bin/bash

start=$(date +%s)

/home/pi/build/2038-AugmentedNormalcy/bin/app_camera

end=$(date +%s)

duration=$((end - start))
echo "The process ran for $duration seconds"