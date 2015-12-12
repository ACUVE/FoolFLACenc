#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <tuple>

#include <iostream>
#include <mutex>

#include "flac_encode.hpp"

namespace FLAC
{

constexpr std::size_t RICE_LEN = 31;
struct rice_search_data
{
    std::uint64_t num[ RICE_LEN ];
};

static
std::uint8_t MaxRicePartitionOrder( std::uint8_t const predict_order, std::uint16_t const blocksize )
{
    for( std::uint8_t order = 0; order < (1u << 4) - 1; ++order )
    {
        if( (blocksize >> order) & 1 )
            return order;
        if( blocksize >> (order + 1) < predict_order )
            return order;
    }
    return (1u << 4) - 1;
}
static
std::unique_ptr< rice_search_data[] > MakeRiceSearchData( std::int64_t const *residual, std::uint8_t const part_order, std::uint8_t const predict_order, std::uint16_t const blocksize )
{
    std::uint16_t const max_partitions = 1u << part_order;
    auto data = std::make_unique< rice_search_data[] >( max_partitions );
    std::uint16_t const default_sample_num = blocksize >> part_order;
    std::uint16_t sample = 0;
    for( std::uint16_t part = 0; part < max_partitions; ++part )
    {
        std::uint16_t const this_sample_num = part == 0 ? default_sample_num - predict_order : default_sample_num;
        for( std::uint16_t j = 0; j < this_sample_num; ++j, ++sample )
        {
            std::int64_t const s = residual[ sample ];
            std::uint64_t us = s >= 0 ? static_cast< std::uint64_t >( s ) << 1 : (static_cast< std::uint64_t >( -s ) << 1) - 1;
            for( unsigned int k = 0; us && k < RICE_LEN; ++k, us >>= 1 )
                data[ part ].num[ k ] += us;
        }
    }
    return std::move( data );
}
static
std::tuple< std::uint64_t, bool > FindBestRiceParameterFixedPartitions( std::uint8_t const order, std::uint8_t *buff, rice_search_data const *data, std::uint8_t const max_order, std::uint8_t const predict_order, std::uint16_t const blocksize)
{
    if( order > max_order )
        throw exception( "FindBestRiceParameterFixedPartitions: order is too big" );
    std::uint16_t const default_sample = blocksize >> order;
    std::uint16_t const partitions = 1u << order;
    std::uint16_t const same_part_num = 1u << (max_order - order);
    std::uint16_t part_index = 0;
    std::uint64_t bits = 0;
    bool is_rice2 = false;
    for( std::uint16_t part = 0; part < partitions; ++part )
    {
        std::uint64_t sum[ RICE_LEN ] = {};
        for( std::uint16_t i = 0; i < same_part_num; ++i, ++part_index )
            for( unsigned int m = 0; m < RICE_LEN; ++m )
                sum[ m ] += data[ part_index ].num[ m ];
        std::uint64_t min_part_bits = std::numeric_limits< decltype( min_part_bits ) >::max();
        std::uint8_t min_part_param = 0;
        std::uint16_t this_sample_num = part == 0 ? default_sample - predict_order : default_sample;
        for( std::uint8_t l = 0; l < RICE_LEN; ++l )
        {
            std::uint64_t const c_bits = static_cast< std::uint64_t >( l + 1 ) * this_sample_num + sum[ l ];
            if( c_bits < min_part_bits )
            {
                min_part_bits = c_bits;
                min_part_param = l;
            }
            if( sum[ l ] == 0 )
                break;
        }
        bits += min_part_bits;
        buff[ part ] = min_part_param;
        if( min_part_param > 14 )
            is_rice2 = true;
    }
    bits += (is_rice2 ? 5 : 4 ) * partitions;
    return std::make_tuple( bits, is_rice2 );
}
static
std::tuple< Subframe::PartitionedRice, Subframe::EntropyCodingMethodType, std::uint64_t > FindBestRiceParameter( std::int64_t const *residual, std::uint8_t const predict_order, std::uint16_t const blocksize )
{
    Subframe::PartitionedRice rice;
    std::uint8_t const max_order = MaxRicePartitionOrder( predict_order, blocksize );
    std::uint16_t const max_partitions = 1u << max_order;
    auto data = MakeRiceSearchData( residual, max_order, predict_order, blocksize );
    
    std::uint64_t min_bits = std::numeric_limits< decltype( min_bits ) >::max();
    std::uint8_t min_bits_order = std::numeric_limits< decltype( min_bits_order ) >::max();
    bool min_bits_is_rice2 = false;
    auto min_bits_parameters = std::make_unique< std::uint8_t[] >( max_partitions ), buff = std::make_unique< std::uint8_t[] >( max_partitions );
    for( std::uint8_t order = 0; order <= max_order; ++order )
    {
        std::uint64_t bits;
        bool is_rice2;
        std::tie( bits, is_rice2 ) = FindBestRiceParameterFixedPartitions( order, buff.get(), data.get(), max_order, predict_order, blocksize );
        if( bits < min_bits )
        {
            min_bits = bits;
            min_bits_order = order;
            min_bits_is_rice2 = is_rice2;
            std::swap( min_bits_parameters, buff );
        }
    }
    rice.order = min_bits_order;
    rice.parameters = std::move( min_bits_parameters );
    rice.is_raw_bits = std::make_unique< bool[] >( 1u << rice.order );
    for( std::uint16_t i = 0; i < (1u << rice.order); ++i )
        rice.is_raw_bits[ i ] = false;
    return std::make_tuple( std::move( rice ), !min_bits_is_rice2 ? Subframe::EntropyCodingMethodType::PARTITIONED_RICE : Subframe::EntropyCodingMethodType::PARTITIONED_RICE2, min_bits );
}
// set res.residual before call
static
std::uint64_t FindBestResidualParameter( Subframe::Residual &res, std::uint8_t const predict_order, std::uint16_t const blocksize )
{
    auto t = FindBestRiceParameter( res.residual.get(), predict_order, blocksize );
    res.type = std::get< 1 >( t );
    res.data = std::move( std::get< 0 >( t ) );
    return std::get< 2 >( t ) + 2;
}
static
std::unique_ptr< double[] > CalcAutocorrelation( std::uint8_t const max_gap, std::int64_t const *buff, std::uint16_t const length )
{
    auto autoc = std::make_unique< double[] >( max_gap + 1 );
    for( std::uint8_t i = 0; i <= max_gap; ++i )
    {
        std::uint16_t const loop_num = length - max_gap;
        double d = 0;
        for( std::uint16_t j = 0; j < loop_num; ++j )
            d += static_cast< double >( buff[ j ] ) * static_cast< double >( buff[ j + i ] );
        autoc[ i ] = d;
    }
    return std::move( autoc );
}
static
std::unique_ptr< double[][ MAX_LPC_ORDER ] > CalcLPCCoefficient( std::int64_t const *buff, double const *autoc, std::uint8_t &max_order, std::uint16_t const blocksize )
{
    if( max_order == 0 )
        return std::make_unique< double[][ MAX_LPC_ORDER ] >( 0 );
    auto lpc = std::make_unique< double[][ MAX_LPC_ORDER ] >( max_order );
    lpc[ 0 ][ 0 ] = autoc[ 1 ] / autoc[ 0 ];
    double err = autoc[ 0 ] - lpc[ 0 ][ 0 ] * autoc[ 1 ];
    for( std::uint8_t i = 1; i < max_order; ++i )
    {
        double const lambda = err == 0.0 ? 0.0 : [ & ]
        {
            double l = -autoc[ i + 1 ];
            for( std::uint8_t j = 1; j <= i; ++j )
                l += lpc[ i - 1 ][ j - 1 ] * autoc[ i + 1 - j ];
            return l / err;
        }();
        for( std::uint8_t j = 0; j < i; ++j )
            lpc[ i ][ j ] = lpc[ i - 1 ][ j ] + lambda * lpc[ i - 1 ][ i - j - 1 ];
        lpc[ i ][ i ] = -lambda;
        err *= 1 - lambda * lambda;
        if( err == 0.0 )
        {
            max_order = i;
            break;
        }
    }
    return std::move( lpc );
}
static
std::tuple< Subframe::LPC, std::uint64_t > EncodeLPCFixedOrderAndPrecision( std::uint8_t const order, std::uint8_t const precision, double const (*coef)[ MAX_LPC_ORDER ], std::int64_t const *src, std::uint8_t const bps, std::uint16_t const blocksize )
{
    Subframe::LPC lpc;
    lpc.order = order;
    for( std::uint8_t i = 0; i < order; ++i )
        lpc.warmup[ i ] = src[ i ];
    lpc.qlp_coeff_precision = precision;
    double max_coef = 0;
    for( std::uint8_t i = 0; i < order; ++i )
        max_coef = std::max( max_coef, std::abs( coef[ order - 1 ][ i ] ) );
    double const bits = std::log2( max_coef ) + 1;
    // if( order <= 4 )
    // {
        // static std::mutex mutex;
        // mutex.lock();
        // std::cout << (int)order << "," << max_coef << ","  << bits << std::endl;
        // mutex.unlock();
    // }
    std::int8_t const bits_int = static_cast< std::int8_t >( std::ceil( bits ) );
    double const sumbits = std::log2( order ) + bps;
    std::int8_t const max_bits_int = static_cast< std::int8_t >( std::ceil( bits + sumbits ) );
    lpc.quantization_level = precision - bits_int;
    for( std::int8_t i = 0; i < order; ++i )
        lpc.qlp_coeff[ i ] = coef[ order - 1 ][ i ] * (1u << lpc.quantization_level);
    lpc.residual.residual = std::make_unique< std::int64_t[] >( blocksize - order );
    for( std::uint16_t i = order; i < blocksize; ++i )
    {
        std::int64_t sum = 0;
        for( std::uint8_t j = 0; j < order; ++j )
            sum += lpc.qlp_coeff[ j ] * src[ i - j - 1 ];
        lpc.residual.residual[ i - order ] = src[ i ] - (sum >> lpc.quantization_level);
    }
    std::uint64_t const lenbits = FindBestResidualParameter( lpc.residual, order, blocksize );
    // TODO:
    return std::make_tuple( std::move( lpc ), lenbits + bps * order + lpc.qlp_coeff_precision * order + 9 );
}

std::tuple< Subframe::Constant, std::uint64_t > EncodeConstant( std::int64_t const *src, std::uint8_t const bps, std::uint16_t const blocksize )
{
    Subframe::Constant co;
    co.value = src[ 0 ];
    return std::make_tuple( std::move( co ), bps );
}
std::tuple< Subframe::Fixed, std::uint64_t > EncodeFixed( std::int64_t const *src, std::uint8_t const bps, std::uint8_t const order, std::uint16_t const blocksize )
{
    if( order >= blocksize )
        throw exception( "EncodeFixed: order must be smaller than blocksize" );
    Subframe::Fixed f;
    f.order = order;
    for( std::uint8_t i = 0; i < order; ++i )
        f.warmup[ i ] = src[ i ];
    f.residual.residual = std::make_unique< std::int64_t[] >( blocksize - order );
    switch( order )
    {
    case 0:
        for( std::uint16_t i = order; i < blocksize; ++i )
            f.residual.residual[ i - order ] = src[ i ];
        break;
    case 1:
        for( std::uint16_t i = order; i < blocksize; ++i )
            f.residual.residual[ i - order ] = src[ i ] - src[ i - 1 ];
        break;
    case 2:
        for( std::uint16_t i = order; i < blocksize; ++i )
            f.residual.residual[ i - order ] = src[ i ] - 2 * src[ i - 1 ] + src[ i - 2 ];
        break;
    case 3:
        for( std::uint16_t i = order; i < blocksize; ++i )
            f.residual.residual[ i - order ] = src[ i ] - 3 * src[ i - 1 ] + 3 * src[ i - 2 ] - src[ i - 3 ];
        break;
    case 4:
        for( std::uint16_t i = order; i < blocksize; ++i )
            f.residual.residual[ i - order ] = src[ i ] - 4 * src[ i - 1 ] + 6 * src[ i - 2 ] - 4 * src[ i - 3 ] + src[ i - 4 ];
        break;
    default:
        throw exception( "EncodeFixed: unknown order" );
    }
    std::uint64_t const bits = FindBestResidualParameter( f.residual, order, blocksize );
    return std::make_tuple( std::move( f ), bits + bps * order );
}
std::tuple< Subframe::LPC, std::uint64_t > EncodeLPC( std::int64_t const *src, std::uint8_t const bps, std::uint8_t const max_order, std::uint16_t const blocksize )
{
    if( max_order > MAX_LPC_ORDER )
        throw exception( "EncodeLPC: max_order is too big" );
    std::uint8_t bounded_max_order = max_order >= blocksize ? blocksize - 1 : max_order;
    auto autoc = CalcAutocorrelation( bounded_max_order, src, blocksize );
    auto lpccoef = CalcLPCCoefficient( src, autoc.get(), bounded_max_order, blocksize );
    Subframe::LPC lpc;
    std::uint64_t min_bits = std::numeric_limits< decltype( min_bits ) >::max();
    for( std::uint8_t order = 1; order <= bounded_max_order; ++order )
    {
        for( std::uint8_t precision = MIN_QLP_COEFF_PRECISION; precision <= MAX_QLP_COEFF_PRECISION; ++precision )
        {
            auto encoded = EncodeLPCFixedOrderAndPrecision( order, precision, lpccoef.get(), src, bps, blocksize );
            if( std::get< 1 >( encoded ) < min_bits )
            {
                min_bits = std::get< 1 >( encoded );
                lpc = std::move( std::get< 0 >( encoded ) );
            }
        }
    }
    return std::make_tuple( std::move( lpc ), min_bits );
}
std::tuple< Subframe::Verbatim, std::uint64_t > EncodeVerbatim( std::int64_t const *src, std::uint8_t const bps, std::uint16_t const blocksize )
{
    Subframe::Verbatim ver;
    ver.data = std::make_unique< std::int64_t[] >( blocksize );
    std::memcpy( ver.data.get(), src, sizeof( std::int64_t ) * blocksize );
    return std::make_tuple( std::move( ver ), bps * blocksize );
}

} // namespace FLAC
