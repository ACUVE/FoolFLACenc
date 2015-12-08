#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <fstream>
#include <random>
#include <experimental/optional>

#include "flacutil/buffer.hpp"
#include "flacutil/flac_struct.hpp"
#include "flacutil/file.hpp"

#include "utility.hpp"

template< typename T >
using optional = std::experimental::optional< T >;

int main( int argc, char **argv )
try
{
    if( argc < 3 )
        fatal( "no filename" );
    file::sound_data sd;
    try
    {
        sd = file::decode_wavefile( argv[ 1 ] );
    }
    catch( ... )
    {
        std::cerr << "\"" << argv[ 1 ] << "\": decode error" << std::endl;
        throw;
    }
    file::print_sound_data( sd );
    
    FLAC::MetaData::StreamInfo si;
    si.min_blocksize = std::numeric_limits< decltype( si.min_blocksize ) >::max();
    si.max_blocksize = 0;
    si.min_framesize = std::numeric_limits< decltype( si.min_framesize ) >::max();
    si.max_blocksize = 0;
    si.sample_rate = sd.sample_rate;
    si.channels = sd.wave.size();
    si.bits_per_sample = sd.bits_per_sample;
    si.total_sample = sd.samples;
    std::memset( si.md5sum, 0, sizeof( si.md5sum ) );
    
    buffer::bytestream<> fbs;
    std::random_device rnd_d;
    std::mt19937 gen( rnd_d() );
    auto rand = std::bind( std::uniform_int_distribution< std::uint16_t >( 1 ), std::mt19937_64( rnd_d() ) );
    for( std::uint64_t sample = 0; sample < sd.samples; )
    {
        std::uint16_t blocksize = rand();
        if( sample + blocksize > sd.samples )
            blocksize = sd.samples - sample;
        FLAC::Frame::Frame f;
        f.header.blocksize = blocksize;
        f.header.sample_rate = sd.sample_rate;
        f.header.channels = sd.wave.size();
        f.header.channel_assignment = FLAC::Frame::ChannelAssignment::INDEPENDENT;
        f.header.bits_per_sample = sd.bits_per_sample;
        f.header.number_type = FLAC::Frame::NumberType::SAMPLE_NUMBER;
        f.header.number.sample_number = sample;
        for( std::size_t ch = 0; ch < sd.wave.size(); ++ch )
        {
            FLAC::Subframe::Subframe sf;
            sf.header.wasted_bits = 0;
            if( [&]{
                for( std::uint64_t s = sample, last = sample + blocksize; s < last; ++s )
                    if( sd.wave[ ch ][ sample ] != sd.wave[ ch ][ s ] )
                        return true;
                return false;
            }() )
            {
                sf.header.type = FLAC::Subframe::Type::VERBATIM;
                FLAC::Subframe::Verbatim ver;
                ver.data = std::make_unique< std::int64_t[] >( blocksize );
                std::memcpy( ver.data.get(), &sd.wave[ ch ][ sample ], sizeof( std::int64_t ) * blocksize );
                sf.data = std::move( ver );
            }
            else
            {
                sf.header.type = FLAC::Subframe::Type::CONSTANT;
                FLAC::Subframe::Constant con;
                con.value = sd.wave[ ch ][ sample ];
                sf.data = std::move( con );
            }
            f.subframes[ ch ] = std::move( sf );
        }
        std::size_t const pos = fbs.get_position();
        FLAC::WriteFrame( fbs, f );
        std::uint32_t const framesize = fbs.get_position() - pos;
        if( sample + blocksize != sd.samples || si.min_blocksize != si.max_blocksize )
        {
            si.min_blocksize = std::min( si.min_blocksize, blocksize );
            si.max_blocksize = std::max( si.max_blocksize, blocksize );
        }
        si.min_framesize = std::min( si.min_framesize, framesize );
        si.max_framesize = std::max( si.max_framesize, framesize );
        sample += blocksize;
    }
    FLAC::MetaData::Metadata md;
    md.type = FLAC::MetaData::Type::STREAMINFO;
    md.is_last = true;
    md.length = FLAC::STREAMINFO_LENGTH;
    md.data = std::move( si );
    buffer::bytestream<> mdbs;
    mdbs.put_bytes( FLAC::STREAM_SYNC_STRING, 4 );
    FLAC::WriteMetadata( mdbs, md );
    
    std::ofstream ofs( argv[ 2 ] );
    if( !ofs )
        fatal( argv[ 2 ], ": open error" );
    ofs.write( (char*)mdbs.data(), mdbs.get_position() );
    ofs.write( (char*)fbs.data(), fbs.get_position() );
}
catch( std::exception &e )
{
    std::cerr << e.what() << std::endl;
}
