# Artemis: low level Computer Vision program for the FORmicidae Tracker


This project is a c++11 low-level program that is used to track
ants. It uses apriltags2 to detect fiducial markers, Euresys GenTL
implementation to acquire data from CoaXPress framegrabber, OpenCV for
live image conversion annotation and book keeping, and protocol buffer
for realtime communication of the extracted data.
