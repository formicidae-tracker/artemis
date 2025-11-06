#!/usr/bin/fish

build/src/artemis --stub-image-paths (echo f(seq 1 10).png | tr ' ' ,)  --log-output-dir logs --camera.fps=8.0 --at.family Standard41h12
