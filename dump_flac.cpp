#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <utility>
#include <experimental/optional>

#include "flacutil/buffer.hpp"
#include "flacutil/flac_struct.hpp"

#include "utility.hpp"

void dump_flacfile( char const *filename )
{
    std::ifstream file( filename, std::ios::binary | std::ios::ate );
    if( !file )
        fatal( filename, " open error" );
    std::streamsize size = file.tellg();
    file.seekg( 0, std::ios::beg );
    auto buff = std::make_unique< std::uint8_t[] >(size);
    if( !file.read( (char *)buff.get(), size ) )
        fatal( filename, " is not readable." );
    buffer::bytestream<> bs( buffer::buffer( std::move( buff ), size ) );
    if( std::memcmp( bs.get_bytes( 4 ).get(), FLAC::STREAM_SYNC_STRING, 4 ) != 0 )
        fatal( filename, " is not FLAC file." );
    
    std::experimental::optional< FLAC::MetaData::StreamInfo > si;
    while( true )
    {
        auto md = FLAC::ReadMetadata( bs );
        if( md.type == FLAC::MetaData::Type::STREAMINFO )
            si = md.data.data< FLAC::MetaData::StreamInfo >();
        if( md.is_last )
            break;
    }
    if( !si )
        fatal( filename, ": No StreamInfo." );
    FLAC::PrintStreamInfo( *si );
    
    while( true )
    {
        auto frame = FLAC::ReadFrame( bs, *si );
        std::cout << "-----------------------------------" << std::endl;
        FLAC::PrintFrame( frame );
        if( bs.get_position() >= size )
            break;
    }
}

int main( int argc, char **argv )
{
    if( argc <= 1 )
        fatal( "No filename" );
    try
    {
        dump_flacfile( argv[ 1 ] );
    }
    catch( std::exception &e )
    {
        std::cerr << e.what() << std::endl;
    }
}
