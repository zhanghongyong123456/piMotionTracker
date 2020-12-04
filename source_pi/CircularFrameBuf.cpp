/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 11/14/2020
 *
 * Description:
 * This is the functional code for two ciruclar buffers. They are identical except for the type of data they hold.
 * All of the code has been taken directly from (with light modifications):
 * https://www.geeksforgeeks.org/circular-queue-set-1-introduction-array-implementation/
 *
 * The two circular queues hold (1) cv::Mat frames and (2) "struct packet" type.
 *
 */

#include "CircularFrameBuf.h"


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
bool QueueMat::enQueue(cv::Mat& frame)
{
    if ((front == 0 && rear == size - 1) ||
        (rear == (front - 1) % (size - 1)))
    {
#ifdef DEBUG
        std::cout << "Frame queue is Full" << std::endl;
#endif
        return false;
    }

    else if (front == -1) /* Insert First Element */
    {
        front = rear = 0;
        buffer[rear] = frame.clone();
    }

    else if (rear == size - 1 && front != 0)
    {
        rear = 0;
        buffer[rear] = frame.clone();
    }

    else
    {
        rear++;
        buffer[rear] = frame.clone();
    }

    return true;
}


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
bool QueueMat::deQueue(cv::Mat& frame)
{
    if (front == -1)
    {
#ifdef DEBUG
        std::cout << "Frame queue is Empty" << std::endl;
#endif
        return false; 
    }

    buffer[front].copyTo(frame);

    if (front == rear)
    {
        front = -1;
        rear = -1;
    }
    else if (front == size - 1)
        front = 0;
    else
        front++;

    return true;
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
bool QueuePkt::enQueue(packet& pkt)
{
    if ((front == 0 && rear == size - 1) ||
        (rear == (front - 1) % (size - 1)))
    {
#ifdef DEBUG
        std::cout << "Pkt queue is Full" << std::endl;
#endif
        return false;
    }

    else if (front == -1) /* Insert First Element */
    {
        front = rear = 0;
        buffer[rear].size = pkt.size;
        memcpy(buffer[rear].buffer, pkt.buffer, pkt.size);
    }

    else if (rear == size - 1 && front != 0)
    {
        rear = 0;
        buffer[rear].size = pkt.size;
        memcpy(buffer[rear].buffer, pkt.buffer, pkt.size);
    }

    else
    {
        rear++;
        buffer[rear].size = pkt.size;
        memcpy(buffer[rear].buffer, pkt.buffer, pkt.size);
    }

    return true;
}


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
bool QueuePkt::deQueue(packet& pkt)
{
    if (front == -1)
    {
#ifdef DEBUG
        std::cout << "Pkt queue is Empty" << std::endl;
#endif
        return false;
    }

    pkt.size = buffer[front].size;
    memcpy(pkt.buffer, buffer[front].buffer, buffer[front].size);

    if (front == rear)
    {
        front = -1;
        rear = -1;
    }
    else if (front == size - 1)
        front = 0;
    else
        front++;

    return true;
}
