/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 10/30/2020
 *
 * Description:
 * This is the functional code for the MotionTracker class. It contains all functions needed to analyze a video
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

#include "MotionTracker.h"


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
void MotionTracker::detect(const Mat& inImage, Mat& outMask, std::vector<KeyPoint>& centroids)
{
	// Segment the foreground from background
	pBackSub->apply(inImage, outMask);

	// Morpological open/close to remove noise
	morphologyEx(outMask, outMask, MORPH_OPEN, openStrel);
	morphologyEx(outMask, outMask, MORPH_CLOSE, closeStrel);

	// Binary threshold with inversion. The background detector outputs a mask that has the background as 
	// black, objects as white and shadows as gray. So turn objects + shadows to white and keep the background black.
	// Then finally (using the binary_inv mode) invert everything for the blob detector (which finds black blobs on white)
	threshold(outMask, outMask, 1, 255, THRESH_BINARY_INV);

	// Detect blobs / groups of related pixels and return their centroid
	pBlobDetector->detect(outMask, centroids);
}


/*
 * predictNewLocationsOfTracks(void);
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
void MotionTracker::predictNewLocationsOfTracks(void)
{
	// For every detected track run the Kalman filter
	for (auto& track : tracks)
	{
		Mat prediction = track.kalmanFilter.predict();
		track.centroid.pt.x = prediction.at<float>(0);
		track.centroid.pt.y = prediction.at<float>(1);
	}
}


/*
 * void MotionTracker::createNewTrack(const KeyPoint& centroid)
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
void MotionTracker::createNewTrack(const KeyPoint& centroid)
{
	//KalmanFilter kf(4, 2, 0);
	//kf.statePost = (Mat_<float>(4, 1) << centroid.pt.x, centroid.pt.y, 0, 0); // Load state with current location of unassignedDetection (this estimates the starting point for the filter)
	//float dt = 1.0f / float(fps);
	//kf.transitionMatrix = (Mat_<float>(4, 4) << 1, 0, dt, 0, 0, 1, 0, dt, 0, 0, 1, 0, 0, 0, 0, 1); // 'constant velocity'
	KalmanFilter kf(6, 2, 0);
	kf.statePost = (Mat_<float>(6, 1) << centroid.pt.x, centroid.pt.y, 0, 0, 0, 0); // Load state with current location of unassignedDetection (this estimates the starting point for the filter)
	float dt = 2;// 1.0f / float(fps);
	kf.transitionMatrix = (Mat_<float>(6, 6) << 1, 0, dt, 0, dt*dt, 0, 0, 1, 0, dt, 0, dt*dt, 0, 0, 1, 0, dt, 0, 0, 0 ,0 ,1, 0, dt, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1); // 'constant velocity'
	setIdentity(kf.measurementMatrix);
	setIdentity(kf.processNoiseCov, Scalar::all(1e-5));
	setIdentity(kf.measurementNoiseCov, Scalar::all(1e-1));
	setIdentity(kf.errorCovPost, Scalar::all(1));
	
	track newTrack;
	newTrack.kalmanFilter = kf;
	newTrack.id = numTracks++; // Assign to current number and then increment for next track
	newTrack.age = 1;
	newTrack.totalVisibleCount = 1;
	newTrack.consecutiveInvisibleCount = 0;
	newTrack.flag = true;

	// Add to array of tracks
	tracks.push_back(newTrack);
}


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
void MotionTracker::deleteLostTracks(void)
{
	long int invisibleForTooLong = 20;
	long int ageThreshold = 8;


	for (auto track = tracks.begin(); track != tracks.end(); ++track)
	{
		int idx = std::distance(tracks.begin(), track);

		// Invisibility / age heuristics were taken straight from Matlab demo reference throughout 
		int visibility = track->totalVisibleCount / track->age;
		if ((track->age < ageThreshold && visibility < 0.6) || track->consecutiveInvisibleCount >= invisibleForTooLong)
		{
			tracks.erase(tracks.begin() + idx);

			// Because erasing the track alters the vector, I recursively call deleteLostTracks to refresh the iterators with
			// the newly allocated vector. Once all tracks are deleted the recursions will all return consecutively.
			deleteLostTracks();
			return;
		}
	}
}


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
void MotionTracker::getCentroids(std::vector<KeyPoint>& centroids)
{
	std::vector<KeyPoint> pts;
	pts.resize(tracks.size());

	for (auto& track : tracks)
	{
		pts.push_back(track.centroid);
	}

	centroids = pts;
}


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
void MotionTracker::assignDetectionsToTracks(std::vector<KeyPoint> centroids, double distCutoff)
{
	// set all tracks flags to false initially so we can keep track of which tracks get new detections
	for (auto& track : tracks)
		track.flag = false;

	// Cycle through all detected centriods and attempt to place to path
	for (auto& centroid : centroids)
	{
		bool assigned = false;

		// This if/else only needed for intiialization
		if (tracks.size() > 0)
		{
			// Check all tracks to try and assign
			for (auto& track : tracks)
			{
				Mat st(track.kalmanFilter.statePost); // get current position (i.e. the first two kalman filter state variables)
				Point2f curPoint(st.at<float>(0), st.at<float>(1));

				if (norm(curPoint - centroid.pt) < distCutoff)
				{
					track.kalmanFilter.correct((Mat_<float>(2, 1) << centroid.pt.x, centroid.pt.y));
					track.centroid = centroid;
					track.age++;
					track.totalVisibleCount++;
					track.consecutiveInvisibleCount = 0;
					track.flag = true;
					assigned = true;
					break;
				}
			}


			// We checked all tracks but did not assign the detection to any, so create new track
			if (!assigned)
			{
				createNewTrack(centroid);
			}
		}
		else
		{
			createNewTrack(centroid);
		}
	}

	// If a track wasn't assigned a new detection then update the invisible track count (and age)
	for (auto& track : tracks)
	{
		if (!track.flag)
		{
			track.age++;
			track.consecutiveInvisibleCount++;
		}
	}
}
