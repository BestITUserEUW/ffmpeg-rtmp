#include "libav_helpers.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
}

namespace st::libav {

namespace detail {

void DictionaryDeleter::operator()(AVDictionary* ptr) const { av_dict_free(&ptr); }
void PacketDeleter::operator()(AVPacket* ptr) const { av_packet_free(&ptr); }
void FrameDeleter::operator()(AVFrame* ptr) const { av_frame_free(&ptr); }
void SwsContextDeleter::operator()(SwsContext* ptr) const { sws_freeContext(ptr); }
void CodecParserContextDeleter::operator()(AVCodecParserContext* ptr) const { av_parser_close(ptr); }
void CodecContextDeleter::operator()(AVCodecContext* ptr) const { avcodec_free_context(&ptr); }
void BsfContextDeleter::operator()(AVBSFContext* ptr) const { av_bsf_free(&ptr); }
void FormatContextDeleter::operator()(AVFormatContext* ptr) const { avformat_free_context(ptr); }
void IoContextDeleter::operator()(AVIOContext* ptr) const { avio_close(ptr); }

}  // namespace detail

auto MakeUniquePacket() -> UniquePacketPtr { return UniquePacketPtr(av_packet_alloc()); }

auto MakeUniqueFrame() -> UniqueFramePtr { return UniqueFramePtr(av_frame_alloc()); }

auto MakeUniqueCodecParserContext(int codec_id) -> UniqueCodecParserContextPtr {
    return UniqueCodecParserContextPtr(av_parser_init(codec_id));
}

auto MakeUniqueCodecContext(const AVCodec* codec) -> UniqueCodecContextPtr {
    return UniqueCodecContextPtr(avcodec_alloc_context3(codec));
}

auto MakeUniqueBsfContext(const AVBitStreamFilter* filter) -> UniqueBsfContextPtr {
    AVBSFContext* ctx;
    if (av_bsf_alloc(filter, &ctx) < 0) {
        return nullptr;
    }
    return UniqueBsfContextPtr(ctx);
}

auto GetSwsConvertFormatContext(int src_format, int dst_format, int width, int height, int flags)
    -> UniqueSwsContextPtr {
    return UniqueSwsContextPtr(sws_getContext(width, height, static_cast<AVPixelFormat>(src_format), width, height,
                                              static_cast<AVPixelFormat>(dst_format), flags, nullptr, nullptr,
                                              nullptr));
}

}  // namespace st::libav