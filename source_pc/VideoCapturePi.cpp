/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 10/25/2020
 *
 * Description:
 * This is the functional code for the VideoCapturePi class that is used to connect to and stream video
 * data from a Raspberry Pi + Camera + Server. This class is meant to "look" like the OpenCV
 * VideoCapture class, but is really only designed for use as a streaming object. In fact if this
 * class begins reading frames and then stops - the network socket will be closed by the server. This
 * is by design - and for simplicity.
 *
 */

#include "VideoCapturePi.h"


 /*
  * int connectTcpSocket(void);
  *
  * Description:
  * (Private member function)
  * Create and connect to RPI server via TCP socket.
  *
  * Inputs:
  *		N/A
  *
  * Outputs:
  *		int				status of connection
  */
int VideoCapturePi::connectTcpSocket(void)
{
    // Need constant pointer to c-array for INET setup, but member variable is std::string. 
    // So convert here at creation.
    const char* ipAddr = ip.c_str();


    // Initialize Winsock
    sts = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (sts != NO_ERROR) {
        std::cerr << "WSAStartup failed: " << sts << std::endl;
        return 1;
    }

    socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketFd == INVALID_SOCKET)
    {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, ipAddr, &(serverAddr.sin_addr));
    serverAddr.sin_port = htons(port);

    // Connect
    sts = connect(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (sts == SOCKET_ERROR)
    {
        closesocket(sts);
        std::cerr << "Unable to connect to server: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    return 0;
}


/*
 * int setupCamera(void);
 *
 * Description:
 * (Private member function)
 * Setup camera parameters
 *
 * Inputs:
 *		N/A
 *
 * Outputs:
 *		int				status of camera
 */
int VideoCapturePi::setupCamera(void)
{
    int sts;

    sts = send(socketFd, (char *)&camSettings, sizeof(camSettings), 0);
    if (sts == SOCKET_ERROR) 
    {
        std::cerr << "Camera Setup Failed: " << WSAGetLastError() << std::endl;
        closesocket(socketFd);
        WSACleanup();
        return 1;
    }

    return 0;
}


/*
 * int initialize(void);
 *
 * Description:
 * (Private member function)
 * Setup socket and camera (ready to stream after this)
 *
 * Inputs:
 *		N/A
 *
 * Outputs:
 *		int				status of initialization
 */
int VideoCapturePi::initialize(void)
{
    int sts;

    sts = connectTcpSocket();
    if (sts)
    {
        return 1;
    }

    sts = setupCamera();
    if (sts)
    {
        return 1;
    }

    return 0;
}


/*
 * unsigned int getWidth(void) const;
 *
 * Description:
 * (Public member function)
 * Get the current frame width.
 *
 * Inputs:
 *		N/A
 *
 * Outputs:
 *		unsigned int		camera frame width
 */
unsigned int VideoCapturePi::getWidth(void) const
{
    return camSettings.width;
}


/*
 * unsigned int getHeight(void) const;
 *
 * Description:
 * Get the current frame height.
 *
 * Inputs:
 *		N/A
 *
 * Outputs:
 *		unsigned int		camera frame height
 */
unsigned int VideoCapturePi::getHeight(void) const
{
    return camSettings.height;
}


/*
 * unsigned int getFps(void) const;
 *
 * Description:
 * (Public member function)
 * Get the current frame rates per second (fps).
 *
 * Inputs:
 *		N/A
 *
 * Outputs:
 *		unsigned int		camera fps
 */
unsigned int VideoCapturePi::getFps(void) const
{
    return camSettings.fps;
}


/*
 * bool isOpened(void)
 *
 * Description:
 * (Public member function)
 * Check the camera status.  True if socket is opened and camera stream available.
 *
 * Inputs:
 *		N/A
 *
 * Outputs:
 *		bool							indicates status of Rpi + Camera link
 */
bool VideoCapturePi::isOpened(void) const
{
    return linkStatus;
}


/*
 * bool open(const std::string inIpAddr, const unsigned int inPort);
 *
 * Description:
 * (Public member function)
 * Establish link to Rpi + Camera server. This is used if the default constructor is
 * called and the socket has not yet been set up
 *
 * Inputs:
 *		N/A
 *
 * Outputs:
 *		bool							indicates status of Rpi + Camera link
 */
bool VideoCapturePi::open(const std::string inIpAddr, const unsigned int inPort)
{
    // Set member variables that weren't set during construction
    ip = inIpAddr;
    port = inPort;

    // Initialize link and camera
    linkStatus = (bool)(!initialize());

    return linkStatus;
}


/*
 * bool read(cv::Mat& image);
 *
 * Description:
 * (Public member function)
 * Grabs, decodes and returns the next video frame. If H264 is enabled the incoming packet over TCP is sent to the CODEC 
 * for decoding into a video frame.
 * 
 * Note: Once this function is called it is assumed that it will be called repeatedly (or fast enough) to 
 * main a stream over the socket. If it falls behind or is only called once, for example, the server software on the 
 * Raspberry Pi will assume the client closed the socket and it will return to a listening state.
 *
 * Inputs:
 *		cv::OutputArray image			image the video frame is returned here. If no frames has been grabbed the image will be empty.
 *
 * Outputs:
 *		bool							false if no frames has been grabbed
 */
bool VideoCapturePi::read(cv::Mat& image)
{
    unsigned int imgSize = camSettings.height * camSettings.width * 3;
    int iResult = 0;   

    if (codecName != "none")
    {  
        bool validFrame = false;
        // It may take several TCP packets to collect a full frame. So loop here receiving until 
        // we get one.
        do
        {
            // Try to receive the raw image size since we know thats more than enough to get a packet
            // Note: Only get more info if the vidDecoder is free. Packets may contains multiple frames, 
            // so we do not need to read more data over the network until we've gotten all the frames 
            // from our last packet
            if (vidDecoder->isFree)
                iResult = recv(socketFd, socketBuffer, imgSize, 0);

            // Decode if we return info over socket or if vidDecoder is still mid decode from last socket packet
            if (iResult > 0 || !vidDecoder->isFree) {       
                av_packet_from_data(rcvPkt, (uint8_t*)socketBuffer, iResult);
                validFrame = vidDecoder->decode(rcvPkt, image); //rcvPkt is unused if !vidDecoder->isFree
            }
            else if (iResult == 0)
            {
                std::cerr << "Connection closed" << std::endl;
                exit(1);
            }
            else
            {
                std::cerr << "Recv failed: " << WSAGetLastError() << std::endl;
                return false;
            }
        } while (!validFrame);
    }

    // If not using compression then we need to recieve an entire raw image (height x width x 3 x 8 bits)
    // Then we need to convert from the planar [B G R B G R...] format to the 2D format
    else
    {
        // Receive a raw image - we will need to get multiple packets containing all the bytes
        for (unsigned int i = 0; i < imgSize; i += iResult)
        {
            // Initially this asks to receive the full image size number of bytes, each time a receive happens
            // we subtract the number of bytes received and request to receive the remaining number of bytes.
            iResult = recv(socketFd, socketBuffer + i, imgSize - i, 0);
            if (iResult > 0) {
                // bytes received, all good
            }
            else if (iResult == 0)
            {
                std::cerr << "Connection closed" << std::endl;
                exit(1);
            }
            else
            {
                std::cerr << "Recv failed: " << WSAGetLastError() << std::endl;
                return false;
            }
        }

        // Received data is in 1D buffer [ B G R  B G R  B G R ... ]
        // So cycle through height/width of image casting each pixel location to a 3D array of B G R.
        // The output of this look is a Height x Width x 3 image array.
        int ptr = 0;
        for (int i = 0; i < camSettings.height; i++) {
            for (int j = 0; j < camSettings.width; j++) {
                image.at<cv::Vec3b>(i, j) = cv::Vec3b(socketBuffer[ptr + 0], socketBuffer[ptr + 1], socketBuffer[ptr + 2]);
                ptr = ptr + 3;
            }
        }
    }

    return true;
}


/*
 * VideoCapturePi& operator>> (cv::Mat& image);
 *
 * Description:
 * (Public member function)
 * Grabs, decodes and returns the next video frame. Same is read()
 *
 * Inputs:
 *		VideoCapturePi&					VideoCapturePi object to read from
 *
 * Outputs:
 *		cv::Mat& image					Returned video frame
 */
VideoCapturePi& VideoCapturePi::operator>> (cv::Mat& image)
{
    this->read(image);
    return *this;
}


/*
 * void release(void);
 *
 * Description:
 * (Public member function)
 * Close the socket to the Rpi + Camera and free up server to service someone else.
 *
 * Inputs:
 *		N/A
 *
 * Outputs:
 *		N/A
 */
void VideoCapturePi::release(void)
{
    closesocket(socketFd);
    WSACleanup();
}