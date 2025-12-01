# FORmicidae Tracker (FORT) : Online Tracking Software

[![DOI](https://zenodo.org/badge/177124380.svg)](https://zenodo.org/doi/10.5281/zenodo.10019138)

The [FORmicidae Tracker (FORT)](https://formicidae-tracker.github.io) is an advanced online tracking system designed specifically for studying social insects, particularly ants and bees, FORT utilizes fiducial markers for extended individual tracking. Its key features include real-time online tracking and a modular design architecture that supports distributed processing. The project's current repositories encompass comprehensive hardware blueprints, technical documentation, and associated firmware and software for online tracking and offline data analysis.# Artemis: low level Computer Vision program for the FORmicidae Tracker


This project is a c++11 low-level program that is used to track
fiducial markers. It uses apriltags3 to detect markers, Euresys GenTL
implementation to acquire data from a CoaXPress framegrabber, OpenCV for
live image conversion annotation and book keeping, OpenGL for realtime preview, and protocol buffer
for realtime communication of the extracted data.


## Docker image

This repository provides a docker image. But primarly for test and development
purpose. For deployment, the need of custom kernel driver to interface with the
actual software make it pretty much too convoluted. The docker image has no
support for any framegrabber.


It needs a lot of different variable to run.

* `- e DISPLAY=#$DISPLAY`, `-v /tmp/.X11-unix:/tmp/.X11-unix`, `--user $(id -u):$(id -g)`  to connect to the X11 server
* `-v .:/app` to allow for file access
* `--driver /dev/dri:/dev/dri` for access to the graphic card for VA-API encoding.


```bash
docker build -t artemis .
docker run --rm -e DISPLAY=$DISPLAY \
	-v /tmp/.X11-unix:/tmp/.X11-unix \
	--user $(id -u):$(id -g) \
	-v .:/app
	artemis /usr/local/bin/artemis [OPTIONS]
```
