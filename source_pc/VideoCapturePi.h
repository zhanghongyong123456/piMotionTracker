/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 10/25/2020
 *
 * Description:
 * This is the header file for the VideoCapturePi class that is used to connect to and stream video
 * data from a Raspberry Pi + Camera + Server. This class is meant to "look" like the OpenCV 
 * VideoCapture class, but is really only designed for use as a streaming object. In fact if this
 * class begins reading frames and then stops - the network socket will be closed by the server. This 
 * is by design - and for simplicity.
 * 
 */

#pragma once
#include <iostream>
#include <string>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <opencv2/opencv.hpp>
#include "VideoCodec.h"


 // Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")


// A useful data struct to send/receive camera setup params
struct cameraSettings {
	unsigned int height;
	unsigned int width;
	unsigned int fps;
	char codec[20];
};


/*
 * class VideoCapturePi
 *
 * The VideoCapturePi class is meant to immitate the OpenCV "VideoCapture" class. This class is 
 * connects to a Raspberry Pi + Camera server located on a local network at construction, sets up the 
 * camera properties, and then handles streaming of the data over the network.
 * 
 * At this time the camera captures are raw RGB data and NOT H.264 encoded. The data is sent over the 
 * network as a 1D array that is (height x width x 3) bytes long, with ordering [BGR BGR BGR ... ]. 
 *
 */
class VideoCapturePi
{
	/********** Private Members **********/
	// Network Variables
	std::string ip;
	unsigned int port;
	int socketFd, sts;
	WSADATA wsaData;
	struct sockaddr_in serverAddr;
	struct hostent* server;

	// Codec Variables
	Decoder* vidDecoder;
	std::string codecName;
	AVPacket* rcvPkt;

	// Camera Variables
	cameraSettings camSettings;
	char* socketBuffer;

	// Misc
	bool linkStatus;


	/*
	 * int connectTcpSocket(void);
	 *
	 * Description:
	 * Create and connect to RPI server via TCP socket.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		int				status of connection
	 */
	int connectTcpSocket(void);


	/*
	 * int setupCamera(void);
	 *
	 * Description:
	 * Setup camera parameters
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		int				status of camera
	 */
	int setupCamera(void);


	/*
	 * int initialize(void);
	 *
	 * Description:
	 * Setup socket and camera (ready to stream after this)
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		int				status of initialization
	 */
	int initialize(void);



public:	
	/********** Public Members **********/

	/*
	 * Delete default constructor. Do NOT allow users to use the
	 * VideoCapturePi class without providing an ip and port num
	 */
	VideoCapturePi() = delete;


	/*
	 * VideoCapturePi(std::string inIpAddr, unsigned int inPort, unsigned int inWidth, unsigned int inHeight, unsigned int inFps) :
	 *
	 * Description:
	 * Constructor overload for VideoCapturePi class. This constructors sets up the network connection as 
	 * well as overrides the default camera parameters.
	 *
	 * Inputs:
	 *		const std::string inIpAddr			IP Address of Raspberry Pi + Camera server
	 *		const unsigned int inPort			Port of Raspberry Pi + Camera server
	 *		const unsigned int inWidth			camera frame width
	 *		const unsigned int inHeight			camera frame height
	 *		const unsigned int inFps			camera frame fps
	 *
	 * Outputs:
	 *		N/A
	 */
	VideoCapturePi(const std::string inIpAddr, const unsigned int inPort, const unsigned int inWidth, const unsigned int inHeight, const unsigned int inFps) :
		ip(inIpAddr),
		port(inPort),
		codecName("none")
	{		
		camSettings.height = inHeight;
		camSettings.width = inWidth;
		camSettings.fps = inFps;
		memset(camSettings.codec, 0, sizeof(camSettings.codec));
		memcpy(camSettings.codec, codecName.c_str(), codecName.length());

		// Setup frame buffer just once and reuse
		socketBuffer = new char[camSettings.width * camSettings.height * 3 + AV_INPUT_BUFFER_PADDING_SIZE]; //every pixel is RGB (3 bytes)
		linkStatus = (bool)(!initialize());
	}


	/*
	 * VideoCapturePi(std::string inIpAddr, unsigned int inPort, unsigned int inWidth, unsigned int inHeight, unsigned int inFps) :
	 *
	 * Description:
	 * Constructor overload for VideoCapturePi class. This constructors sets up the network connection as
	 * well as overrides the default camera parameters.
	 *
	 * Inputs:
	 *		const std::string inIpAddr			IP Address of Raspberry Pi + Camera server
	 *		const unsigned int inPort			Port of Raspberry Pi + Camera server
	 *		const unsigned int inWidth			camera frame width
	 *		const unsigned int inHeight			camera frame height
	 *		const unsigned int inFps			camera frame fps
	 *	    const bool inEnaH264				enable H264 compression over the tcp socket
	 *
	 * Outputs:
	 *		N/A
	 */
	VideoCapturePi(const std::string inIpAddr, const unsigned int inPort, const unsigned int inWidth, const unsigned int inHeight, const unsigned int inFps, std::string codec) :
		ip(inIpAddr),
		port(inPort),
		codecName(codec)
	{
		camSettings.height = inHeight;
		camSettings.width = inWidth;
		camSettings.fps = inFps;
		memset(camSettings.codec, 0, sizeof(camSettings.codec));
		memcpy(camSettings.codec, codecName.c_str(), codecName.length());

		if (codecName != "none")
		{
			// I don't want to construct the decoder object unless we're actually using it. So my solution was to have a pointer to a Decoder object
			// as a member variable, construct a new one when needed, then set the pointer to the class member Decoder.
			vidDecoder = new Decoder(camSettings.codec, AV_PIX_FMT_BGR24, AV_PIX_FMT_YUV420P, camSettings.width, camSettings.height, camSettings.fps);

			// Allocate packet to be used to get data from decoder
			rcvPkt = av_packet_alloc();
			if (!rcvPkt)
				exit(1);

			socketBuffer = (char*)av_malloc(camSettings.width * camSettings.height * 3 + AV_INPUT_BUFFER_PADDING_SIZE); // use av_malloc for socketBuffer when using H264
			if (!socketBuffer)
				exit(1);
		}
		else
		{
			// Setup frame buffer just once and reuse
			socketBuffer = new char[camSettings.width * camSettings.height * 3 + AV_INPUT_BUFFER_PADDING_SIZE]; //every pixel is RGB (3 bytes)
		}

		linkStatus = (bool)(!initialize());
	}


	/*
	* !VideoCapturePi(void) :
	*
	* Description:
	* Destructor for VideoCapturePi class. 
	*
	* Inputs:
	*		N/A
	*
	* Outputs:
	*		N/A
	*/
	~VideoCapturePi(void)
	{
		// Clean up
		release();		
		if (codecName != "none")
		{
			delete vidDecoder;
			av_packet_free(&rcvPkt);
			//av_freep(socketBuffer);
		}
		else
		{
			delete[] socketBuffer;
		}
 	}


	/*
	 * unsigned int getWidth(void) const;
	 *
	 * Description:
	 * Get the current frame width.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		unsigned int		camera frame width
	 */
	unsigned int getWidth(void) const;


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
	unsigned int getHeight(void) const;


	/*
	 * unsigned int getFps(void) const;
	 *
	 * Description:
	 * Get the current frame rates per second (fps).
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		unsigned int		camera fps
	 */
	unsigned int getFps(void) const;


	/*
	 * bool isOpened(void)
	 *
	 * Description:
	 * Check the camera status.  True if socket is opened and camera stream available.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		bool							indicates status of Rpi + Camera link
	 */
	bool isOpened(void) const;


	/*
	 * bool open(const std::string inIpAddr, const unsigned int inPort);
	 *
	 * Description:
	 * Establish link to Rpi + Camera server. This is used if the default constructor is 
	 * called and the socket has not yet been set up
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		bool							indicates status of Rpi + Camera link
	 */
	bool open(const std::string inIpAddr, const unsigned int inPort);


	/*
	 * bool read(cv::Mat& image);
	 *
	 * Description:
	 * Grabs, decodes and returns the next video frame.
	 *
	 * Inputs:
	 *		cv::Mat image					image the video frame is returned here. If no frames has been grabbed the image will be empty.
	 *
	 * Outputs:
	 *		bool							false if no frames has been grabbed
	 */
	bool read(cv::Mat& image);


	/*
	 * VideoCapturePi& operator>> (cv::Mat& image);
	 *
	 * Description:
	 * Grabs, decodes and returns the next video frame. Same is read()
	 *
	 * Inputs:
	 *		VideoCapturePi&					VideoCapturePi object to read from
	 *
	 * Outputs:
	 *		cv::Mat& image					Returned video frame
	 */
	VideoCapturePi& operator>> (cv::Mat& image);


	/*
	 * void release(void);
	 *
	 * Description:
	 * Close the socket to the Rpi + Camera and free up server to service someone else.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	void release(void);
};