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

#include "utility.hpp"

template< typename T >
using optional = std::experimental::optional< T >;

struct sound_data
{
    std::vector< std::unique_ptr< std::int64_t[] > > wave;
    std::uint8_t                                     bps;
    std::uint64_t                                    length;
    std::uint32_t                                    sample_rate;
};

optional< sound_data > decode_wavefile( char const *filename ) noexcept
try
{
    std::unique_ptr< std::uint8_t[] > buff;
    std::size_t filesize;
    std::tie( buff, filesize ) = read_file( filename );
    if( !buff )
        return {};
    buffer::bytestream<> bs( buffer::buffer( std::move( buff ), filesize ) );
    auto le = buffer::make_bytestream_le( bs );
    sound_data sd;
    if( std::memcmp( bs.get_bytes( 4 ).get(), "RIFF", 4 ) != 0 )
        return {};
    bs.get_bytes( 4 );
    if( std::memcmp( bs.get_bytes( 4 ).get(), "WAVE", 4 ) != 0 )
        return {};
    if( std::memcmp( bs.get_bytes( 4 ).get(), "fmt ", 4 ) != 0 )
        return {};
    if( le.get32() != 0x10 )
        return {};
    if( le.get16() != 1 )
        return {};
    std::uint16_t const ch_num = le.get16();
    sd.sample_rate = le.get32();
    std::uint32_t const dataspeed = le.get32();
    std::uint16_t const blocksize = le.get16();
    std::uint16_t const bps = sd.bps = le.get16();
    if( blocksize != bps / 8 * ch_num )
        return {};
    if( dataspeed != bps / 8 * ch_num * sd.sample_rate )
        return {};
    if( std::memcmp( bs.get_bytes( 4 ).get(), "data", 4 ) != 0 )
        return {};
    std::size_t const size = le.get32();
    if( size % blocksize != 0 )
        return {};
    std::size_t const sample_num = sd.length = size / blocksize;
    for( std::uint16_t ch = 0; ch < ch_num; ++ch )
        sd.wave.emplace_back( std::make_unique< std::int64_t[] >( sample_num ) );
    for( std::size_t i = 0; i < sample_num; ++i )
        for( std::uint16_t ch = 0; ch < ch_num; ++ch )
            sd.wave[ ch ][ i ] = le.get_int( bps / 8 );
    return std::move( sd );
}
catch( ... )
{
    return {};
}

void print_sound_data( sound_data const &sd )
{
    std::cout << "sound_data" << std::endl;
    std::cout << std::dec;
    std::cout << "  bps         = " << (int)sd.bps         << std::endl;
    std::cout << "  length      = " <<      sd.length      << std::endl;
    std::cout << "  sample_rate = " <<      sd.sample_rate << std::endl;
    std::cout << "  channels    = " <<      sd.wave.size() << std::endl;
}

int main( int argc, char **argv )
try
{
    if( argc < 3 )
        fatal( "no filename" );
    auto sd = decode_wavefile( argv[ 1 ] );
    if( !sd )
        fatal( argv[ 1 ], ": wave decode error" );
    print_sound_data( *sd );
    
    FLAC::MetaData::StreamInfo si;
    si.min_blocksize = std::numeric_limits< decltype( si.min_blocksize ) >::max();
    si.max_blocksize = 0;
    si.min_framesize = std::numeric_limits< decltype( si.min_framesize ) >::max();
    si.max_blocksize = 0;
    si.sample_rate = sd->sample_rate;
    si.channels = sd->wave.size();
    si.bits_per_sample = sd->bps;
    si.total_sample = sd->length;
    std::memset( si.md5sum, 0, sizeof( si.md5sum ) );
    
    buffer::bytestream<> fbs;
    std::random_device rnd_d;
    std::mt19937 gen( rnd_d() );
    auto rand = std::bind( std::uniform_int_distribution< std::uint16_t >( 1 ), std::mt19937_64( rnd_d() ) );
    for( std::uint64_t sample = 0; sample < sd->length; )
    {
        std::uint16_t blocksize = rand();
        if( sample + blocksize > sd->length )
            blocksize = sd->length - sample;
        FLAC::Frame::Frame f;
        f.header.blocksize = blocksize;
        f.header.sample_rate = sd->sample_rate;
        f.header.channels = sd->wave.size();
        f.header.channel_assignment = FLAC::Frame::ChannelAssignment::INDEPENDENT;
        f.header.bits_per_sample = sd->bps;
        f.header.number_type = FLAC::Frame::NumberType::SAMPLE_NUMBER;
        f.header.number.sample_number = sample;
        for( std::size_t ch = 0; ch < sd->wave.size(); ++ch )
        {
            FLAC::Subframe::Subframe sf;
            sf.header.type = FLAC::Subframe::Type::VERBATIM;
            sf.header.wasted_bits = 0;
            FLAC::Subframe::Verbatim ver;
            ver.data = std::make_unique< std::int64_t[] >( blocksize );
            std::memcpy( ver.data.get(), &sd->wave[ ch ][ sample ], sizeof( std::int64_t ) * blocksize );
            sf.data = std::move( ver );
            f.subframes[ ch ] = std::move( sf );
        }
        std::size_t const pos = fbs.get_position();
        FLAC::WriteFrame( fbs, f );
        std::uint32_t const framesize = fbs.get_position() - pos;
        if( sample + blocksize != sd->length || si.min_blocksize != si.max_blocksize )
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
