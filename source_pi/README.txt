/****************** System Requirements ******************/
Hardware:           Raspberry Pi 3B w/ Raspberry Pi v2 Camera module
Operating System:   Raspbian GNU/Linux 10 (buster)

Note: After installation of the operating system it is critical that the camera interface be enabled.



/****************** Library Requirements ******************/
The source code requires the following third party libraries to build:

OpenCV 4.1.0    Useful link here: https://solarianprogrammer.com/2019/09/17/install-opencv-raspberry-pi-raspbian-cpp-python-development/
FFMPEG 3.4      Useful link here: https://pimylifeup.com/compiling-ffmpeg-raspberry-pi/
                Note: The link installs the master branch from FFMPEG. This is not the correct version - so be sure to switch to the 3.4 branch before building
                Note2: This is the configuration used to build FFMPEG (this only works if all the libraries are built/installed from the "pimylifeup" link above)
                ./configure \
                    --extra-cflags="-I/usr/local/include" \
                    --extra-ldflags="-L/usr/local/lib" \
                    --extra-libs="-lpthread -lm -latomic" \
                    --arch=armel \
                    --enable-gpl \
                    --enable-libx264 \
                    --enable-mmal \
                    --enable-nonfree \
                    --enable-omx \
                    --enable-omx-rpi \
                    --enable-version3 \
                    --target-os=linux \
                    --enable-shared \
                    --disable-static \
                    --enable-pthreads





/****************** Required Source Code******************/
cameraServer_v010.cpp
CircularFrameBuf.cpp
CircularFrameBuf.h
VideoCodec.cpp
VideoCodec.h


/****************** Build Command ******************/
g++ CircularFrameBuf.cpp VideoCodec.cpp cameraServer_v010.cpp -I/home/pi/FFmpeg34/include -L/home/pi/FFmpeg34/lib -lavcodec -lvpx -lm -lvpx -lm -lvpx -lm -lvpx -lm -lwebpmux -lwebp -lm -llzma -lm -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lm -lpthread -lm -lpng -lz -lsnappy -lstdc++ -lz -lm -lpthread -lmp3lame -lm -lopus -lm -logg -lvorbis -lvorbisenc -lwebp -lx264 -lx265 -lxvidcore -ldl -pthread -lva `pkg-config --cflags --libs opencv libavutil libswscale` -o cameraServer_v010
