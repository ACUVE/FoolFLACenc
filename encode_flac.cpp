#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <experimental/optional>

#include "flacutil/buffer.hpp"
#include "flacutil/flac_struct.hpp"

#include "utility.hpp"

int main( int argc, char **argv )
try
{
    if( argc < 3 )
        fatal( "no filename" );
    char const *filename = argv[ 1 ], *outfilename = argv[ 2 ];
    std::unique_ptr< std::uint8_t[] > buff;
    std::size_t size;
    std::tie( buff, size ) = read_file( filename );
    if( !buff )
        fatal( filename, " load error" );
    buffer::bytestream<> bs( buffer::buffer( std::move( buff ), size ) );
    if( std::memcmp( bs.get_bytes( 4 ).get(), FLAC::STREAM_SYNC_STRING, 4 ) != 0 )
        fatal( filename, " is not FLAC file." );
    
    std::experimental::optional< FLAC::MetaData::Metadata > simd;
    while( true )
    {
        auto md = FLAC::ReadMetadata( bs );
        if( md.type == FLAC::MetaData::Type::STREAMINFO )
            simd = std::move( md );
        if( md.is_last )
            break;
    }
    if( !simd )
        fatal( filename, ": No StreamInfo." );
    FLAC::PrintStreamInfo( simd->data.data< FLAC::MetaData::StreamInfo >() );
    buffer::bytestream<> outbs;
    simd->is_last = true;
    outbs.put_bytes( FLAC::STREAM_SYNC_STRING, 4 );
    FLAC::WriteMetadata( outbs, *simd );
    while( true )
    {
        std::cout << std::dec << outbs.get_position() << std::endl;
        auto frame = FLAC::ReadFrame( bs, simd->data.data< FLAC::MetaData::StreamInfo >() );
        FLAC::WriteFrame( outbs, frame );
        
        if( bs.get_position() >= size )
            break;
    }
    std::ofstream ofs( outfilename );
    if( !ofs )
        fatal( outfilename, " open error" );
    ofs.write( (char*)outbs.data(), outbs.get_position() );
}
catch( std::exception &e )
{
    std::cerr << e.what() << std::endl;
}
