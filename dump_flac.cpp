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
    std::unique_ptr< std::uint8_t[] > buff;
    std::size_t size;
    std::tie( buff, size ) = read_file( filename );
    if( !buff )
        fatal( filename, " load error" );
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
    
    std::size_t frame_num = 0;
    try
    {
        while( true )
        {
            auto frame = FLAC::ReadFrame( bs, *si );
            std::cout << "-----------------------------------" << std::endl;
            FLAC::PrintFrame( frame );
            if( bs.get_position() >= size )
                break;
            frame_num++;
        }
    }
    catch( ... )
    {
        std::cerr << "dump_flacfile error" << std::endl;
        std::cerr << "  frame_num = " << frame_num << std::endl;
        throw;
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
