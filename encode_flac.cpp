#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <experimental/optional>

#include "buffer.hpp"
#include "flac_struct.hpp"

[[noreturn]]
inline
bool fatal_impl( void )
{
    std::cerr << std::endl;
    std::exit( -1 );
}

template< typename T, typename... Args >
[[noreturn]]
inline
bool fatal_impl( T &&t,  Args &&... args )
{
    std::cerr << std::forward< T >( t );
    fatal_impl( std::forward< Args >( args )... );
}

template< typename... Args >
[[noreturn]]
inline
bool fatal( Args &&... args )
{
    std::cerr << "fatal: ";
    fatal_impl( std::forward< Args >( args )... );
}

int main( int argc, char **argv )
try
{
    if( argc < 3 )
        fatal( "no filename" );
    char const *filename = argv[ 1 ], *outfilename = argv[ 2 ];
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
