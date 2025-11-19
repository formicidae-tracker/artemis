#!/usr/bin/fish

rm -Rf ./movies

build/src/artemis --stub-image-paths (echo f(seq 1 10).png | tr ' ' ,)  \
				  --log-output-dir logs \
				  --camera.fps 10.0 \
				  --at.family Standard41h12 \
				  --video-output.dir=./movies \
				  # --video-output.host=localhost
