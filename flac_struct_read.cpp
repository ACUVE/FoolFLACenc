#include <cstdint>
#include <iostream>
#include <tuple>
#include <utility>
#include "buffer.hpp"
#include "flac_struct.hpp"
#include "utility.hpp"

#define SHOW(op) do{ std::cout << __func__ << ":L." << __LINE__ << ": " << #op << " = " << static_cast< std::intmax_t >( op ) << std::endl; }while( false )

using namespace buffer;

namespace FLAC
{

template< std::uint8_t PARAMETER_LEN, typename BitStream >
static
std::tuple< std::unique_ptr< std::int64_t[] >, Subframe::PartitionedRice > ReadSubframe_Residual_PartitionedRice( BitStream &b, std::uint8_t const predictor_order, std::uint8_t const bps, std::uint16_t const blocksize )
{
    static_assert( PARAMETER_LEN <= 8, "PARAMETER_LEN must be under or equal to 8" );
    auto bs = make_useful_bitstream( b );
    std::unique_ptr< std::int64_t[] > residual;
    Subframe::PartitionedRice rice;
    std::uint8_t const partition_order = rice.order = bs.get( 4 );
    std::uint32_t const partitions = 1 << partition_order;
    constexpr std::uint8_t ESCAPE_PARAMETER = (1 << PARAMETER_LEN) - 1;
    if( (blocksize >> partition_order) < predictor_order )
        throw exception( "ReadSubframe_Residual_PartitionedRice: partition_order mismatch" );
    residual         = std::make_unique< std::int64_t[] >( blocksize - predictor_order );
    rice.parameters  = std::make_unique< std::uint8_t[] >( partitions );
    rice.is_raw_bits = std::make_unique< bool[] >        ( partitions );
    std::uint32_t sample = 0;
    for( std::uint32_t partition = 0; partition < partitions; ++partition )
    {
        std::uint8_t rice_parameter = rice.parameters[ partition ] = bs.get( PARAMETER_LEN );
        std::uint16_t this_part_sample_num = partition == 0 ? (blocksize >> partition_order) - predictor_order : blocksize >> partition_order;
        if( rice_parameter != ESCAPE_PARAMETER )
        {
            rice.is_raw_bits[ partition ] = false;
            for( std::uint16_t u = 0; u < this_part_sample_num; ++u, ++sample )
                residual[ sample ] = bs.get_rice_int( rice_parameter );
        }
        else
        {
            std::uint8_t bits_per_sample = bs.get( 5 );
            rice.is_raw_bits[ partition ] = true;
            for( std::uint16_t u = 0; u < this_part_sample_num; ++u, ++sample )
                residual[ sample ] = bs.get_int( bits_per_sample );
        }
    }
    return std::make_tuple( std::move( residual ), std::move( rice ) );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
Subframe::Residual ReadSubframe_Residual( BitStream &b, std::uint8_t predictor_order, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    Subframe::Residual res;
    res.type = static_cast< Subframe::EntropyCodingMethodType >( bs.get( 2 ) );
    switch( res.type )
    {
    case Subframe::EntropyCodingMethodType::PARTITIONED_RICE:
        std::tie(res.residual, res.data) = ReadSubframe_Residual_PartitionedRice< 4 >( bs, predictor_order, bps, blocksize);
        break;
    case Subframe::EntropyCodingMethodType::PARTITIONED_RICE2:
        std::tie(res.residual, res.data) = ReadSubframe_Residual_PartitionedRice< 5 >( bs, predictor_order, bps, blocksize);
        break;
    default:
        throw exception( "ReadSubframe_Residual: unknown type" );
    }
    return std::move( res );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
Subframe::LPC ReadSubframe_LPC( BitStream &b, std::uint8_t const order, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    Subframe::LPC lpc;
    lpc.order = order;
    for( std::uint8_t i = 0; i < order; ++i )
        lpc.warmup[ i ] = bs.get_int( bps );
    auto const qlp_coeff_precision_bit = bs.get( 4 );
    if( qlp_coeff_precision_bit == 0b1111 ) // invalid
        throw exception( "ReadSubframe_LPC: unknown qlp_coeff_precision_bit" );
    lpc.qlp_coeff_precision = qlp_coeff_precision_bit + 1;
    lpc.quantization_level = bs.get( 5 );
    for( std::uint8_t i = 0; i < order; ++i )
        lpc.qlp_coeff[ i ] = bs.get_int( lpc.qlp_coeff_precision );
    lpc.residual = ReadSubframe_Residual( bs, order, bps, blocksize );
    return std::move( lpc );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
Subframe::Fixed ReadSubframe_Fixed( BitStream &b, std::uint8_t const order, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    Subframe::Fixed fixed;
    if( order > 4 )
        throw exception( "ReadSubframe_Fixed: order is too big" );
    fixed.order = order;
    for( std::uint8_t i = 0; i < order; ++i )
        fixed.warmup[ i ] = bs.get_int( bps );
    fixed.residual = ReadSubframe_Residual( bs, order, bps, blocksize);
    return std::move( fixed );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
Subframe::Verbatim ReadSubframe_Verbatim( BitStream &b, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    Subframe::Verbatim v;
    v.data = std::make_unique< std::int32_t[] >( blocksize );
    for( std::uint16_t i = 0; i < blocksize; ++i )
        v.data[ i ] = bs.get_int( bps );
    return std::move( v );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
Subframe::Constant ReadSubframe_Constant( BitStream &b, std::uint8_t const bps, std::uint16_t const blocksize )
{
    auto bs = make_useful_bitstream( b );
    Subframe::Constant c;
    c.value = bs.get_int( bps );
    return std::move( c );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
Subframe::Header ReadSubframe_Header( BitStream &b )
{
    auto bs = make_useful_bitstream( b );
    Subframe::Header sfh;
    if( bs.get( 1 ) != 0b0 ) // must be zero
        return {};
    sfh.type_bits = bs.get( 6 );
    if( sfh.type_bits == 0b000000 )
        sfh.type = Subframe::Type::CONSTANT;
    else if( sfh.type_bits == 0b000001 )
        sfh.type = Subframe::Type::VERBATIM;
    else if( (sfh.type_bits & 0b111110) == 0b000010 ) // reserved
        throw exception( "ReadFrame_Header: unknown type_bits" );
    else if( (sfh.type_bits & 0b111100) == 0b000100 ) // reserved
        throw exception( "ReadFrame_Header: unknown type_bits" );
    else if( (sfh.type_bits & 0b111000) == 0b001000 )
    {
        std::uint8_t order = sfh.type_bits & 0b000111;
        if( order <= 4 )
            sfh.type = Subframe::Type::FIXED;
        else // reserved
            throw exception( "ReadFrame_Header: unknown type_bits" );
    }
    else if( (sfh.type_bits & 0b110000) == 0b010000 ) // reserved
        throw exception( "ReadFrame_Header: unknown type_bits" );
    else
        sfh.type = Subframe::Type::LPC;
    if( bs.get( 1 ) )
        sfh.wasted_bits = bs.get_unary() + 1;
    else
        sfh.wasted_bits = 0;
    return std::move( sfh );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
Subframe::Subframe ReadSubframe( BitStream &b, std::uint8_t bps, std::uint16_t blocksize )
{
    auto bs = make_useful_bitstream( b );
    Subframe::Subframe sf;
    sf.header = ReadSubframe_Header( bs );
    switch( sf.header.type )
    {
    case Subframe::Type::CONSTANT:
        sf.data = ReadSubframe_Constant( bs, bps, blocksize );
        break;
    case Subframe::Type::VERBATIM:
            sf.data = ReadSubframe_Verbatim( bs, bps, blocksize );
        break;
    case Subframe::Type::FIXED:
        sf.data = ReadSubframe_Fixed( bs, sf.header.type_bits & 0b000111, bps, blocksize );
        break;
    case Subframe::Type::LPC:
        sf.data = ReadSubframe_LPC( bs, (sf.header.type_bits & 0b011111) + 1, bps, blocksize );
        break;
    }
    return std::move( sf );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
Frame::Footer ReadFrame_Footer( BitStream &b )
{
    auto bs = make_useful_bitstream( b );
    Frame::Footer f;
    f.crc = bs.get( 16 );
    return std::move( f );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
Frame::Header ReadFrame_Header( BitStream &b, MetaData::StreamInfo const &si )
{
    auto bs = make_useful_bitstream( b );
    assert( bs.is_byte_aligned() );
    Frame::Header h;
    if( bs.get( 14 ) != FRAME_HEADER_SYNC )
        throw exception( "ReadFrame_Header: sync error" );
    if( bs.get( 1 ) == 0b1 ) // reserved
        throw exception( "ReadFrame_Header: unknown bit" );
    h.number_type = static_cast< Frame::NumberType >( bs.get( 1 ) );
    auto blocksizebit = bs.get( 4 );
    if( blocksizebit == 0b0000 ) // reserved
        throw exception( "ReadFrame_Header: unknown blocksizebit" );
    auto sampleratebit = bs.get( 4 );
    if( sampleratebit == 0b1111 ) // invalid
        throw exception( "ReadFrame_Header: invalid sampleratebit" );
    auto channelbit = bs.get( 4 );
    if( channelbit == 0b1011 || channelbit == 0b1111 ) // reserved
        throw exception( "ReadFrame_Header: unknown channelbit" );
    switch( channelbit )
    {
    case 0b0000: case 0b0001: case 0b0010: case 0b0011: case 0b0100: case 0b0101: case 0b0110: case 0b0111:
        h.channels           = channelbit + 1;
        h.channel_assignment = Frame::ChannelAssignment::INDEPENDENT;
        break;
    case 0b1000:
        h.channels           = 2;
        h.channel_assignment = Frame::ChannelAssignment::LEFT_SIDE;
        break;
    case 0b1001:
        h.channels           = 2;
        h.channel_assignment = Frame::ChannelAssignment::RIGHT_SIDE;
        break;
    case 0b1010:
        h.channels           = 2;
        h.channel_assignment = Frame::ChannelAssignment::MID_SIDE;
        break;
    }
    auto samplesizebit = bs.get( 3 );
    if( samplesizebit == 0b011 || samplesizebit == 0b111 ) // reserved
        throw exception( "ReadFrame_Header: unknown sampleratebit" );
    if( bs.get( 1 ) == 0b1 ) // reserved
        throw exception( "ReadFrame_Header: unknown bit" );
    std::uint64_t num = bs.get_utf8();
    std::uint8_t  clz = utility::clz( num );
    switch( h.number_type )
    {
    case Frame::NumberType::FRAME_NUMBER:
        h.number.frame_number = num;
        if( 64 - clz > 31 ) // frame_number is 31 bits
            throw exception( "ReadFrame_Header: frame_number is too big" );
        break;
    case Frame::NumberType::SAMPLE_NUMBER:
        h.number.sample_number = num; // get_utf8() return 35 bits integer
        break;
    }
    switch( blocksizebit )
    {
    case 0b0001: h.blocksize =   192; break;
    case 0b0010: h.blocksize =   576; break;
    case 0b0011: h.blocksize =  1152; break;
    case 0b0100: h.blocksize =  2304; break;
    case 0b0101: h.blocksize =  4608; break;
    case 0b0110: h.blocksize =  bs.get( 8 ) + 1; break;
    case 0b0111: h.blocksize =  bs.get( 16 ) + 1; break;
    case 0b1000: h.blocksize =   256; break;
    case 0b1001: h.blocksize =   512; break;
    case 0b1010: h.blocksize =  1024; break;
    case 0b1011: h.blocksize =  2048; break;
    case 0b1100: h.blocksize =  4096; break;
    case 0b1101: h.blocksize =  8192; break;
    case 0b1110: h.blocksize = 16384; break;
    case 0b1111: h.blocksize = 32768; break;
    }
    switch( sampleratebit )
    {
    case 0b0000: h.sample_rate = si.sample_rate; break;
    case 0b0001: h.sample_rate =  88200; break;
    case 0b0010: h.sample_rate = 176400; break;
    case 0b0011: h.sample_rate = 192000; break;
    case 0b0100: h.sample_rate =   8000; break;
    case 0b0101: h.sample_rate =  16000; break;
    case 0b0110: h.sample_rate =  22050; break;
    case 0b0111: h.sample_rate =  24000; break;
    case 0b1000: h.sample_rate =  32000; break;
    case 0b1001: h.sample_rate =  44100; break;
    case 0b1010: h.sample_rate =  48000; break;
    case 0b1011: h.sample_rate =  96000; break;
    case 0b1100: h.sample_rate = bs.get( 8 ) * 1000; break;
    case 0b1101: h.sample_rate = bs.get( 16 ); break;
    case 0b1110: h.sample_rate = bs.get( 16 ) * 10; break;
    }
    switch( samplesizebit )
    {
    case 0b000: h.bits_per_sample = si.bits_per_sample; break;
    case 0b001: h.bits_per_sample =  8; break;
    case 0b010: h.bits_per_sample = 12; break;
    case 0b100: h.bits_per_sample = 16; break;
    case 0b101: h.bits_per_sample = 20; break;
    case 0b110: h.bits_per_sample = 24; break;
    }
    h.crc = bs.get( 8 );
    return std::move( h );
}

/***********************************************************************************************************************/

Frame::Frame ReadFrame( bytestream<> &b, MetaData::StreamInfo const &si )
{
    bitstream< bytestream<> > bits( b );
    auto bs = make_useful_bitstream( bits );
    Frame::Frame f;
    f.header = ReadFrame_Header( bs, si );
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
            throw exception( "ReadFrame: unknown channel_assignment" );
        }
        f.subframes[ i ] = ReadSubframe( bs, bps, f.header.blocksize );
    }
    while( std::get< 1 >( bs.get_position() ) )
        bs.get( 1 );
    f.footer = ReadFrame_Footer( bs );
    return std::move( f );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
MetaData::Padding ReadMetadata_Padding( BitStream &b, std::uint32_t length )
{
    auto bs = make_useful_bitstream( b );
    assert( bs.is_byte_aligned() );
    MetaData::Padding p;
    return std::move( p );
}

/***********************************************************************************************************************/

template< typename BitStream >
static
MetaData::StreamInfo ReadMetadata_StreamInfo( BitStream &b, std::uint32_t length )
{
    auto bs = make_useful_bitstream( b );
    assert( bs.is_byte_aligned() );
    MetaData::StreamInfo si;
    si.min_blocksize   = bs.get( 16 );
    si.max_blocksize   = bs.get( 16 );
    si.min_framesize   = bs.get( 24 );
    si.max_framesize   = bs.get( 24 );
    si.sample_rate     = bs.get( 20 );
    si.channels        = bs.get(  3 ) + 1;
    si.bits_per_sample = bs.get(  5 ) + 1;
    si.total_sample    = bs.get( 36 );
    std::memcpy(si.md5sum, bs.get_bytes( 16 ).get(), 16);
    return std::move( si );
}

/***********************************************************************************************************************/

MetaData::Metadata ReadMetadata( bytestream<> &b )
{
    bitstream< bytestream<> > bits( b );
    auto bs = make_useful_bitstream( bits );
    MetaData::Metadata md;
    md.is_last = bs.get( 1 );
    md.type    = static_cast< MetaData::Type >( bs.get( 7 ) );
    md.length  = bs.get( 24 );
    auto position = bs.get_position();
    switch( md.type )
    {
    case MetaData::Type::STREAMINFO:
        md.data = ReadMetadata_StreamInfo( bs, md.length );
        break;
    case MetaData::Type::PADDING:
        md.data = ReadMetadata_Padding( bs, md.length );
        break;
    }
    bs.set_position( position );
    bs.skip_byte( md.length );
    return std::move( md );
}

} // namespace FLAC
