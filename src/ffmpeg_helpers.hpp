#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavcodec/bsf.h>
}

namespace st {

struct AvPacketDeleter {
    void operator()(AVPacket* ptr) const { av_packet_free(&ptr); }
};

struct AvFrameDeleter {
    void operator()(AVFrame* ptr) const { av_frame_free(&ptr); }
};

struct SwsContextDeleter {
    void operator()(SwsContext* ptr) const { sws_freeContext(ptr); }
};

struct AvCodecParserContextDeleter {
    void operator()(AVCodecParserContext* ptr) const { av_parser_close(ptr); }
};

struct AvCodecContextDeleter {
    void operator()(AVCodecContext* ptr) const { avcodec_free_context(&ptr); }
};

struct AvBSFContextDeleter {
    void operator()(AVBSFContext* ptr) const { av_bsf_free(&ptr); }
};

using AvPacketPtr = std::unique_ptr<AVPacket, AvPacketDeleter>;
using AvFramePtr = std::unique_ptr<AVFrame, AvFrameDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
using AvCodecParserContextPtr = std::unique_ptr<AVCodecParserContext, AvCodecParserContextDeleter>;
using AvCodecContextPtr = std::unique_ptr<AVCodecContext, AvCodecContextDeleter>;
using AvBSFContextPtr = std::unique_ptr<AVBSFContext, AvBSFContextDeleter>;

inline auto MakeAvPacketPtr() { return AvPacketPtr(av_packet_alloc()); }
inline auto MakeAvFramePtr() { return AvFramePtr(av_frame_alloc()); }
inline auto MakeAvCodecParserContextPtr(AVCodecID id) { return AvCodecParserContextPtr(av_parser_init(id)); }
inline auto MakeAvCodecContextPtr(const AVCodec* codec) { return AvCodecContextPtr(avcodec_alloc_context3(codec)); }
inline auto MakeAvBSFContextPtr(const AVBitStreamFilter* filter) {
    AVBSFContext* ctx;
    if (av_bsf_alloc(filter, &ctx) < 0) {
        return AvBSFContextPtr(nullptr);
    }
    return AvBSFContextPtr(ctx);
}

}  // namespace st