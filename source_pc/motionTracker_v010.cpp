/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 10/25/2020
 *
 * Description:
 * Main function for "Motion Detection" via Raspberry Pi IP Camera project.
 *
 * The "VideoCapturePi" class is used to emulate the VideoCapture OpenCV class. The
 * custom class establishes a connection to Raspberry Pi + Camera over a TCP socket and
 * streams video frames. The class constructor sets up the frame size and frame rate. Reading
 * from the VdeoCapturePi object starts the server -> client TCP stream.
 *
 * The video stream from the Raspberry Pi will be run through a motion detection algorith and
 * bounding boxes around the detected "moving objects" will be drawn on screen. The algorithm
 * used is from a Matlab example program from the Computer Vision toolbox, but ported to C++ and
 * OpenCV.
 *
 * Note for myself: RPi ETH0 =	192.168.0.112
 *					port =		20006
 *
 */

#include <thread>
#include <mutex>
#include "VideoCapturePi.h"
#include "MotionTracker.h"
#include "CircularFrameBuf.h"



 /******************** Function Definitions ********************/
cv::Mat flipMat(const cv::Mat& inImage);
void processVideo(cv::Mat frameIn);



/******************** Global Variables ********************/
// Circular buffers / mutexes for video frames going to video processing thread, and frames coming out
std::mutex qFrameRaw_mutex;
QueueMat qFrameRaw(64);
std::mutex qFrameProc_mutex;
QueueMat qFrameProc(64);

// Motion tracking object pointer (is initialized in main());
MotionTracker* mTracker;

// Allow program to exit when user hits ESC
bool exitProgram = false;



int main(int argc, char* argv[])
{
    /******************** Command Line Parsing ********************/ //16,24,30
    const cv::String keys =
        "{help h usage ? |               | Help is on the way!                                            }"
        "{height rows    | 480           | video frame height                                             }"
        "{width cols     | 640           | video frame width                                              }"
        "{fps            | 20            | fps for output video                                           }"
        "{ip             | 192.168.0.112 | ip address of RPI                                              }"
        "{port           | 20006         | port of RPI socket                                             }"
        "{codec          | mpeg4         | Compression? ('none' for no, 'mpeg2video', 'mpeg4', etc for yes}"
        ;

    cv::CommandLineParser parser(argc, argv, keys);
    parser.about("RPI Motion Tracker v0.1.0");
    if (parser.has("help"))
    {
        parser.printMessage();
        return 1;
    }

    unsigned int height = parser.get<unsigned int>("height");
    unsigned int width = parser.get<unsigned int>("width");
    unsigned int fps = parser.get<unsigned int>("fps");
    std::string ip = parser.get<std::string>("ip");
    unsigned int port = parser.get<unsigned int>("port");
    std::string codec = parser.get<std::string>("codec");


    if (!parser.check())
    {
        parser.printErrors();
        return 1;
    }



    /******************** Camera Setup ********************/
    // Note: This constructor overload will open socket and set up camera so 
    // we are ready to stream after.
    VideoCapturePi vidCam(ip, port, width, height, fps, codec);

    if (!vidCam.isOpened())
    {
        std::cerr << "Application Failure: Video stream failed. Exiting now..." << std::endl;
        return 1;
    }


    /******************** Motion Tracker Setup ********************/
    Ptr<BackgroundSubtractorMOG2> pBackSub = createBackgroundSubtractorMOG2();
    pBackSub->setBackgroundRatio(0.7);	// set to match Matlab
    pBackSub->setNMixtures(3); // set to match Matlab


    SimpleBlobDetector::Params blobParams;
    blobParams.minThreshold = 0;
    blobParams.maxThreshold = 254;
    blobParams.thresholdStep = 253;
    blobParams.minDistBetweenBlobs = 50;
    blobParams.filterByArea = true;
    blobParams.minArea = 400;
    blobParams.maxArea = (height * width) / 10; // allow blobs as large as 1/10th the screen to be detected
    blobParams.filterByColor = false;
    blobParams.filterByCircularity = false;
    blobParams.filterByConvexity = false;
    blobParams.filterByInertia = false;
    Ptr<SimpleBlobDetector> pBlobDetector = SimpleBlobDetector::create(blobParams);


    Mat openStrel = getStructuringElement(cv::MORPH_RECT, Size(10, 10));
    Mat closeStrel = getStructuringElement(cv::MORPH_RECT, Size(20, 20));


    mTracker = new MotionTracker(pBackSub, pBlobDetector, openStrel, closeStrel, fps);
    Mat frame = cv::Mat::zeros(height, width, CV_8UC3), flipImg;// , detectFrame, mask;
    //std::vector<KeyPoint> detectedCentroids, trackedCentroids;

    // For some reason the CODEC needs a few seconds to gather itself before streaming, maybe to flush
    // after it initializes?
    if (codec != "none")
    {
        std::cout << "Flushing CODEC..." << std::endl;
        Sleep(3000);
    }


    /******************** Video Processor Thread Setup ********************/
    cv::Mat frameVid(height, width, CV_8UC3);
    std::thread vidProc_Thread;
    vidProc_Thread = std::thread(processVideo, frameVid);



    /******************** Primary Application Loop ********************/
    // Loop forever getting video frames, putting them in the circular buffer.
    bool success = false;
    while (!exitProgram)
    {
        vidCam >> frame;
        frame = flipMat(frame);

        // Loop until we can get a lock to put frame into queue
		do
		{
			qFrameRaw_mutex.lock();
			success = qFrameRaw.enQueue(frame);
            qFrameRaw_mutex.unlock(); 

		} while (!success && !exitProgram);



        // Note:
        // Right now I'm leaving the "image show" in the video processing loop. This is because both the TCP receiving AND the encoding (if used) occur
        // in this loop whenever a frame is read. So even though reading a frame is in theory light work, there's a lot going on behind that I'd rather 
        // leave in a thread separate from showing the video



        //imshow("Video Stream", frame);
        //char c = (char)waitKey(25);
        //if (c == 27)
        //    break;

    }

    delete mTracker;
    vidProc_Thread.join();
}


/*
 * cv::Mat flipMat(const cv::Mat& inImage)
 *
 * Description:
 * This functions horizontall and vertically flips an OpenCv matrix. It's convienient for when a camera
 * is upside down.
 * 
 *
 * Inputs:
 *		cv::Mat& inFrame			    input matrix to flip
 * 
 * Outputs:
 *		cv::Mat (return type)           flipped matrix
 */
cv::Mat flipMat(const cv::Mat& inImage)
{
    cv::Mat outImage = cv::Mat::zeros(inImage.rows, inImage.cols, CV_8UC3);

    for (int row = 0; row < inImage.rows; row++)
    {
        for (int col = 0; col < inImage.cols; col++)
        {
            outImage.at<cv::Vec3b>((inImage.rows - 1) - row, (inImage.cols - 1) - col) = inImage.at<cv::Vec3b>(row, col);
        }
    }

    return outImage;
}


/*
 * void processVideo(cv::Mat& frameIn)
 *
 * Description:
 * This function performs the video processing (in this case motion tracking). The frames are pulled from a global circular buffer 
 * processed, and then the output put in to a separate global circular buffer (or optionally displayed).
 *
 *
 * Inputs:
 *		cv::Mat& frameIn			    OpenCV reference matrix. All size / type info in processVideo is derived from this.
 *
 * Outputs:
 *		cv::Mat (return type)           flipped matrix
 */
void processVideo(cv::Mat frameIn)
{
    bool success = false;
    cv::Mat mask, detectFrame;
    std::vector<KeyPoint> detectedCentroids, trackedCentroids;

    while (!exitProgram)
    {
        // Loop until we can get a lock to put frame into queue
        do
        {
            qFrameRaw_mutex.lock();
            success = qFrameRaw.deQueue(frameIn);
            qFrameRaw_mutex.unlock();
        } while (!success);

        mTracker->detect(frameIn, mask, detectedCentroids);
        mTracker->predictNewLocationsOfTracks();
        mTracker->getCentroids(trackedCentroids);
        mTracker->assignDetectionsToTracks(detectedCentroids, 200.0);
        mTracker->deleteLostTracks();

        drawKeypoints(frameIn, trackedCentroids, detectFrame, Scalar(0, 0, 255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        drawKeypoints(detectFrame, detectedCentroids, detectFrame, Scalar(0, 255, 255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        imshow("blobs", detectFrame);

        drawKeypoints(mask, trackedCentroids, detectFrame, Scalar(0, 0, 255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        drawKeypoints(detectFrame, detectedCentroids, detectFrame, Scalar(0, 255, 255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        imshow("mask", detectFrame);
        //imshow("mask", frameIn);
        char c = (char)waitKey(1);
        if (c == 27)
        {
            exitProgram = true;
        }
    }
}
