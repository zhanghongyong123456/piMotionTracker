/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 10/30/2020
 *
 * Description:
 * This is the header file for the MotionTracker class. It contains all functions needed to analyze a video
 * stream detecting motion, track using a Kalman filter, and draw bounding boxes on the video stream. 
 * 
 * This class and the algorithm are taken directly from a publicly available Matlab motion tracking example
 * and ported to OpenCV. The algorithm and Matlab code can be found here:
 * https://www.mathworks.com/help/vision/ug/motion-based-multiple-object-tracking.html
 * 
 * NOTE: I reused variable names and image processing algorithm settings as much as possible to have a coherent
 *		mapping from the Matlab code to C++.
 *
 */

#pragma once
#include <opencv2/opencv.hpp>

using namespace cv;

// All track related information necessary for maintaing active/lost tracks
struct track {
	unsigned long id;
	KeyPoint centroid;
	KalmanFilter kalmanFilter;
	unsigned long age;
	unsigned long totalVisibleCount;
	unsigned long consecutiveInvisibleCount;
	bool flag;
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
class MotionTracker
{
	/********** Private Members **********/
	// Image Processing Members
	Ptr<BackgroundSubtractorMOG2> pBackSub;
	SimpleBlobDetector::Params blobParams;
	Ptr<SimpleBlobDetector> pBlobDetector;
	Mat openStrel;
	Mat closeStrel; 
	int fps;

	// Motion Tracking Members
	std::vector<track> tracks;
	unsigned long numTracks;


	
public:	
	/********** Public Members **********/
	/*
	 * MotionTracker(void) :
	 *
	 * Description:
	 * Default constructor for MotionTracker class. This constructor sets initial values and will
	 * perform the motion tracking on atrium.mp4 similarly to the Matlab example.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	MotionTracker(void):
		numTracks(0)
	{
		/******************** Background Subtractor Initialization ********************/

		pBackSub = cv::createBackgroundSubtractorMOG2(); // default background subtractor constructor
		pBackSub->setBackgroundRatio(0.7);	// set to match Matlab
		pBackSub->setNMixtures(3); // set to match Matlab



		/******************** Blob Detector Initialization ****************************/

		// Change thresholds
		// The image is separated in to multiple images:
		// 1. thresholded @ minThreshold 
		// 2. thresholded @ minThreshold + thresholdStep
		// 3. ...
		// 4. thresholded @ maxThreshold.
		// Since the image I'm detecting in is not grayscale but B/W
		// I'm choosing to do this in as few steps as possible for speed.
		// Note: Unforunately it doesn't work to do min = 0, max = step. So choosing
		// step to be JUST under max to minimize number of images.
		blobParams.minThreshold = 0;
		blobParams.maxThreshold = 254;
		blobParams.thresholdStep = 253;

		// Sometimes things like trees break up a blob into a bunch of close blobs, so 
		// combine anything within 50
		blobParams.minDistBetweenBlobs = 50;

		// Filter by Area
		// Chosen based on image resolution/etc. This is a manually tuned param
		blobParams.filterByArea = true;
		blobParams.minArea = 400;

		// Color disabled because we're using black/white
		blobParams.filterByColor = false;

		// Shape filters disabled
		// Disable all filters that detect based on shape. These all deal with varying
		// degrees of "how circular the blob is"
		blobParams.filterByCircularity = false;
		blobParams.filterByConvexity = false;
		blobParams.filterByInertia = false;
		
		pBlobDetector = SimpleBlobDetector::create(blobParams);


	
		/************* Morphological Structuring Element Initialization ***************/
	
		openStrel = getStructuringElement(cv::MORPH_RECT, Size(5, 5));
		closeStrel = getStructuringElement(cv::MORPH_RECT, Size(15, 15));
	}


	/*
	 * MotionTracker(cv::Ptr<cv::BackgroundSubtractorMOG2> ptrBackSub, cv::Ptr<cv::SimpleBlobDetector> ptrBlobDetector, cv::Mat openStructEle, cv::Mat closeStructEle):
	 *
	 * Description:
	 * Constructor for full customization of image processing algorithm
	 *
	 * Inputs:
	 *		Ptr<cv::BackgroundSubtractorMOG2> ptrBackSub		Pointer to background subtractor
	 *      Ptr<cv::SimpleBlobDetector> ptrBlobDetector			Pointer to blob detector
	 *      Mat openStructEle									Morphological opening structural element (for noise after foreground estimation/background subtract)
	 *      Mat closeStructEle									Morphological closing structural element (for noise after foreground estimation/background subtract)
	 *		itn fps												the fps of the camera
	 *
	 * Outputs:
	 *		N/A
	 */
	MotionTracker(Ptr<BackgroundSubtractorMOG2> ptrBackSub, Ptr<SimpleBlobDetector> ptrBlobDetector, Mat openStructEle, Mat closeStructEle, int inFps):
		pBackSub(ptrBackSub),
		pBlobDetector(ptrBlobDetector),
		openStrel(openStructEle),
		closeStrel(closeStructEle),
		fps(inFps),
		numTracks(0)
	{
	}


	/*
	 * void detectObject(const Mat& inImage, Mat& outMask, KeyPoint& centroid);
	 *
	 * Description:
	 * Perform motion segmentation using foreground detector, remove noise using morphological open+close, then
	 * find any blobs and return their centers.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		Mat& outMask					Binary mask, same size as input. 255 = foreground, 0 = background
	 *      KeyPoint& centroid				Centroid of detected object.
	 */
	void detect(const Mat& inImage, Mat& outMask, std::vector<KeyPoint>& centroids);


	/*
	 * void predictNewLocationsOfTracks(void);
	 *
	 * Description:
	 * Predict the new locations of each existing trac. This steps the Kalman filter forward to predict the expected
	 * location of the centroid.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	void predictNewLocationsOfTracks(void);


	/*
	 * void createNewTrack(const KeyPoint& centroid)
	 *
	 * Description:
	 * Initializes a new track and adds it to the tracks vector memeber variable
	 * Note : The Kalman filter is configured for constant velocity tracking. See the following for more info:
	 * https://www.mathworks.com/help/vision/ref/configurekalmanfilter.html#d122e144530
	 * https://docs.opencv.org/master/de/d70/samples_2cpp_2kalman_8cpp-example.html#a23
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	void createNewTrack(const KeyPoint& centroid);


	/*
	 * deleteLostTracks(void);
	 *
	 * Description:
	 * Remove tracks that have been invisible for too many consecutive frames.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	void deleteLostTracks(void);


	/*
	 * void getCentroids(std::vector<KeyPoint>& centroids);
	 *
	 * Description:
	 * Returns the current location of all dectected objects / tracks.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		std::vector<KeyPoint> centroids			Centroids of all current tracks
	 */
	void getCentroids(std::vector<KeyPoint>& centroids);



	/*
	 * void MotionTracker::assignDetectionsToTracks(std::vector<KeyPoint> centroids, double distCutoff)
	 *
	 * Description:
	 * Assigns all detections to a "most likely" track, i.e. a track with current position having L2 norm less than distCutoff from detection.
	 * If no paths exist wtihin distCutoff of the detection a new track is created.
	 *
	 * IF the detection IS placed on a track then the Kalman Filter is updated along with all track parameters. Existing tracks that are not
	 * assigned a new detection will have their "invisibility" parameters incremented
	 *
	 * Inputs:
	 *		std::vector<KeyPoint> centroids				vector of detected objects KeyPoints that need placed
	 *		double distCutoff							cutoff length for determining if a detection belongs to a track or not
	 *
	 * Outputs:
	 *		N/A
	 */
	void assignDetectionsToTracks(std::vector<KeyPoint> centroids, double distCutoff);

};