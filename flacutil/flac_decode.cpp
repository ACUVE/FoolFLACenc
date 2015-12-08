#include <iostream>
#include "flac_decode.hpp"
#include "flac_struct.hpp"

namespace FLAC
{

std::unique_ptr< std::int64_t[] > DecodeConstant( Subframe::Constant const &c, std::uint16_t const blocksize )
{
    auto buff = std::make_unique< std::int64_t[] >( blocksize );
    DecodeConstant( buff.get(), c, blocksize );
    return std::move( buff );
}
std::unique_ptr< std::int64_t[] > DecodeFixed( Subframe::Fixed const &f, std::uint16_t const blocksize )
{
    auto buff = std::make_unique< std::int64_t[] >( blocksize );
    DecodeFixed( buff.get(), f, blocksize );
    return std::move( buff );
}
std::unique_ptr< std::int64_t[] > DecodeLPC( Subframe::LPC const &lpc, std::uint16_t const blocksize )
{
    auto buff = std::make_unique< std::int64_t[] >( blocksize );
    DecodeLPC( buff.get(), lpc, blocksize );
    return std::move( buff );
}
std::unique_ptr< std::int64_t[] > DecodeVerbatim( Subframe::Verbatim const &v, std::uint16_t const blocksize )
{
    auto buff = std::make_unique< std::int64_t[] >( blocksize );
    DecodeVerbatim( buff.get(), v, blocksize );
    return std::move( buff );
}
std::unique_ptr< std::int64_t[] > DecodeSubframe( Subframe::Subframe const &s, std::uint16_t const blocksize )
{
    auto buff = std::make_unique< std::int64_t[] >( blocksize );
    DecodeSubframe( buff.get(), s, blocksize );
    return std::move( buff );
}

void DecodeConstant( std::int64_t *buff, Subframe::Constant const &c, std::uint16_t const blocksize ) noexcept
{
    for( std::uint16_t i = 0; i < blocksize; ++i )
        buff[ i ] = c.value;
}
void DecodeFixed( std::int64_t *buff, Subframe::Fixed const &f, std::uint16_t const blocksize ) noexcept
{
    for( std::uint16_t i = 0; i < f.order; ++i )
        buff[ i ] = f.warmup[ i ];
    switch( f.order )
    {
    case 0:
        for( std::uint16_t i = f.order; i < blocksize; ++i )
            buff[ i ] = f.residual.residual[ i - f.order ];
        break;
    case 1:
        for( std::uint16_t i = f.order; i < blocksize; ++i )
            buff[ i ] = f.residual.residual[ i - f.order ] + buff[ i - 1 ];
        break;
    case 2:
        for( std::uint16_t i = f.order; i < blocksize; ++i )
            buff[ i ] = f.residual.residual[ i - f.order ] + 2 * buff[ i - 1 ] - buff[ i - 2 ];
        break;
    case 3:
        for( std::uint16_t i = f.order; i < blocksize; ++i )
            buff[ i ] = f.residual.residual[ i - f.order ] + 3 * buff[ i - 1 ] - 3 * buff[ i - 2 ] + buff[ i - 3 ];
        break;
    case 4:
        for( std::uint16_t i = f.order; i < blocksize; ++i )
            buff[ i ] = f.residual.residual[ i - f.order ] + 4 * buff[ i - 1 ] - 6 * buff[ i - 2 ] + 4 * buff[ i - 3 ] - buff[ i - 4 ];
        break;
    default:
        throw exception( "DecodeFixed: unknown order" );
    }
}
void DecodeLPC( std::int64_t *buff, Subframe::LPC const &lpc, std::uint16_t const blocksize ) noexcept
{
    for( std::uint16_t i = 0; i < lpc.order; ++i )
        buff[ i ] = lpc.warmup[ i ];
    for( std::uint16_t i = lpc.order; i < blocksize; ++i )
    {
        std::int64_t sum = 0;
        for( std::uint8_t j = 0; j < lpc.order; ++j )
            sum += lpc.qlp_coeff[ j ] * buff[ i - j - 1 ];
        buff[ i ] = lpc.residual.residual[ i - lpc.order ] + (sum >> lpc.quantization_level);
    }
}
void DecodeVerbatim( std::int64_t *buff, Subframe::Verbatim const &v, std::uint16_t const blocksize ) noexcept
{
    for( std::uint16_t i = 0; i < blocksize; ++i )
        buff[ i ] = v.data[ i ];
}
void DecodeSubframe( std::int64_t *buff, Subframe::Subframe const &s, std::uint16_t const blocksize ) noexcept
{
    switch( s.header.type )
    {
    case Subframe::Type::CONSTANT:
        DecodeConstant( buff, s.data.data< Subframe::Constant >(), blocksize );
        break;
    case Subframe::Type::FIXED:
        DecodeFixed( buff, s.data.data< Subframe::Fixed >(), blocksize );
        break;
    case Subframe::Type::LPC:
        DecodeLPC( buff, s.data.data< Subframe::LPC >(), blocksize );
        break;
    case Subframe::Type::VERBATIM:
        DecodeVerbatim( buff, s.data.data< Subframe::Verbatim >(), blocksize );
        break;
    }
    if( s.header.wasted_bits != 0 )
        for( std::uint16_t i = 0; i < blocksize; ++i )
            buff[ i ] <<= s.header.wasted_bits;
}

} // namespace FLAC
