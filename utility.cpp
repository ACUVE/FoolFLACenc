#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>
#include <fstream>

#include "utility.hpp"

std::tuple< std::unique_ptr< std::uint8_t[] >, std::size_t > read_file( char const *filename )
{
    std::ifstream file( filename, std::ios::binary | std::ios::ate );
    if( !file )
        return std::make_tuple( nullptr, 0 );
    std::streamsize size = file.tellg();
    file.seekg( 0, std::ios::beg );
    auto buff = std::make_unique< std::uint8_t[] >(size);
    if( !file.read( (char *)buff.get(), size ) )
        return std::make_tuple( nullptr, 0 );
    return std::make_tuple( std::move( buff ), size );
}
