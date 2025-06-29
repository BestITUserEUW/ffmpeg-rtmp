#include "av_helpers.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace oryx::av {

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

ImageBuffer::ImageBuffer()
    : data(nullptr),
      linesizes(),
      data_size() {}

ImageBuffer::ImageBuffer(ImageSize size, int pix_fmt)
    : data(nullptr),
      linesizes(),
      data_size() {
    data_size = av_image_alloc(data, linesizes, size.width, size.height, static_cast<AVPixelFormat>(pix_fmt), 1);
}

ImageBuffer::~ImageBuffer() {
    if (data[0]) {
        av_freep(&data[0]);
    }
}

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

auto GetSwsConvertFormatContext(int from, int to, ImageSize size, int flags) -> UniqueSwsContextPtr {
    return UniqueSwsContextPtr(sws_getContext(size.width, size.height, static_cast<AVPixelFormat>(from), size.width,
                                              size.height, static_cast<AVPixelFormat>(to), flags, nullptr, nullptr,
                                              nullptr));
}

}  // namespace oryx::av