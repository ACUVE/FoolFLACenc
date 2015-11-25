#include <cstdint>
#include "hash.hpp"

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

constexpr auto calc_crc8_table()
{
    array< std::uint8_t, 256 > buff = {};
    for( std::uint8_t i = 0; i < 256; ++i )
    {
        std::uint8_t c = 0;
        for( std::uint8_t j = 0; j < 8; ++j )
        {
            c = (c & 1) ? 0x07 ^ (c >> 1) : c >> 1;
        }
        buff[ i ] = c;
    }
    return buff;
}

// constexpr auto crc8_table = calc_crc8_table();
