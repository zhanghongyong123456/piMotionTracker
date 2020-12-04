/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 11/09/2020
 *
 * Description:
 * This is the functional code for the VideoCodec class. The VideoCodec class is a container for many FFMPEG functions
 * that make using FFMPEG for video encoding/decoding much easier.
 *
 * FFMPEG is written entirely in C, so on top of abtracting away many of the intializations needed, this class enables
 * convienient and readable OOP programming with FFMPEG.
 *
 */

#include "VideoCodec.h"


/*
 * void convertFrame_CV2AV(cv::Mat& frameCV, AVFrame* frameAV)
 *
 * Description:
 * Converts an OpenCV Mat to an FFMPEG AVFrame.
 *
 * Inputs:
 *		const cv::Mat& frameCV		OpenCV Video Frame
 *
 * Outputs:
 *      AVFrame* frameAV			FFMPEG Video Frame
 */
void Encoder::convertFrame_CV2AV(cv::Mat& frameCV, AVFrame* frameAV)
{
    // make sure the frame data is writable 
    // i.e. has a buffer/memory tied to it and initialized
    int ret = av_frame_make_writable(frameAV);
    if (ret < 0)
        exit(1);

    frameAV->pts = frameIdx++;

    const int stride[] = { static_cast<int>(frameCV.step[0]) };
    sws_scale(swsCtx, &frameCV.data, stride, 0, frameCV.rows, frameAV->data, frameAV->linesize);
}


/*
 * bool encodeFrame(AVFrame* frameAV, AVPacket* pktAV)
 *
 * Description:
 * Encode an FFMPEG AVFrame to a packet using the encoder context defined in constructor.
 *
 * Inputs:
 *		AVFrame* frameAV			FFMPEG Video Frame
 *
 * Outputs:
 *		AVPacket* pktAV				Encoded FFMPEG Video packet
 *      bool (return type)          indicates pktAV is valid (i.e. there was info to compress and we get a packet)
 */
bool Encoder::encodeFrame(AVFrame* frameAV, AVPacket* pktAV)
{   
    // Send frame to encoder
    int ret = avcodec_send_frame(ctx, frameAV);
    if (ret < 0) 
    {
        std::cerr << "Error sending a frame for encoding" << std::endl;
        exit(1);
    }

    // Get encoded packet back from encoder
    while (ret >= 0) 
    {
        ret = avcodec_receive_packet(ctx, pktAV);
        if (ret == 0)
            return true; //packet received, move on
        else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return false; // nothing to receive, move on
        else if (ret < 0) 
        {
            std::cerr << "Error during encoding" << std::endl;
            exit(1);
        }
    }
}


/*
 * bool encode(cv::Mat& frameCV, AVPacket* pktAV)
 *
 * Description:
 * Encode an OpenCV Mat to an FFMPEG packet using the encoder context defined in constructor.
 * This is just a container that abstracts the conversion from OpenCV -> frame before encoding.
 *
 * Inputs:
 *		cv::Mat frameCV				OpencV Video Frame
 *
 * Outputs:
 *		AVPacket* pktAV				Encoded FFMPEG Video packet
 *		bool (return type)			indicates pktAV is valid (i.e. there was info to compress and we get a packet)
 */
bool Encoder::encode(cv::Mat& frameCV, AVPacket* pktAV)
{
    convertFrame_CV2AV(frameCV, frame);
    return encodeFrame(frame, pktAV);
}


/*
 * void convertFrame_AV2CV(AVFrame* frameAV, cv::Mat& frameCV);
 *
 * Description:
 * Converts an FFMPEG AVFrame to an OpenCV Mat.
 *
 * Inputs:
 *      AVFrame* frameAV			FFMPEG Video Frame
 *
 * Outputs:
 *		const cv::Mat& frameCV		OpenCV Video Frame
 */
void Decoder::convertFrame_AV2CV(AVFrame* frameAV, cv::Mat& frameCV)
{
    // Use the internal Decoder member 'frame' as an intermediary for conversion
    sws_scale(swsCtx, frameAV->data, frameAV->linesize, 0, tmpFrame->height, tmpFrame->data, tmpFrame->linesize);
    cv::Mat image(height, width, CV_8UC3, tmpFrame->data[0], tmpFrame->linesize[0]);
    frameCV = image;
}


/*
 * void decodePacket(AVPacket* pktAV, AVFrame* frameAV)
 *
 * Description:
 * Decode an FFMPEG packet using the decoder context defined in the constructor.
 *
 * Inputs:
 *		AVPacket* pktAV				Encoded FFMPEG Video packet
 *
 * Outputs:
 *		AVFrame* frameAV			FFMPEG Video Frame
 *      bool (return type)          indicates frameAV is valid (i.e. there was info to uncompress and we get a frame)
 */
bool Decoder::decodePacket(AVPacket* pktAV, AVFrame* frameAV)
{
    // If we aren't currently in the middle of parsing a packet, then 
    if (isFree)
    {
        isFree = false;
        pktParse->data = pktAV->data;
        pktParse->size = pktAV->size;
    }

    while (pktParse->size > 0)
    {
        // Parse pktAV for packets and put the results in pkt (internal VideoCodec member packet)
        int ret = av_parser_parse2(parser, ctx, &pkt->data, &pkt->size, pktParse->data, pktParse->size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0) 
        {
            std::cerr << "Error while parsing" << std::endl;
            exit(1);
        }
        // Advance pointer to unparsed data and decrement how much is left to parse
        pktParse->data += pkt->size;
        pktParse->size -= pkt->size;

        // If parsed packet is not empty, send to decoder
        if (pkt->size)
        {
            ret = avcodec_send_packet(ctx, pkt);
            if (ret < 0)
            {
                std::cerr << "Error sending a packet for decoding" << std::endl;
                exit(1);
            }

            av_packet_unref(pkt); // done with this packet, free it

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(ctx, frameAV);
                if (ret == 0)
                    return true; //frame received, move on
                else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break; // no frames in parsed packet, so parse some more
                else if (ret < 0) {
                    std::cerr << "Error during decoding" << std::endl;
                    exit(1);
                }
            }
        }
    }

    isFree = true; // done parsing this packet, indicate we are ready for more.
    return false; // if we made it here then we went through all packet data without finding a frame

}


/*
 * bool decode(AVPacket* pktAV, cv::Mat& frameCV)
 *
 * Description:
 * Decode an FFMPEG packet and get an OpenCV Mat using the decoder context defined in constructor.
 * This is just a container that abstracts the conversion from AVframe -> OpenCV Mat before decoding.
 *
 * Inputs: 		
 *      AVPacket* pktAV				Encoded FFMPEG Video packet
 *
 * Outputs:
 *		cv::Mat& frameCV				OpencV Video Frame
 *		bool (return type)			indicates pktAV is valid (i.e. there was info to compress and we get a packet)
 */
bool Decoder::decode(AVPacket* pktAV, cv::Mat& frameCV)
{
    if (decodePacket(pktAV, frame))
    {
        convertFrame_AV2CV(frame, frameCV);	
        return true;
    }
    else
    {
        return false;
    }
}
