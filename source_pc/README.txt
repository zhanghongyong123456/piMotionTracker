/****************** System Requirements ******************/
Hardware:           x86-64 PC
Operating System:   Windows 10


/****************** Library Requirements ******************/
The source code requires the following third party libraries to build:

OpenCV 4.5.0    https://opencv.org/releases/
                Another useful link to set up Visual Studio: https://medium.com/@subwaymatch/adding-opencv-4-2-0-to-visual-studio-2019-project-in-windows-using-pre-built-binaries-93a851ed6141
                Setting up system paths in windows: https://docs.opencv.org/master/d3/d52/tutorial_windows_install.html#tutorial_windows_install_path
CUDA v11.1      https://developer.nvidia.com/cuda-downloads
FFMPEG 3.4      https://github.com/FFmpeg/FFmpeg/tree/release/3.4
                Critically important helper link: https://www.youtube.com/watch?v=hEYqT0IcpAo
                When using MinGW/MSYS2 to build:
                    1 Type the following to launch Msys
                        C:\msys64\msys2_shell.cmd
                    2 In Msys, type the followings to copy cuda libraries to /usr/local/include/ and /c/local/lib64/ respectively
                        mkdir /usr/local/include/
                        mkdir /usr/local/lib64
                        cp -R /c/Program\ Files/NVIDIA\ GPU\ Computing\ Toolkit/CUDA/v9.1/include/* /usr/local/include/
                        cp -R /c/Program\ Files/NVIDIA\ GPU\ Computing\ Toolkit/CUDA/v9.1/lib/x64/* /usr/local/lib64/
                    3 Type the following to add the path of MS VC 2017 cl.exe and link.exe
                        export PATH="/C/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Tools/MSVC/14.12.25827/bin/Hostx64/x64/":$PATH
                    4 Type the followings to make sure cl and make correctly link to Microsoft cl.exe and link.exe
                        cl
                        link
                    5 Go to ffmpeg-3.4.2 source directory
                        cd /C/..../ffmpeg-3.4.2
                    6 Type
                        ./configure \
                        --enable-gpl\
                        --enable-cuda \
                        --enable-cuvid \
                        --enable-nvenc \
                        --enable-nonfree \
                        --enable-libnpp \
                        --prefix=/c/ffmpeg \
                        --extra-cflags=-I/usr/local/include \
                        --extra-ldflags=-LIBPATH:/usr/local/lib64 \
                        --toolchain=msvc
                    7 Complie and install in prefix directory
                        make
                        make install
                

/****************** Required Source Code******************/
motionTracker_v010.cpp
CircularFrameBuf.cpp
CircularFrameBuf.h
MotionTracker.cpp
MotionTracker.h
VideoCapturePi.cpp
VideoCapturePi.h
VideoCodec.cpp
VideoCodec.h


/****************** Build Command ******************/
N/A when using Visual Studio.
See links above for linking libraries from OpenCV and FFMPEG to Visual Studio.