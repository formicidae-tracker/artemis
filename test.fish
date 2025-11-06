#!/usr/bin/fish

build/src/artemis --stub-image-paths (echo f(seq 1 2).png | tr ' ' ,)  --log-output-dir logs --camera.fps=1.0 --at.family Standard41h12
