#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <utility>

#include "bitstream.hpp"
#include "flac_struct.hpp"

constexpr char const *filename = "flac.flac";

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
{
    char const *openfilename = argc > 1 ? argv[ 1 ] : filename;
    std::ifstream file( openfilename, std::ios::binary | std::ios::ate );
    if( !file )
        fatal( openfilename, " open error" );
    std::streamsize size = file.tellg();
    file.seekg( 0, std::ios::beg );
    auto buff = std::make_unique< std::uint8_t[] >(size);
    if( !file.read( (char *)buff.get(), size ) )
        fatal( openfilename, " is not readable." );
    bitstream bs( std::move( buff ), size );
    if( std::memcmp( bs.get_bytes< 4 >().data(), FLAC::STREAM_SYNC_STRING, 4 ) != 0 )
        fatal( openfilename, " is not FLAC file." );
    if( bs.is_error() )
        fatal( openfilename, " is not FLAC file?" );
    
    FLAC::optional< FLAC::MetaData::StreamInfo > si;
    while( true )
    {
        auto md = FLAC::ReadMetadata( bs );
        if( !md )
            fatal( openfilename, ": ReadMetadata error." );
        if( md->type == FLAC::MetaData::Type::STREAMINFO )
            si = md->data.data< FLAC::MetaData::StreamInfo >();
        if( md->is_last )
            break;
    }
    if( !si )
        fatal( openfilename, ": No StreamInfo.");
    FLAC::PrintStreamInfo( *si );
    
    while( true )
    {
        auto frame = FLAC::ReadFrame( bs, *si );
        if( !frame )
            fatal( openfilename, ": ReadFrame error.");
        std::cout << "-----------------------------------" << std::endl;
        FLAC::PrintFrame( *frame );
        if( std::get< 0 >( bs.get_position() ) >= size )
            break;
    }
}
