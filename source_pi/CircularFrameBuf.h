/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 11/14/2020
 *
 * Description:
 * This is the header for two ciruclar buffers. The are identical except for the type of data they hold.
 * All of the code has been taken directly from (with light modifications):
 * https://www.geeksforgeeks.org/circular-queue-set-1-introduction-array-implementation/
 * 
 * The two circular queues hold (1) cv::Mat frames and (2) "struct packet" type.
 * 
 */


#include <opencv2/opencv.hpp>
#include <vector>

//#define DEBUG


/*
* struct QueueMat
*
* Description:
* A structure implementating a circular queue for OpenCV Mat types.
* Queue is initialized at construction. The queue contains actual copies of the 
* data, NOT pointers.
* 
*/
struct QueueMat
{
    // Initialize front and rear 
    int rear, front;

    // Circular Queue 
    int size;
    std::vector<cv::Mat> buffer;


    /*
     * QueueMat(int s)
     *
     * Description:
     * Constructor for queue. Initialzation Ex.:
     * QueuePkt qBuf(64);
     *
     * Inputs:
     *		int s       Size of queue
     *
     * Outputs:
     *		N/A
     */
    QueueMat(int s)
    {
        front = rear = -1;
        size = s;
        buffer.resize(size);
    }


    /*
     * bool enQueue(cv::Mat& frame);
     *
     * Description:
     * Struct function to add items to the queue
     *
     * Inputs:
     *		cv::Mat& frame       frame to copy into queue
     *
     * Outputs:
     *		bool (return val)    indicates success of placing into queue
     */
    bool enQueue(cv::Mat& frame);


    /*
     * bool deQueue(cv::Mat& frame);
     *
     * Description:
     * Struct function to remove items from the queue
     *
     * Inputs:
     *		cv::Mat& frame       frame to store removed queue item
     *
     * Outputs:
     *		bool (return val)    indicates success of removing from queue
     */
    bool deQueue(cv::Mat& frame);
};



/*
 * struct packet
 *
 * Description:
 * A very simple struct that holds a buffer and the size of the data in the buffer. this is meant to be a low 
 * overhead method for queueing FFMPEG AVFrame data and sizes.
 * 
 * Note: Size of buffer is hardcoded for now. No reason to make it larger until FFMPEG codec issues get resolved.
 *
 */
struct packet
{
    int size;
    char* buffer = new char[480 * 640 * 3];
};



/*
 * struct QueuePkt
 *
 * Description:
 * A structure implementating a circular queue for packet types.
 * Queue is initialized at construction. The queue contains actual copies of the
 * data, NOT pointers.
 * 
 */
struct QueuePkt
{
    // Initialize front and rear 
    int rear, front;

    // Circular Queue 
    int size;
    std::vector<packet> buffer;


    /*
     * QueuePkt(int s)
     *
     * Description:
     * Constructor for queue. Initialzation Ex.:
     * QueuePkt qBuf(64);
     *
     * Inputs:
     *		int s       Size of queue
     *
     * Outputs:
     *		N/A
     */
    QueuePkt(int s)
    {
        front = rear = -1;
        size = s;
        buffer.resize(size);
    }


    /*
     * bool enQueue(packet& pkt);
     *
     * Description:
     * Struct function to add items to the queue
     *
     * Inputs:
     *		packet& pkt          packet to copy into queue
     *
     * Outputs:
     *	    bool (return val)    indicates success of placing into queue
     */
    bool enQueue(packet& pkt);


    /*
     * bool deQueue(packet& pkt);
     *
     * Description:
     * Struct function to remove items from the queue
     *
     * Inputs:
     *		packet& pkt          packet to store removed queue item
     *
     * Outputs:
     *		bool (return val)    indicates success of removing from queue
     */
    bool deQueue(packet& pkt);
};