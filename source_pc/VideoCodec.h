/*
 * Author:  Jordan Leiker
 * Class: ECE6122
 * Last Date Modified: 11/09/2020
 *
 * Description:
 * This is the header file for the VideoCodec class. The VideoCodec class is a container for many FFMPEG functions
 * that make using FFMPEG for video encoding/decoding much easier.
 * 
 * FFMPEG is written entirely in C, so on top of abtracting away many of the intializations needed, this class enables
 * convienient and readable OOP programming with FFMPEG.
 *
 */

#pragma once
#include <string>
#include <iostream>
#include <opencv2/opencv.hpp>

// FFMPEG is in native so, so need the extern "C" to compile
extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavutil/opt.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
}


/*
 * class VideoCodec
 *
 * The VideoCodec class is a container for many FFMPEG functions that make using FFMPEG for video encoding/decoding much easier.
 * 
 * FFMPEG is written entirely in C, so on top of abtracting away many of the intializations needed, this class enables
 * convienient and readable OOP programming with FFMPEG.
 *
 */
class VideoCodec
{
	/********** Private Members **********/
	// empty.

protected:
	/********** Protected Members **********/

	AVPixelFormat frameFormat; // Colorspace/format of frames coming in to encode/out of decode methods
	AVPixelFormat codecFormat; // Colorspace/format of frames through encoder/decoder (i.e. what the codecs use for compression)

	const char* codecName; // FYI: encoders(h264_nvenc (gpu hard codec resources), libx264 (software)) | decoders (h264 (uses gpu), h264_cuvid (uses gpu hard codec resources)
	const AVCodec* codec;
	AVCodecContext* ctx;

	int width;
	int height;
	int fps;

	struct SwsContext* swsCtx; // for convering between OpenCV Mat and FFMPEG AVFrame

	AVPacket* pkt; // compressed information packet
	AVFrame* frame; // frame for OpenCV/FFMPEG conversions



public:
	/********** Public Members **********/
	bool isFree;

	/*
	 * Delete default constructor. Do NOT allow users to use the
	 * class without providing some information
	 */
	VideoCodec() = delete;

	/*
	 * VideoCodec(void) :
	 *
	 * Description:
	 * Default constructor for VideoCodec class. 
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	VideoCodec(const char* strCodecName, const AVPixelFormat pixFrameFormat, const AVPixelFormat pixCodecFormat,
			   const unsigned int uintWidth, const unsigned int uintHeight, const unsigned int uintFps) :
		frameFormat(pixFrameFormat),
		codecFormat(pixCodecFormat), // AV_PIX_FMT_NV12; //AV_PIX_FMT_YUV420
		codecName(strCodecName),
		width(uintWidth),
		height(uintHeight),
		fps(uintFps),
		isFree(true)
		 
	{
		//// Init FFMPEG. This loads all the codec info in the system
		avcodec_register_all();


		//// Set up general structures to be used by either encoder or decoder
		// Allocate an FFMPEG packet. This is used to store encoded/compressed information
		pkt = av_packet_alloc();
		if (!pkt)
			exit(1);


		//// Allocate an FFMPEG frame. This is used to to move an OpenCV mat to/from FFMPEG land.
		frame = av_frame_alloc();
		if (!frame) 
		{
			std::cerr << "ERR - Could not allocate video frame" << std::endl;
			exit(1);
		}
		frame->format = codecFormat;
		frame->width = width;
		frame->height = height;

		// Initialize the frame data and tie a memory/buffer space to the allocated frame.
		int ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) 
		{
			std::cerr << "ERR - Could not allocate the video frame data" << std::endl;
			exit(1);
		}
	}


	/*
	 * ~VideoCodec(void) :
	 *
	 * Description:
	 * Destructor VideoCodec class.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	~VideoCodec(void)
	{
		avcodec_free_context(&ctx);
		av_frame_free(&frame);
		av_packet_free(&pkt);
		sws_freeContext(swsCtx);
	}
};


/*
 * class Encoder
 * (derived from VideoCodec)
 *
 * The Encoder class takes in OpenCV Mat frames, encodes them using FFMPEG acceleration (and the users choice of encoder, e.g.
 * libx264 (software encoder), h264_nvenc (NVIDIA GPU CODEC), etc). The output of the encoder is an FFMPEG packet.
 *
 */
class Encoder : public VideoCodec
{	
	/********** Private Members **********/
	unsigned long frameIdx;


public:
	/********** Public Members **********/

	/*
	 * Delete default constructor. Do NOT allow users to use the
	 * class without providing some information
	 */
	Encoder() = delete;


	/*
	  * Encoder() :
	  *
	  * Description:
	  * TODO.
	  *
	  * Inputs:
	  *		N/A
	  *
	  * Outputs:
	  *		N/A
	  */
	Encoder(const char* strCodecName, const AVPixelFormat pixFrameFormat, const AVPixelFormat pixCodecFormat,
		    const unsigned int uintWidth, const unsigned int uintHeight, const unsigned int uintFps) : 
		VideoCodec(strCodecName, pixFrameFormat, pixCodecFormat, uintWidth, uintHeight, uintFps)
	{
		frameIdx = 0;
		
		//// ENCODER 
		//// Setup Codec Context. 
		// (1) find pointer to codec
		// (2) allocate context
		// (3) fill out context with codec info.
		// (4) open codec
		codec = avcodec_find_encoder_by_name(codecName);
		if (!codec)
		{
			std::cerr << "ERR - Codec not found" << std::endl;
			exit(1);
		}

		ctx = avcodec_alloc_context3(codec);
		if (!ctx) 
		{
			std::cerr << "ERR - Could not allocate video codec context" << std::endl;
			exit(1);
		}

		ctx->bit_rate = int(float(width * height * fps * 24.0) / 10.0); // TODO figure out a better method for picking bit rate
		ctx->width = width;
		ctx->height = height;
		ctx->time_base = AVRational({ 1, fps });
		ctx->framerate = AVRational({ fps, 1 });
		/* emit one intra frame every ten frames
		 * check frame pict_type before passing frame
		 * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
		 * then gop_size is ignored and the output of encoder
		 * will always be I frame irrespective to gop_size
		 */
		ctx->gop_size = 10;
		ctx->max_b_frames = 1;
		ctx->pix_fmt = codecFormat;

		if (codec->id == AV_CODEC_ID_H264) 
		{
			av_opt_set(ctx->priv_data, "preset", "slow", 0);
		}
		int ret = avcodec_open2(ctx, codec, NULL);
		if (ret < 0) 
		{
			std::cerr << "ERR - Could not open codec" << std::endl;
			exit(1);
		}

		//// Setup Software Rescaler. This is used to convert from OpenCV mat's in BGR24 format 
		//	 to whatever format our encoder is using
		swsCtx = NULL;
		swsCtx = sws_getContext(
			width,
			height,
			frameFormat,
			width,
			height,
			codecFormat,
			NULL, NULL, NULL, NULL);

		if (!swsCtx)
		{
			std::cerr << "ERR - fail to initialize software rescale context" << std::endl;
			exit(1);
		}
	}


	/*
	 * Encoder() :
	 *
	 * Description:
	 * Destructor Encoder class.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	~Encoder(void)
	{
		int ret = avcodec_send_frame(ctx, NULL); // flush encoder
	}


	/*
	 * void convertFrame_CV2AV(const cv::Mat& frameCV, AVFrame* frameAV)
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
	void convertFrame_CV2AV(cv::Mat& frameCV, AVFrame* frameAV);


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
	  *     bool (return type)          indicates pktAV is valid (i.e. there was info to compress and we get a packet)
	  */
	bool encodeFrame(AVFrame* frameAV, AVPacket* pktAV);


	/*
	 * bool encode(cv::Mat& frameCV, AVPacket* pktAV)
	 *
	 * Description:
	 * Encode an OpenCV Mat to an FFMPEG packet using the encoder context defined in constructor.
	 *
	 * Inputs:
	 *		cv::Mat& frameCV				OpencV Video Frame
	 *
	 * Outputs:
	 *		AVPacket* pktAV				Encoded FFMPEG Video packet
	 *		bool (return type)			indicates pktAV is valid (i.e. there was info to compress and we get a packet)
	 */
	bool encode(cv::Mat& frameCV, AVPacket* pktAV);

};



/*
 * class Decoder
 * (derived from VideoCodec)
 *
 * The Decoder class takes in FFMpeg packets, decodes them using FFMPEG acceleration (and the users choice of decoder, e.g.
 * h264 (GPU accelerated), h264_cuvid (NVIDIA GPU CODEC), etc). The output of the decoder is an OpenCV Mat frame.
 *
 */
class Decoder : public VideoCodec
{
	/********** Private Members **********/
	AVCodecParserContext* parser; // gathers incoming packets until it can form a frame
	AVFrame* tmpFrame; // frame to assist with AV->CV conversion

	AVPacket* pktParse; // A packet to keep track of where we are while parsing

public:
	/********** Public Members **********/

	/*
	 * Delete default constructor. Do NOT allow users to use the
	 * class without providing some information
	 */
	Decoder() = delete;


	/*
	 * Decoder() :
	 *
	 * Description:
	 * TODO.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	Decoder(const char* strCodecName, const AVPixelFormat pixFrameFormat, const AVPixelFormat pixCodecFormat,
		    const unsigned int uintWidth, const unsigned int uintHeight, const unsigned int uintFps) :
		VideoCodec(strCodecName, pixFrameFormat, pixCodecFormat, uintWidth, uintHeight, uintFps)
	{
		//// DECODER 
		//// Setup Codec Context. 
		// (1) find pointer to codec
		// (2) allocate context
		// ( ) No need to fill out contex, this is all derived from incoming packets in the FFMPEG code
		// (4) open codec
		codec = avcodec_find_decoder_by_name(codecName);
		if (!codec) 
		{
			std::cerr << "ERR - Codec not found" << std::endl;
			exit(1);
		}

		ctx = avcodec_alloc_context3(codec);
		if (!ctx) 
		{
			std::cerr << "ERR - Could not allocate video codec context" << std::endl;
			exit(1);
		}

		if (codec->id == AV_CODEC_ID_H264) 
		{
			av_opt_set(ctx->priv_data, "preset", "slow", 0);
		}

		int ret = avcodec_open2(ctx, codec, NULL);
		if (ret < 0) 
		{
			std::cerr << "ERR - Could not open codec" << std::endl;
			exit(1);
		}

		//// Setup Software Rescaler. This is used to convert from whatever format our decoder is using
		//	 to  OpenCV mat's in BGR24 format 
		swsCtx = NULL;
		swsCtx = sws_getContext(
			width,
			height,
			codecFormat,
			width,
			height,
			frameFormat,
			NULL, NULL, NULL, NULL);

		if (!swsCtx) 
		{
			std::cerr << "ERR - fail to initialize software rescale context" << std::endl;
			exit(1);
		}


		//// Setup packet parser
		parser = av_parser_init(codec->id);
		if (!parser) 
		{
			std::cerr << "ERR - parser not found" << std::endl;
			exit(1);
		}

		// Allocate an FFMPEG packet. This is used to store keep track of currently parsed packets
		pktParse = av_packet_alloc();
		if (!pktParse)
			exit(1);

		// Tmp frame to hold FFMPEG frame (codec format) -> temp FFMPEG (frame format) -> OpenCV Mat (frame format)
		tmpFrame = av_frame_alloc();
		if (!tmpFrame)
		{
			std::cerr << "ERR - Could not allocate conversion video frame" << std::endl;
			exit(1);
		}
		tmpFrame->format = frameFormat;
		tmpFrame->width = width;
		tmpFrame->height = height;
		ret = av_frame_get_buffer(tmpFrame, 0);
		if (ret < 0)
		{
			std::cerr << "ERR - Could not allocate the conversion video frame data" << std::endl;
			exit(1);
		}
	}


	/*
	 * Decoder() :
	 *
	 * Description:
	 * Destructor Decoder class.
	 *
	 * Inputs:
	 *		N/A
	 *
	 * Outputs:
	 *		N/A
	 */
	~Decoder(void)
	{
		int ret = avcodec_send_packet(ctx, NULL); //flush decoder
		av_packet_free(&pktParse);
		av_frame_free(&tmpFrame);
		av_parser_close(parser);
	}


	/*
	 * void convertFrame_AV2CV(AVFrame* frameAV, const cv::Mat& frameCV);
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
	void convertFrame_AV2CV(AVFrame* frameAV, cv::Mat& frameCV);


	/*
	 * bool decodePacket(AVPacket* pktAV, AVFrame* frameAV)
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
	bool decodePacket(AVPacket* pktAV, AVFrame* frameAV);
	
	
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
	bool decode(AVPacket* pktAV, cv::Mat& frameCV);
};
