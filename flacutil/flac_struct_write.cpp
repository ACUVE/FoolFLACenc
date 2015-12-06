#include <cstdint>
#include <iostream>
#include <tuple>
#include <utility>
#include "buffer.hpp"
#include "flac_struct.hpp"
#include "hash.hpp"
#include "utility.hpp"

#define SHOW(op) do{ std::cout << __func__ << ":L." << __LINE__ << ": " << #op << " = " << static_cast< std::intmax_t >( op ) << std::endl; }while( false )

using namespace buffer;

namespace FLAC
{

template< std::uint8_t PARAMETER_LEN, typename BitStream >
static
void WriteSubframe_Residual_PartitionedRice( BitStream &b, std::int64_t const *residual, Subframe::PartitionedRice const &rice, std::uint8_t const predictor_order, std::uint8_t const bps, std::uint16_t const blocksize )
{
    static_assert( PARAMETER_LEN <= 8, "PARAMETER_LEN must be under or equal to 8" );
    auto bs = make_useful_bitstream( b );
    std::uint8_t const partition_order = rice.order;
    bs.put( partition_order, 4 );
    std::uint32_t const partitions = 1 << partition_order;
    constexpr std::uint8_t ESCAPE_PARAMETER = (1 << PARAMETER_LEN) - 1;
    if( (blocksize >> partition_order) < predictor_order )
        throw exception( "WriteSubframe_Residual_PartitionedRice: partition_order mismatch" );
    std::uint32_t sample = 0;
    for( std::uint32_t partition = 0; partition < partitions; ++partition )
    {
        std::uint16_t const this_part_sample_num = partition == 0 ? (blocksize >> partition_order) - predictor_order : blocksize >> partition_order;
        if( !rice.is_raw_bits[ partition ] )
        {
            bs.put( rice.parameters[ partition ], PARAMETER_LEN );
            std::uint8_t const rice_parameter = rice.parameters[ partition ];
            for( std::uint16_t u = 0; u < this_part_sample_num; ++u, ++sample )
                bs.put_rice_int( residual[ sample ], rice_parameter );
        }
        else
        {
            bs.put( ESCAPE_PARAMETER, PARAMETER_LEN );
            std::uint8_t const bits_per_sample = rice.parameters[ partition ];
            bs.put( bits_per_sample, 5 );
            for( std::uint16_t u = 0; u < this_part_sample_num; ++u, ++sample )
                bs.put_int( residual[ sample ], bits_per_sample );
        }
    }
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteSubframe_Residual( BitStream &b, Subframe::Residual const &res, std::uint8_t predictor_order, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    bs.put( static_cast< std::uint8_t >( res.type ), 2 );
    switch( res.type )
    {
    case Subframe::EntropyCodingMethodType::PARTITIONED_RICE:
        WriteSubframe_Residual_PartitionedRice< 4 >( bs, res.residual.get(), res.data.data< Subframe::PartitionedRice >(), predictor_order, bps, blocksize);
        break;
    case Subframe::EntropyCodingMethodType::PARTITIONED_RICE2:
        WriteSubframe_Residual_PartitionedRice< 5 >( bs, res.residual.get(), res.data.data< Subframe::PartitionedRice >(), predictor_order, bps, blocksize);
        break;
    default:
        throw exception( "WriteSubframe_Residual: unknown type" );
    }
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteSubframe_LPC( BitStream &b, Subframe::LPC const &lpc, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    for( std::uint8_t i = 0; i < lpc.order; ++i )
        bs.put_int( lpc.warmup[ i ], bps );
    bs.put( lpc.qlp_coeff_precision - 1, 4 );
    bs.put( lpc.quantization_level, 5 );
    for( std::uint8_t i = 0; i < lpc.order; ++i )
        bs.put_int( lpc.qlp_coeff[ i ], lpc.qlp_coeff_precision );
    WriteSubframe_Residual( bs, lpc.residual, lpc.order, bps, blocksize );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteSubframe_Fixed( BitStream &b, Subframe::Fixed const &fixed, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    if( fixed.order > 4 )
        throw exception( "WriteSubframe_Fixed: order is too big" );
    for( std::uint8_t i = 0; i < fixed.order; ++i )
        bs.put_int( fixed.warmup[ i ], bps );
    WriteSubframe_Residual( bs, fixed.residual, fixed.order, bps, blocksize);
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteSubframe_Verbatim( BitStream &b, Subframe::Verbatim const &v, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    for( std::uint16_t i = 0; i < blocksize; ++i )
        bs.put_int( v.data[ i ], bps );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteSubframe_Constant( BitStream &b, Subframe::Constant const &c ,std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    bs.put_int( c.value, bps );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteSubframe_Header( BitStream &b, Subframe::Header const &sfh, std::uint8_t const type_bits )
{
    auto bs = make_useful_bitstream( b );
    bs.put( 0, 1 ); // must be zero
    bs.put( type_bits, 6 ); // ignore sfh.type_bits
    if( sfh.wasted_bits )
    {
        bs.put( 1, 1 );
        bs.put_unary( sfh.wasted_bits - 1 );
    }
    else
        bs.put( 0, 1 );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteSubframe( BitStream &b, Subframe::Subframe const &sf, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    switch( sf.header.type )
    {
    case Subframe::Type::CONSTANT:
        WriteSubframe_Header( b, sf.header, 0b000000 );
        WriteSubframe_Constant( b, sf.data.data< Subframe::Constant >(), bps, blocksize );
        break;
    case Subframe::Type::VERBATIM:
        WriteSubframe_Header( b, sf.header, 0b000001 );
        WriteSubframe_Verbatim( b, sf.data.data< Subframe::Verbatim >(), bps, blocksize );
        break;
    case Subframe::Type::FIXED:{
        Subframe::Fixed const &fixed = sf.data.data< Subframe::Fixed >();
        if( fixed.order <= 4 )
        {
            WriteSubframe_Header( b, sf.header, 0b001000 | fixed.order );
            WriteSubframe_Fixed( b, fixed, bps, blocksize );
        }
        else
            throw exception( "WriteSubframe: fixed.order is too big" );
        break;
    }
    case Subframe::Type::LPC:{
        Subframe::LPC const &lpc = sf.data.data< Subframe::LPC >();
        if( 1 <= lpc.order && lpc.order <= 32 )
        {
            WriteSubframe_Header( b, sf.header, 0b100000 | (lpc.order - 1) );
            WriteSubframe_LPC( b, lpc, bps, blocksize );
        }
        else
            throw exception( "WriteSubframe: lpc.order is out of range" );
        break;
    }
    default:
        throw exception( "WriteSubframe: unknown Subframe Type" );
    }
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteFrame_Footer( BitStream &b, Frame::Footer const &f )
{
    assert( b.is_byte_aligned() );
    auto bs = make_useful_bitstream( b );
    hash::crc16_hash const &crc16_hash = bs.get_bytestream().get_hash();
    std::uint16_t const calculated_crc16 = crc16_hash.get();
    bs.put( calculated_crc16, 16 );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteFrame_Header( BitStream &b, Frame::Header const &h )
{
    assert( b.is_byte_aligned() );
    auto crc8bs = make_hash_bytestream< hash::crc8_hash >( b.get_bytestream() );
    auto crc8bits = make_bitstream( crc8bs );
    auto bs = make_useful_bitstream( crc8bits );
    bs.put( FRAME_HEADER_SYNC, 14 );
    bs.put( 0, 1 ); // reserved
    bs.put( static_cast< std::uint8_t >( h.number_type ), 1 );
    int blocksize_last = 0;
    switch( h.blocksize )
    {
    case   192: bs.put( 0b0001, 4 ); break;
    case   576: bs.put( 0b0010, 4 ); break;
    case  1152: bs.put( 0b0011, 4 ); break;
    case  2304: bs.put( 0b0100, 4 ); break;
    case  4608: bs.put( 0b0101, 4 ); break;
    case   256: bs.put( 0b1000, 4 ); break;
    case   512: bs.put( 0b1001, 4 ); break;
    case  1024: bs.put( 0b1010, 4 ); break;
    case  2048: bs.put( 0b1011, 4 ); break;
    case  4096: bs.put( 0b1100, 4 ); break;
    case  8192: bs.put( 0b1101, 4 ); break;
    case 16384: bs.put( 0b1110, 4 ); break;
    case 32768: bs.put( 0b1111, 4 ); break;
    default:
        if( h.sample_rate - 1 <= 0xFF )
        {
            bs.put( 0b0110, 4 );
            blocksize_last = 1;
        }
        else
        {
            bs.put( 0b0111, 4 );
            blocksize_last = 2;
        }
        break;
    }
    int samplerate_last = 0;
    switch( h.sample_rate )
    {
    case  88200: bs.put( 0b0001, 4 ); break;
    case 176400: bs.put( 0b0010, 4 ); break;
    case 192000: bs.put( 0b0011, 4 ); break;
    case   8000: bs.put( 0b0100, 4 ); break;
    case  16000: bs.put( 0b0101, 4 ); break;
    case  22050: bs.put( 0b0110, 4 ); break;
    case  24000: bs.put( 0b0111, 4 ); break;
    case  32000: bs.put( 0b1000, 4 ); break;
    case  44100: bs.put( 0b1001, 4 ); break;
    case  48000: bs.put( 0b1010, 4 ); break;
    case  96000: bs.put( 0b1011, 4 ); break;
    default:
        if( h.sample_rate > MAX_SAMPLE_RATE )
            throw exception( "WriteFrame_Header: sample_rate is too big" );
        if( h.sample_rate % 1000 == 0 && h.sample_rate / 1000 <= 0xFF )
        {
            bs.put( 0b1100, 4 );
            samplerate_last = 1;
        }
        else if( h.sample_rate <= 0xFFFF )
        {
            bs.put( 0b1101, 4 );
            samplerate_last = 2;
        }
        else if( h.sample_rate % 10 == 0 && h.sample_rate / 10 <= 0xFFFF )
        {
            bs.put( 0x1110, 4 );
            samplerate_last = 3;
        }
        else
            bs.put( 0x0000, 4 );
        break;
    }
    switch( h.channel_assignment )
    {
    case Frame::ChannelAssignment::INDEPENDENT:
        if( 1 <= h.channels && h.channels <= 8 )
            bs.put( h.channels - 1, 4 );
        else
            throw exception( "WriteSubframe_Header: channels is too big" );
        break;
    case Frame::ChannelAssignment::LEFT_SIDE:
        if( h.channels == 2 )
            bs.put( 0b1000, 4 );
        else
            throw exception( "WriteSubframe_Header: invaild channels" );
        break;
    case Frame::ChannelAssignment::RIGHT_SIDE:
        if( h.channels == 2 )
            bs.put( 0b1001, 4 );
        else
            throw exception( "WriteSubframe_Header: invalid channels" );
        break;
    case Frame::ChannelAssignment::MID_SIDE:
        if( h.channels == 2 )
            bs.put( 0b1010, 4 );
        else
            throw exception( "WriteSubframe_Header: invalid channels" );
        break;
    default:
        throw exception( "WriteSubframe_Header: unknown number_type" );
    }
    switch( h.bits_per_sample )
    {
    case  8: bs.put( 0b001, 3 ); break;
    case 12: bs.put( 0b010, 3 ); break;
    case 16: bs.put( 0b100, 3 ); break;
    case 20: bs.put( 0b101, 3 ); break;
    case 24: bs.put( 0b110, 3 ); break;
    default: bs.put( 0b000, 3 ); break;
    }
    bs.put( 0, 1 ); // reserved
    switch( h.number_type )
    {
    case Frame::NumberType::FRAME_NUMBER:
        if( h.number.frame_number <= 0x7FFFFFFF )
            bs.put_utf8( h.number.frame_number );
        else
            throw exception( "WriteSubframe_Header: frame_number is too big" );
        break;
    case Frame::NumberType::SAMPLE_NUMBER:
        if( h.number.sample_number <= 0xFFFFFFFFF )
            bs.put_utf8( h.number.sample_number );
        else
            throw exception( "WriteSubframe_Header: sample_number is too big" );
        break;
    }
    switch( blocksize_last )
    {
    case 1: bs.put( h.blocksize,  8 ); break;
    case 2: bs.put( h.blocksize, 16 ); break;
    }
    switch( samplerate_last )
    {
    case 1: bs.put( h.sample_rate / 1000,  8 ); break;
    case 2: bs.put( h.sample_rate       , 16 ); break;
    case 3: bs.put( h.sample_rate /   10, 16 ); break;
    }
    assert( bs.is_byte_aligned() );
    std::uint8_t const calculated_crc8 = crc8bs.get_hash().get();
    auto noncrc8bs = make_useful_bitstream( b );
    noncrc8bs.put( calculated_crc8, 8 );
}

/***********************************************************************************************************************/

void WriteFrame( bytestream<> &b, Frame::Frame const &f )
{
    auto crc16bs = make_hash_bytestream< hash::crc16_hash >( b );
    auto crc16bits = make_bitstream( crc16bs );
    auto bs = make_useful_bitstream( crc16bits );
    WriteFrame_Header( bs, f.header );
    for( std::uint8_t i = 0; i < f.header.channels; ++i )
    {
        std::uint8_t bps = f.header.bits_per_sample;
        switch( f.header.channel_assignment )
        {
        case Frame::ChannelAssignment::INDEPENDENT:
            break;
        case Frame::ChannelAssignment::LEFT_SIDE:
            if( i == 1 )
                ++bps;
            break;
        case Frame::ChannelAssignment::RIGHT_SIDE:
            if( i == 0 )
                ++bps;
            break;
        case Frame::ChannelAssignment::MID_SIDE:
            if( i == 1 )
                ++bps;
            break;
        default:
            throw exception( "WriteFrame: unknown channel_assignment" );
        }
        WriteSubframe( bs, f.subframes[ i ], bps, f.header.blocksize );
    }
    while( std::get< 1 >( bs.get_position() ) )
        bs.put( 0, 1 );
    assert( bs.is_byte_aligned() );
    WriteFrame_Footer( bs, f.footer );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteMetadata_Padding( BitStream &b, MetaData::Padding const &p, std::uint32_t const length )
{
    auto bs = make_useful_bitstream( b );
    assert( bs.is_byte_aligned() );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
void WriteMetadata_StreamInfo( BitStream &b, MetaData::StreamInfo const &si, std::uint32_t const length )
{
    auto bs = make_useful_bitstream( b );
    assert( bs.is_byte_aligned() );
    bs.put( si.min_blocksize      , 16 );
    bs.put( si.max_blocksize      , 16 );
    bs.put( si.min_framesize      , 24 );
    bs.put( si.max_framesize      , 24 );
    bs.put( si.sample_rate        , 20 );
    bs.put( si.channels - 1       ,  3 );
    bs.put( si.bits_per_sample - 1,  5 );
    bs.put( si.total_sample       , 36 );
    bs.put_bytes( si.md5sum       , 16 );
}

void WriteMetadata( bytestream<> &b, MetaData::Metadata const &md )
{
    bitstream< bytestream<> > bits( b );
    auto bs = make_useful_bitstream( bits );
    bs.put( md.is_last, 1 );
    bs.put( static_cast< std::uint8_t >( md.type ), 7 );
    bs.put( md.length, 24 );
    auto position = bs.get_position();
    switch( md.type )
    {
    case MetaData::Type::STREAMINFO:
        WriteMetadata_StreamInfo( bs, md.data.data< MetaData::StreamInfo >(), md.length );
        break;
    case MetaData::Type::PADDING:
        WriteMetadata_Padding( bs, md.data.data< MetaData::Padding >(), md.length );
    }
    bs.set_position( position );
    bs.skip_byte( md.length );
}

}
