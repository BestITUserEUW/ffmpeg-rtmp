#pragma once

#include <memory>

#include "image.hpp"

// Forward declare libav stuff so we don't polute our headers
struct AVDictionary;
struct AVPacket;
struct AVFrame;
struct SwsContext;
struct AVCodecParserContext;
struct AVCodecContext;
struct AVBSFContext;
struct AVBitStreamFilter;
struct AVCodec;
struct AVFormatContext;
struct AVIOContext;

namespace oryx::av {

namespace detail {

struct DictionaryDeleter {
    void operator()(AVDictionary* ptr) const;
};

struct PacketDeleter {
    void operator()(AVPacket* ptr) const;
};

struct FrameDeleter {
    void operator()(AVFrame* ptr) const;
};

struct SwsContextDeleter {
    void operator()(SwsContext* ptr) const;
};

struct CodecParserContextDeleter {
    void operator()(AVCodecParserContext* ptr) const;
};

struct CodecContextDeleter {
    void operator()(AVCodecContext* ptr) const;
};

struct BsfContextDeleter {
    void operator()(AVBSFContext* ptr) const;
};

struct FormatContextDeleter {
    void operator()(AVFormatContext* ptr) const;
};

struct IoContextDeleter {
    void operator()(AVIOContext* ptr) const;
};

}  // namespace detail

using UniquePacketPtr = std::unique_ptr<AVPacket, detail::PacketDeleter>;
using UniqueFramePtr = std::unique_ptr<AVFrame, detail::FrameDeleter>;
using UniqueSwsContextPtr = std::unique_ptr<SwsContext, detail::SwsContextDeleter>;
using UniqueCodecParserContextPtr = std::unique_ptr<AVCodecParserContext, detail::CodecParserContextDeleter>;
using UniqueCodecContextPtr = std::unique_ptr<AVCodecContext, detail::CodecContextDeleter>;
using UniqueBsfContextPtr = std::unique_ptr<AVBSFContext, detail::BsfContextDeleter>;
using UniqueDictionaryPtr = std::unique_ptr<AVDictionary, detail::DictionaryDeleter>;
using UniqueFormatContextPtr = std::unique_ptr<AVFormatContext, detail::FormatContextDeleter>;
using UniqueIoContextPtr = std::unique_ptr<AVIOContext, detail::IoContextDeleter>;

// make this move only
struct ImageBuffer {
    ImageBuffer();
    ImageBuffer(ImageSize size, int pix_fmt);
    ~ImageBuffer();

    uint8_t* data[4];
    int linesizes[4];
    int pix_fmt;
    size_t data_size;
};

auto MakeUniquePacket() -> UniquePacketPtr;
auto MakeUniqueFrame() -> UniqueFramePtr;
auto MakeUniqueCodecParserContext(int codec_id) -> UniqueCodecParserContextPtr;
auto MakeUniqueCodecContext(const AVCodec* codec) -> UniqueCodecContextPtr;
auto MakeUniqueBsfContext(const AVBitStreamFilter* filter) -> UniqueBsfContextPtr;
auto GetSwsConvertFormatContext(int from, int to, ImageSize size, int flags) -> UniqueSwsContextPtr;

}  // namespace oryx::av