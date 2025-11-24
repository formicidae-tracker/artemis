#!/usr/bin/fish

rm -Rf ./movies
rm -Rf /tmp/video-output*.dot

build/src/artemis --stub-image-paths (echo f(seq 1 10).png | tr ' ' ,)  \
				  --log-output-dir logs \
				  --camera.fps 10.0 \
				  --at.family Standard41h12 \
				  --video-output.dir=./movies \
				  --video-output.host=localhost
