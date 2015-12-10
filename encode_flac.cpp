#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <random>
#include <string>
#include <tuple>

#include "flacutil/buffer.hpp"
#include "flacutil/flac_encode.hpp"
#include "flacutil/flac_struct.hpp"
#include "flacutil/file.hpp"

#include "utility.hpp"

std::tuple< FLAC::Subframe::Subframe, std::uint64_t > EncodeSubframe( std::int64_t const *first_sample, std::uint8_t const bps, std::uint16_t const blocksize )
{
    FLAC::Subframe::Subframe sf;
    sf.header.wasted_bits = 0;
    std::uint64_t best_bits = std::numeric_limits< decltype( best_bits ) >::max();
    if( [&]{
        for( auto sample = first_sample, last = sample + blocksize; sample < last; ++sample )
            if( *sample != *first_sample )
                return false;
        return true;
    }() )
    {
        auto con = FLAC::EncodeConstant( first_sample, bps, blocksize );
        sf.header.type = FLAC::Subframe::Type::CONSTANT;
        sf.data = std::get< 0 >( con );
        best_bits = std::get< 1 >( con );
    }
    else
    {
        auto ver = FLAC::EncodeVerbatim( first_sample, bps, blocksize );
        if( std::get< 1 >( ver ) < best_bits )
        {
            sf.header.type = FLAC::Subframe::Type::VERBATIM;
            sf.data = std::move( std::get< 0 >( ver ) );
            best_bits = std::get< 1 >( ver );
        }
        for( std::uint8_t order = 0; order <= 4; ++order )
        {
            auto fixed = FLAC::EncodeFixed( first_sample, bps, order, blocksize );
            if( std::get< 1 >( fixed ) < best_bits )
            {
                sf.header.type = FLAC::Subframe::Type::FIXED;
                sf.data = std::move( std::get< 0 >( fixed ) );
                best_bits = std::get< 1 >( fixed );
            }
        }
    }
    return std::make_tuple( std::move( sf ), best_bits );
}

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
    std::size_t const sample_width = std::to_string( sd.samples ).length();
    for( std::uint64_t sample = 0; sample < sd.samples; )
    {
        std::cout << std::setw( sample_width ) << sample << " / " << sd.samples << " : " << std::flush;
        std::uint16_t blocksize = 8192;
        if( sample + blocksize > sd.samples )
            blocksize = sd.samples - sample;
        FLAC::Frame::Frame f;
        f.header.blocksize = blocksize;
        f.header.sample_rate = sd.sample_rate;
        f.header.channels = sd.wave.size();
        f.header.channel_assignment = FLAC::Frame::ChannelAssignment::INDEPENDENT;
        f.header.bits_per_sample = sd.bits_per_sample;
        f.header.number_type = FLAC::Frame::NumberType::FRAME_NUMBER;
        f.header.number.frame_number = sample / blocksize;
        std::uint64_t bits = 0;
        for( std::size_t ch = 0; ch < sd.wave.size(); ++ch )
        {
            std::uint64_t best_bits;
            std::tie( f.subframes[ ch ], best_bits ) = EncodeSubframe( &sd.wave[ ch ][ sample ], sd.bits_per_sample, blocksize );
            bits += best_bits;
        }
        std::size_t const pos = fbs.get_position();
        FLAC::WriteFrame( fbs, f );
        std::uint32_t const framesize = fbs.get_position() - pos;
        std::uint32_t const wavesize = blocksize * sd.bits_per_sample * sd.wave.size() / 8;
        std::cout << wavesize << " bytes -> " << framesize << " bytes" << " : " << static_cast< double >( framesize ) / wavesize  * 100 << " %" << std::endl;
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
