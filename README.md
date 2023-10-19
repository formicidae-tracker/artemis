# FORmicidae Tracker (FORT) : Online Tracking Software


The [FORmicidae Tracker (FORT)](https://formicidae-tracker.github.io) is an advanced online tracking system designed specifically for studying social insects, particularly ants and bees, FORT utilizes fiducial markers for extended individual tracking. Its key features include real-time online tracking and a modular design architecture that supports distributed processing. The project's current repositories encompass comprehensive hardware blueprints, technical documentation, and associated firmware and software for online tracking and offline data analysis.# Artemis: low level Computer Vision program for the FORmicidae Tracker


This project is a c++11 low-level program that is used to track
fiducial markers. It uses apriltags3 to detect markers, Euresys GenTL
implementation to acquire data from a CoaXPress framegrabber, OpenCV for
live image conversion annotation and book keeping, OpenGL for realtime preview, and protocol buffer
for realtime communication of the extracted data.
