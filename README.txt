/**********************************************************/
ECE6122 Project Name:       Pi-Vision with Human Detection
Team Member:                Jordan Leiker
Date:                       11/20/2020

This project turns the Raspberry Pi + camera into an OpenCV compatible VideoCapture-like IP camera that can
be used in any (c++) OpenCV project. Built in to the IP camera software is hardware accelerated compression, achieved by
using the FFMpeg C-API to communicate with GPUs. Finally, an example multi-object motion tracking algorithm has been ported
from a Matlab example to OpenCV / custom C++ to illustrate an end-to-end motion tracking with Raspberry Pi IP camera solution.

Class requirements met:
    Sockets
    Multi threading (std::thread)

Custom classes made:
    VideoCapturePi      Emulates OpenCV VideoCapture class, but pulls frames over a TCP socket from a Raspberry Pi
    VideoCodec          Utility class that wraps FFMpeg C-API calls into convienient functions for decoding/encoding
      |---> Encoder     Derived class from VideoCodec. This class accepts encoder information, initializes the encoder, and converts OpenCV frames to encoded AVFrame packets (while hardware acclerating the encoding)
      |---> Decoder     Derived class from VideoCodec. This class accepts decoder information, initializes the decoder, and converts AVFrame packets to OpenCV frames (while hardware acclerating the decoding)
    MotionTracker       This class implements a general purpose motion tracker. The algorithm was ported from a Matlab example (link below) to OpenCV and custom C++. This class 
                        contains the methods needed to use the motion detection and maintain tracks.
                        (https://www.mathworks.com/help/vision/ug/motion-based-multiple-object-tracking.html)



/******************** Directory Outline ********************/
./README.txt                        This file.
./sequenceDiagram_flowChart.pdf     Diagrams of power on sequences/handshaking for Raspberry Pi and Windows PC. Also included is image processing/motion tracking pipeline.
./LeikerJordan_StatementOfWork.pdf  Initial project proposal (included for reference)
./source_pc 
 |---> README.txt                   System information, library requirements, build instructions                        
 |---> motionTracker_v010.cpp       Main program entry point. Uses the rest of the source code to implement motion tracker from Raspberry Pi camera
 |---> CircularFrameBuf.cpp         Circular Buffer (for OpenCV Mats and custom packets) functional code
 |---> CircularFrameBuf.h           Circular Buffer header file.
 |---> MotionTracker.cpp            Class implementing an OpenCV version of Matlab's multiple object motion tracking algorithm 
 |---> MotionTracker.h              Header file for class implementing OpenCV version of Matlabs multiple object motion tracking
 |---> VideoCapturePi.cpp           Class mimicking OpenCV VideoCapture class that instead gets video frames over a TCP socket from custom Raspberry Pi software
 |---> VideoCapturePi.h             Header file for Raspberry Pi video capture
 |---> VideoCodec.cpp               Class functional code that wraps FFMPEG native-C functions for encoding/decoding video
 |---> VideoCodec.h                 Header file for class that wraps FFMPEG functions into easy to use methods.
./source_rpi
 |---> README.txt                   System information, library requirements, build instructions
 |---> cameraServer_v010.cpp        Program that launches a TCP server and sets up the camera, streams video, etc when a VideoCapturePi client connects
 |---> CircularFrameBuf.cpp         (same as above)
 |---> CircularFrameBuf.h           (same as above)
 |---> VideoCodec.cpp               (same as above)
 |---> VideoCodec.h                 (same as above)



/******************** Links to Videos ********************/
ece6122_final_1of4.mov      Link:   https://youtu.be/vBRhVdJyhEw 
                            Description: Phone capture of hardware setup. Shows physical raspberry pi + camera, pi OS launched on monitor, video stream being brought up.
    
    
ece6122_final_2of4.mkv      Link:   https://youtu.be/c5mt6vwBt7c
                            Description: Discusses power on sequence. Illustrates use of CODEC's to bring network bandwidth down by 99%. Show's GPU use to prove hardware acceleration.
    
    
ece6122_final_3of4.mkv      Link:   https://youtu.be/dHgAQSPHgj0
                            Description: Discusses image processing pipeline. Illustrates motion tracking using camera.
                            
                            
ece6122_final_4of4.mkv      Link:   https://youtu.be/c25yK531ktQ
                            Description: Illustrates multi-object tracking using video input (not camera). Shows how to use software for different codecs.
