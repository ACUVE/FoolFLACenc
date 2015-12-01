#include <cstdint>
#include "hash.hpp"

namespace hash{

template< typename T, std::size_t size >
struct array
{
    T data[size];
    constexpr T &operator[]( std::size_t index )
    {
        return data[ index ];
    }
    constexpr T const &operator[]( std::size_t index ) const
    {
        return data[ index ];
    }
};

constexpr auto calc_crc8_table() noexcept
{
    array< std::uint8_t, 256 > buff = {};
    for( int i = 0; i < 256; ++i )
    {
        std::uint8_t v = i;
        std::uint8_t c = 0;
        for( int j = 0; j < 8; ++j )
        {
            if( v & (1 << (7 - j)) )
            {
                c ^= 0b00000111 << (7 - j);
                if( j < 7 )
                    v ^= 0b00000111 >> (j + 1);
            }
        }
        buff[ i ] = c;
    }
    return buff;
}

constexpr auto crc8_table = calc_crc8_table();
void crc8_update( std::uint8_t &crc, std::uint8_t const data ) noexcept
{
    crc = crc8_table[ crc ^ data ];
}
void crc8_update( std::uint8_t &crc, std::uint8_t const *data, std::size_t const len ) noexcept
{
    for( std::size_t i = 0; i < len; ++i )
        crc8_update( crc, data[ i ] );
}
std::uint8_t crc8( std::uint8_t const *data, std::size_t const len ) noexcept
{
    std::uint8_t crc = 0;
    crc8_update( crc, data, len );
    return crc;
}

constexpr auto calc_crc16_table() noexcept
{
    array< std::uint16_t, 256 > buff = {};
    for( int i = 0; i < 256; ++i )
    {
        std::uint8_t v = i;
        std::uint16_t c = 0;
        for( int j = 0; j < 8; ++j )
        {
            if( v & (1 << (7 - j)) )
            {
                c ^= 0b1000000000000101 << (7 - j);
                if( j < 7 )
                    v ^= 0b1000000000000101 >> (j + 9);
            }
        }
        buff[ i ] = c;
    }
    return buff;
}
constexpr auto crc16_table = calc_crc16_table();
void crc16_update( std::uint16_t &crc, std::uint8_t const data ) noexcept
{
    crc = ((crc << 8) ^ crc16_table[ (crc >> 8) ^ data ]);
}
void crc16_update( std::uint16_t &crc, std::uint8_t const *data, std::size_t const len ) noexcept
{
    for( std::size_t i = 0; i < len; ++i )
        crc16_update( crc, data[ i ] );
}
std::uint16_t crc16( std::uint8_t const *data, std::size_t const len ) noexcept
{
    std::uint16_t crc = 0;
    crc16_update( crc, data, len );
    return crc;
}

} // namespace hash
