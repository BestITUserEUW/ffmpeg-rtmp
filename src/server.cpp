#include <print>

#include <opencv2/opencv.hpp>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "enchantum.hpp"

using std::println;

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL;
static const char *src = "rtmp://0.0.0.0:8080/live";
static const char *sink = "test.mp4";
static FILE *video_dst_file = NULL;

static uint8_t *video_dst_data[4] = {NULL};
static int video_dst_linesize[4];
static int video_dst_bufsize;

static int video_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket *pkt = NULL;
static int video_frame_count = 0;

static int output_video_frame(AVFrame *frame) {
    if (frame->width != width || frame->height != height || frame->format != pix_fmt) {
        /* To handle this change, one could call av_image_alloc again and
         * decode the following frames into another rawvideo file. */
        fprintf(stderr,
                "Error: Width, height and pixel format have to be "
                "constant in a rawvideo file, but the width, height or "
                "pixel format of the input video changed:\n"
                "old: width = %d, height = %d, format = %s\n"
                "new: width = %d, height = %d, format = %s\n",
                width, height, av_get_pix_fmt_name(pix_fmt), frame->width, frame->height,
                av_get_pix_fmt_name((AVPixelFormat)frame->format));
        return -1;
    }

    printf("video_frame n:%d\n", video_frame_count++);

    /* copy decoded frame to destination buffer:
     * this is required since rawvideo expects non aligned data */
    av_image_copy2(video_dst_data, video_dst_linesize, frame->data, frame->linesize, pix_fmt, width, height);

    /* write to rawvideo file */
    fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
    return 0;
}

static int decode_packet(AVCodecContext *dec, const AVPacket *pkt) {
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) return 0;

            fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        // write the frame data to output file
        if (dec->codec->type == AVMEDIA_TYPE_VIDEO) {
            ret = output_video_frame(frame);
        }
        av_frame_unref(frame);
    }

    return ret;
}

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx,
                              AVFormatContext *fmt_ctx,
                              enum AVMediaType type) {
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n", av_get_media_type_string(type), src);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        println("Incoming stream media_type={} codec={}", av_get_media_type_string(type),
                enchantum::to_string(st->codecpar->codec_id));

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n", av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

int main() {
    AVDictionary *options = NULL;

    pkt = av_packet_alloc();
    if (!pkt) {
        println("Could not allocate packet");
        return 1;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        goto end;
    }

    int ret;
    if ((ret = av_dict_set(&options, "listen", "1", 0)) < 0) {
        println("Failed to set listen mode for server");
        goto end;
    }

    if ((ret = av_dict_set(&options, "rtmp_buffer", "1000", 0)) < 0) {
        println("Failed to set listen mode for server");
        goto end;
    }

    ret = avformat_open_input(&fmt_ctx, src, nullptr, &options);
    if (ret < 0) {
        println("Failed to open server");
        return 1;
    }
    println("Connection opened");

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        println("Could not find stream information");
        return 1;
    }

    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];

        video_dst_file = fopen(sink, "wb");
        if (!video_dst_file) {
            fprintf(stderr, "Could not open destination file %s\n", sink);
            ret = 1;
            goto end;
        }

        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        ret = av_image_alloc(video_dst_data, video_dst_linesize, width, height, pix_fmt, 1);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate raw video buffer\n");
            goto end;
        }
        video_dst_bufsize = ret;
    }

    av_dump_format(fmt_ctx, 0, src, 0);

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_idx)
            ret = decode_packet(video_dec_ctx, pkt);
        else {
            println("Ignoring stream_index={}", pkt->stream_index);
        }
        av_packet_unref(pkt);
        if (ret < 0) break;
    }

    std::print(
        "Play the output video file with the command:\n"
        "ffplay -f rawvideo -video_size {}x{} {}\n",
        width, height, sink);
    println("Exiting");
    return 0;
end:
    avcodec_free_context(&video_dec_ctx);
    avformat_close_input(&fmt_ctx);
    if (video_dst_file) fclose(video_dst_file);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);

    return ret < 0;
}