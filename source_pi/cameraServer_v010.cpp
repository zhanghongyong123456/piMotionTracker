/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 11/15/2020
 *
 * Description:
 * This program implements a TCP server that controls a raspberry pi camera. When launched, this 
 * program opens a TCP server on port 20006 that a client can connect to. When a client connects there
 * is an expectation that the client send a "camera configuration packet" that allows this server application
 * to set up the camera height/width/fps and optionally a codec for compression of frames.
 * 
 * This program sits in an infinite loop waiting for clients to connect, configure, and stream. If a client 
 * disconnects this program waits for the next client and will reconfingure the camera every time. Each time a 
 * client disconnects this program cleans up, meaning that sometimes clients can use decide to use the CODEC 
 * and sometimes not. 
 *
 * This program can be multi-threaded. The optional encoder is contained in its own thread. If no encoder is used 
 * this program is single threaded. All frames are transferred between thread using a circular queue protected 
 * by mutex.
 *
 */
 
#include <iostream>
#include <opencv2/opencv.hpp>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <string>
#include "VideoCodec.h"
#include "CircularFrameBuf.h"

// Hardcoded. This app launches automatically on Raspberry Pi startup
// so we don't buy anything by making the port a runtime param
#define PORT 20006


// Some useful defines to enable debugging/development
//#define USEVIDEO
#define USECOMPRESSION


// A useful data struct to send/receive camera setup params
struct cameraSettings {
	unsigned int height;
	unsigned int width;
	unsigned int fps;
	char codec[20];
};




/********************** Multi-threading Global Params**********************/
int clientStatus = 1;

// Global encoder and packet so that the main loop can initialize and the encoder thread can utilize
Encoder* vidEncoder;
AVPacket* avPkt;
packet encodePkt, sendPkt;

// Circular buffers / mutexes for video frames going to encoder thread, and encoded packets come from the encoder thread
std::mutex qFrame_mutex;
QueueMat qFrame(64);
std::mutex qPkt_mutex;
QueuePkt qPkt(64);


/*
 * encodeFrames(void) :
 *
 * Description:
 * This function grabs a frame from a circular frame queue, encodes it, then puts the encoded 
 * packet in a circular packet queue. 
 * 
 * This function was created with the intention of putting it in a separate thread, but should work
 * in a single threaded context as well.
 *
 * Inputs:
 *		cv::Mat frame			Mat frame for handling the frames. It is not known until run time the size of the frame - so arg is an uninit cv::Mat
 *
 * Outputs:
 *		N/A
 */
void encodeFrames(cv::Mat frame)
{
	bool success = false;

	// Loop while we still have a client connected
	while (clientStatus > 0)
	{
		// Loop until a frame is available from the input queue
		do
		{
			qFrame_mutex.lock();
			success = qFrame.deQueue(frame);
			qFrame_mutex.unlock();
			
			if (clientStatus < 0 ) return;
		} while (!success);


		// We got a frame, so encode it, then deposit the encoded packet in the output packet queue
		if (vidEncoder->encode(frame, avPkt))
		{
			// Output packet queue uses the "struct packet" data type. It is a extremely simplified verison of an AVPacket
			encodePkt.size = avPkt->size;
			memcpy(encodePkt.buffer, avPkt->data, avPkt->size);

			// Loop until we can deposit the encoded packet in the output queue
			do
			{
				qPkt_mutex.lock();
				success = qPkt.enQueue(encodePkt);
				qPkt_mutex.unlock();
				
				if (clientStatus < 0 ) return;
			} while (!success);
		}
	}
}


int main(int argc, char* argv[])
{
	cv::Mat frame;
	int imgSize;
	cameraSettings camSettings;

	// Camera / video 
#ifdef USEVIDEO
	cv::VideoCapture vidCam("atrium.mp4");	
	camSettings.height = 360;
	camSettings.width = 640;
	camSettings.fps = 30;
#else
	cv::VideoCapture vidCam(0); // Pi camera is /dev/vid0
	camSettings.height = 480;
	camSettings.width = 640;
	camSettings.fps = 30;
#endif

#ifdef USECOMPRESSION
	memset(camSettings.codec, 0, sizeof(camSettings.codec));
	memcpy(camSettings.codec, "mpeg2video", 10);
#else
	memset(camSettings.codec, 0, sizeof(camSettings.codec));
	memcpy(camSettings.codec, "none", 10);
#endif 


	// TCP Socket
	int serverSockFd, clientSockFd;
	struct sockaddr_in serverAddr, clientAddr;
	socklen_t clientLen = sizeof(clientAddr);


	// Thread for encoder
	std::thread m_encoderThread;



	/********************** Setup Socket **********************/
	// Create the socket
	// AF_INET for IPV4, SOCK_STREAM for TCP/IP
	serverSockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSockFd < 0)
	{
		std::cerr << "ERROR opening socket" << std::endl;
		return 1;
	}

	// Create server structure (zero out first)
	memset((char*)&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY; // Current hosts IP addr
	serverAddr.sin_port = htons((int)PORT); // Convert to network byte order

	// Bind server socket to the current IP address on PORT
	if (bind(serverSockFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
	{
		std::cerr << "ERROR on binding" << std::endl;
		return 1;
	}

	// Listen for incoming connections from clients.
	// Note: setting backlog queue to 5, though we only ever expect 1 client
	listen(serverSockFd, 5);



	/******************* Main Server Loop ******************/
	// Run forever accepting connections / handling closed connections
	// Whenver a client connects start streaming video (raw data, not encoded)
	for (;;)
	{
		std::cout << "Listening for connections.." << std::endl;

		// Accept() will write the connecting client's address info 
		// into the the address structure and the size of that structure is clilen.
		// Accept() returns a new socket file descriptor for the accepted connection.
		clientSockFd = accept(serverSockFd, (struct sockaddr*)&clientAddr, &clientLen);
		if (clientSockFd < 0)
		{
			std::cerr << "ERROR on accept" << std::endl;
			return 1;
		}
		std::cout << "Server: Got connection from " << inet_ntoa(clientAddr.sin_addr)
			<< " on port " << ntohs(clientAddr.sin_port) << std::endl;



		/********************** Setup Camera **********************/
		// Read camera setup struct that we know is being sent immediately
		// after connecting.
		clientStatus = read(clientSockFd, &camSettings, sizeof(camSettings));
		if (clientStatus < 0)
		{
			std::cerr << "ERROR reading camera settings" << std::endl;
			return 1;
		}
		std::string codec(camSettings.codec);


		// Check if camera is open and operating before trying to setup
		if (!vidCam.isOpened())
		{
			std::cerr << "Cannot access Raspberry Pi Cam" << std::endl;
			return 1;
		}
#ifndef USEVIDEO
		// Setup basic cam settings	    
		if (!vidCam.set(cv::CAP_PROP_FPS, camSettings.fps) ||
			!vidCam.set(cv::CAP_PROP_FRAME_WIDTH, camSettings.width) ||
			!vidCam.set(cv::CAP_PROP_FRAME_HEIGHT, camSettings.height))
		{
			std::cerr << "Cannot Set FPS" << std::endl;
			return 1;
		}
#endif 



		/********************** Setup Codec **********************/
		// If using compression initalize the codec and a packet that will
		// hold the encoded frame. Launch thread to do encoding
		if (codec != "none")
		{
			vidEncoder = new Encoder(codec.c_str(), AV_PIX_FMT_BGR24, AV_PIX_FMT_YUV420P, camSettings.width, camSettings.height, camSettings.fps);
			//vidEncoder = new Encoder("h264_omx", AV_PIX_FMT_BGR24, AV_PIX_FMT_YUV420P, camSettings.width, camSettings.height, camSettings.fps);
			avPkt = av_packet_alloc();
			if (!avPkt)
				exit(1);

			cv::Mat frameEncode(camSettings.height, camSettings.width, CV_8UC3);
			m_encoderThread = std::thread(encodeFrames, frameEncode);
		}



		/********* Stream Video over TCP Socket ********/
		std::cout << "Streaming Video!" << std::endl;
		// Client has accepted, camera is setup ,stream until client disconnects.
		bool qSuccess;
		int cnt = 0;
		do
		{
			// Get Frame
			vidCam.read(frame);
			if (frame.empty())
			{
				std::cerr << "ERROR! blank frame grabbed" << std::endl;
				return 1;
			}

			// Drop frame into circular queue
			do
			{
				qFrame_mutex.lock();
				qSuccess = qFrame.enQueue(frame);
				qFrame_mutex.unlock();
			} while (!qSuccess);

			// If using compression, then encode frame to packet before sending
			if (codec != "none")
			{				
				// There may not always be a packet to send since the encoder is in another thread, so check
				// to see if a valid packet is available, send if it is
				qPkt_mutex.lock();
				if (qPkt.deQueue(sendPkt))
					clientStatus = send(clientSockFd, sendPkt.buffer, sendPkt.size, 0);
				qPkt_mutex.unlock();
			}
			// If not using compression then, get frame out of circuar queue and resize to send raw data
			else
			{
				qFrame_mutex.lock();
				qFrame.deQueue(frame);
				qFrame_mutex.unlock();

				// Resize to send
				frame = (frame.reshape(0, 1));
				imgSize = frame.total() * frame.elemSize(); // get numBytes
				clientStatus = send(clientSockFd, frame.data, imgSize, 0);
			}
			
#ifdef USEVIDEO
		cnt++;
		if (cnt == vidCam.get(cv::CAP_PROP_FRAME_COUNT))
		{
			cnt = 0;
			vidCam.set(cv::CAP_PROP_POS_FRAMES, 0);
		}
#endif

		} while (clientStatus > 0);

		// The only exit from the video streaming loop is if the send
		// fails to the client (i.e. the client socket was closed) so clean up
		// the socket and go back to waiting for a new connection
		std::cout << "Connection from " << inet_ntoa(clientAddr.sin_addr)
			<< " on port " << ntohs(clientAddr.sin_port)
			<< " has been CLOSED (send fail)." << std::endl;
		close(clientSockFd);
		if (codec != "none")
		{
			m_encoderThread.join();
			delete vidEncoder;
			av_packet_free(&avPkt);
			std::cout << "Connection cleanly closed!" << std::endl;
		}
	}


	std::cout << "Thanks for watching!!!!" << std::endl;

	// Clean up
	close(serverSockFd);



	return 0;
}
