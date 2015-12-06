// License: GPLv3 or Later

#ifndef FLACUTIL_FLAC_STRUCT_HPP
#define FLACUTIL_FLAC_STRUCT_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "variant.hpp"


// in buffer.hpp
namespace buffer{
    template< typename... >
    class bytestream;
}

namespace FLAC
{

class exception : public std::exception
{
private:
    char const *w;
public:
    exception( char const *what ) noexcept
        : w( what )
    {
    }
    virtual char const *what() const noexcept
    {
        return w;
    }
};

constexpr std::uint16_t MIN_BLOCK_SIZE          = 16;
constexpr std::uint16_t MAX_BLOCK_SIZE          = 65535;
constexpr std::uint8_t  MAX_CHANNELS            = 8;
constexpr std::uint8_t  MIN_BITS_PER_SAMPLE     = 4;
constexpr std::uint8_t  MAX_BITS_PER_SAMPLE     = 32;
constexpr std::uint32_t MAX_SAMPLE_RATE         = 655350;
constexpr std::uint8_t  STREAM_SYNC_STRING[4]   = { 'f', 'L', 'a', 'C' };
constexpr std::uint8_t  MAX_METADATA_TYPE_CODE  = 126;
constexpr std::uint8_t  MAX_LPC_ORDER           = 32;
constexpr std::uint8_t  MIN_QLP_COEFF_PRECISION = 5;
constexpr std::uint8_t  MAX_QLP_COEFF_PRECISION = 15;
constexpr std::uint8_t  MAX_FIXED_ORDER         = 15;
constexpr std::uint16_t FRAME_HEADER_SYNC       = 0x3ffe;

constexpr std::uint32_t STREAMINFO_LENGTH       = 20;

namespace Subframe
{
    enum class EntropyCodingMethodType : std::uint8_t
    {
        PARTITIONED_RICE  = 0,
        PARTITIONED_RICE2 = 1,
    };
    enum class Type : std::uint8_t
    {
        CONSTANT = 0,
        VERBATIM = 1,
        FIXED    = 2,
        LPC      = 3,
    };
    struct PartitionedRice
    {
        std::uint8_t                      order;
        std::unique_ptr< std::uint8_t[] > parameters;
        std::unique_ptr< bool[] >         is_raw_bits;
    };
    struct Residual
    {
        EntropyCodingMethodType           type;
        variant<
            PartitionedRice
        >                                 data;
        std::unique_ptr< std::int64_t[] > residual;
    };
    struct Constant
    {
        std::int32_t value;
    };
    struct Verbatim
    {
        std::unique_ptr< std::int64_t[] > data;
    };
    struct Fixed
    {
        std::uint8_t order;
        std::int64_t warmup[MAX_FIXED_ORDER];
        Residual     residual;
    };
    struct LPC
    {
        std::uint8_t order;
        std::int64_t warmup[MAX_LPC_ORDER];
        std::uint8_t qlp_coeff_precision;
        std::uint8_t quantization_level;
        std::int16_t qlp_coeff[MAX_LPC_ORDER];
        Residual     residual;
    };
    struct Header
    {
        Type         type;
        std::uint8_t type_bits;
        std::uint8_t wasted_bits;
    };
    struct Subframe
    {
        Header header;
        variant<
            Constant,
            Fixed,
            LPC,
            Verbatim
        >      data;
    };
} // namespace Subframe
namespace Frame
{
    enum class ChannelAssignment : std::uint8_t
    {
        INDEPENDENT = 0,
        LEFT_SIDE   = 1,
        RIGHT_SIDE  = 2,
        MID_SIDE    = 3,
    };
    enum class NumberType : std::uint8_t
    {
        FRAME_NUMBER =  0,
        SAMPLE_NUMBER = 1,
    };
    struct Header
    {
        std::uint16_t     blocksize;
        std::uint32_t     sample_rate;
        std::uint8_t      channels;
        ChannelAssignment channel_assignment;
        std::uint8_t      bits_per_sample;
        NumberType        number_type;
        union
        {
            std::uint32_t frame_number;
            std::uint64_t sample_number;
        } number;
        std::uint8_t      crc;
    };
    struct Footer
    {
        std::uint16_t crc;
    };
    struct Frame
    {
        Header             header;
        Subframe::Subframe subframes[MAX_CHANNELS];
        Footer             footer;
    };
} // namespace Frame
namespace MetaData
{
    enum class Type : std::uint8_t
    {
        STREAMINFO     = 0,
        PADDING        = 1,
        APPLICATION    = 2,
        SEEKTABLE      = 3,
        VORBIS_COMMENT = 4,
        CUESHEET       = 5,
        PICTURE        = 6,
        UNDEFINED      = 7,
        MAX_METADATA   = MAX_METADATA_TYPE_CODE,
    };
    struct StreamInfo
    {
        std::uint16_t min_blocksize, max_blocksize;
        std::uint32_t min_framesize, max_framesize;
        std::uint32_t sample_rate;
        std::uint8_t  channels;
        std::uint8_t  bits_per_sample;
        std::uint64_t total_sample;
        std::uint8_t  md5sum[16];
    };
    struct Padding
    {
    };
    struct Application
    {
        std::uint8_t                id[4];
        std::vector< std::uint8_t > data;
    };
    struct SeekPoint
    {
        std::uint64_t sample_number;
        std::uint64_t stream_offset;
        std::uint16_t frame_samples;
    };
    struct SeekTable
    {
        std::vector< SeekPoint > points;
    };
    struct VorbisComment{
        // unknown
    };
    struct CueSheet_Index
    {
        std::uint64_t offset;
        std::uint8_t  number;
    };
    struct CueSheet_Track
    {
        std::uint64_t                 offset;
        std::uint8_t                  number;
        char                          isrc[13];
        bool                          type;
        bool                          pre_emphasis;
        std::vector< CueSheet_Index > indices;
    };
    struct CueSheet
    {
        char                          media_catalog_number[129];
        std::uint64_t                 lead_in;
        bool                          is_cd;
        std::vector< CueSheet_Track > tracks;
    };
    enum class PictureType
    {
        OTHER                =  0,
        FILE_ICON_STANDARD   =  1,
        FILE_ICON            =  2,
        FRONT_COVER          =  3,
        BACK_COVER           =  4,
        LEAFLET_PAGE         =  5,
        MEDIA                =  6,
        LEAD_ARTIST          =  7,
        ARTIST               =  8,
        CONDUCTOR            =  9,
        BAND                 = 10,
        COMPOSER             = 11,
        LYRICIST             = 12,
        RECORDING_LOCATION   = 13,
        DURING_RECORDING     = 14,
        DURING_PERFORMANCE   = 15,
        VIDEO_SCREEN_CAPTURE = 16,
        FISH                 = 17,
        ILLUSTRATION         = 18,
        BAND_LOGOTYPE        = 19,
        PUBLISHER_LOGOTYPE   = 20,
        UNDEFINED
    };
    struct Picture
    {
        PictureType                     type;
        std::string                     mine_type;
        std::string                     description;
        std::uint32_t                   width;
        std::uint32_t                   height;
        std::uint32_t                   depth;
        std::uint32_t                   colors;
        std::uint32_t                   data_length;
        std::unique_ptr< std::uint8_t > data;
    };
    struct Unknown
    {
        std::vector< std::uint8_t > data;
    };
    struct Metadata
    {
        Type          type;
        bool          is_last;
        std::uint32_t length;
        variant<
            StreamInfo,
            Padding,
            Application,
            SeekTable,
            VorbisComment,
            CueSheet,
            Picture,
            Unknown
        >             data;
    };
} // namespace MetaData

// functions
//// read.cpp
MetaData::Metadata ReadMetadata( buffer::bytestream<> &bs );
Frame::Frame       ReadFrame   ( buffer::bytestream<> &bs, MetaData::StreamInfo const &si );
//// write.cpp
void WriteFrame   ( buffer::bytestream<> &bs, Frame::Frame const &f );
void WriteMetadata( buffer::bytestream<> &bs, MetaData::Metadata const &md );
//// print.cpp
void PrintStreamInfo      ( FLAC::MetaData::StreamInfo const &si );
void PrintFrameHeader     ( FLAC::Frame::Header const &fh );
void PrintFrameFooter     ( FLAC::Frame::Footer const &ff );
void PrintSubframeHeader  ( FLAC::Subframe::Header const &sfh );
void PrintPartitionedRice ( FLAC::Subframe::PartitionedRice const &rice );
void PrintResidual        ( FLAC::Subframe::Residual const &res, std::uint16_t const residualsize );
void PrintSubframeConstant( FLAC::Subframe::Constant const &c, std::uint16_t const blocksize);
void PrintSubframeFixed   ( FLAC::Subframe::Fixed const &f, std::uint16_t const blocksize );
void PrintSubframeLPC     ( FLAC::Subframe::LPC const &lpc, std::uint16_t const blocksize );
void PrintSubframeVerbatim( FLAC::Subframe::Verbatim const &v, std::uint16_t const blocksize );
void PrintSubframe        ( FLAC::Subframe::Subframe const &sf, std::uint16_t const blocksize );
void PrintFrame           ( FLAC::Frame::Frame const &f );

} // namespace FLAC

#endif // FLACUTIL_FLAC_STRUCT_HPP
