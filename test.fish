#!/usr/bin/fish

rm -Rf ./movies
rm -Rf /tmp/video-output*.dot
rm -Rf ./cu


build/src/artemis --stub-image-paths (echo f(seq 1 10).png | tr ' ' ,)  \
				  --log-output-dir logs \
				  --camera.fps 19.0 \
				  --at.family Standard41h12 \
				  --close-up-dir=./cu \
				  --video-output.height=1080 \
				  --video-output.bitrate=2000 \
				  --video-output.dir=./movies \
				  --video-output.host=localhost

# -*- mode:shell -*-
