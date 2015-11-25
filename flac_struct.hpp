// License: GPLv3 or Later

#ifndef FLAC_STRUCT_HPP
#define FLAC_STRUCT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <experimental/optional>

#include "variant.hpp"

class bitstream;

namespace FLAC
{

template< typename T>
using optional = std::experimental::optional< T >;

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
        std::unique_ptr< std::int32_t[] > data;
    };
    struct Fixed
    {
        std::uint8_t order;
        std::int32_t warmup[MAX_FIXED_ORDER];
        Residual     residual;
    };
    struct LPC
    {
        std::uint8_t order;
        std::uint8_t qlp_coeff_precision;
        std::uint8_t quantization_level;
        std::int32_t qlp_coeff[MAX_LPC_ORDER];
        std::int32_t warmup[MAX_LPC_ORDER];
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
optional< MetaData::Metadata > ReadMetadata( bitstream &bs );
optional< Frame::Frame >       ReadFrame( bitstream &bs, MetaData::StreamInfo const &si );

} // namespace FLAC

#endif // FLAC_STRUCT_HPP
