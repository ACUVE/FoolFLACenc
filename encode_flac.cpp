#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "flacutil/buffer.hpp"
#include "flacutil/flac_encode.hpp"
#include "flacutil/flac_struct.hpp"
#include "flacutil/file.hpp"

#include "utility.hpp"

constexpr std::uint16_t prog_blocksize = 8192;

static
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
        auto lpc = FLAC::EncodeLPC( first_sample, bps, FLAC::MAX_LPC_ORDER, blocksize );
        if( std::get< 1 >( lpc ) < best_bits )
        {
            sf.header.type = FLAC::Subframe::Type::LPC;
            sf.data = std::move( std::get< 0 >( lpc ) );
            best_bits = std::get< 1 >( lpc );
        }
    }
    return std::make_tuple( std::move( sf ), best_bits );
}
class progress{
private:
    std::uint64_t const          maxvalue;
    std::condition_variable      cond;
    std::atomic< std::uint64_t > value;
    std::mutex                   mutex;
    
public:
    progress( std::uint64_t const maxvalue )
        : maxvalue( maxvalue )
        , value( 0 )
    {
    }
    void operator+=( std::uint64_t const v )
    {
        value += v;
        cond.notify_all();
    }
    template< class Rep, class Period >
    void wait_for( std::chrono::duration<Rep, Period> const &rel_time )
    {
        std::uint64_t const v = value.load();
        if( v >= maxvalue )
            return;
        std::unique_lock< std::mutex > ul( mutex );
        cond.wait_for( ul, rel_time, [ & ]{ return v != value.load(); } );
    }
    std::uint64_t get() const noexcept
    {
        return value.load();
    }
    bool is_end() const noexcept
    {
        return value.load() >= maxvalue;
    }
    std::uint64_t get_maxvalue() const noexcept
    {
        return maxvalue;
    }
};
// return: bytestream, min_framesize, max_framesize
static
std::tuple< buffer::bytestream<>, std::uint32_t, std::uint32_t > EncodePartial( file::sound_data const &sd, std::uint64_t const first_sample_num, std::uint64_t const length, std::uint16_t const blocksize, progress &pro )
{
    std::uint64_t const last_sample = first_sample_num + length;
    buffer::bytestream<> fbs;
    std::uint32_t min_framesize = std::numeric_limits< decltype( min_framesize ) >::max();
    std::uint32_t max_framesize = 0;
    for( std::uint64_t sample = first_sample_num; sample < last_sample; sample += blocksize )
    {
        std::uint16_t const this_blocksize = sample + blocksize > last_sample ? last_sample - sample : blocksize;
        FLAC::Frame::Frame f;
        f.header.blocksize = this_blocksize;
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
            std::tie( f.subframes[ ch ], best_bits ) = EncodeSubframe( &sd.wave[ ch ][ sample ], sd.bits_per_sample, this_blocksize );
            bits += best_bits;
        }
        if( sd.wave.size() == 2 )
        {
            auto mid = std::make_unique< std::int64_t[] >( this_blocksize );
            auto side = std::make_unique< std::int64_t[] >( this_blocksize );
            for( std::uint16_t i = 0; i < this_blocksize; ++i )
            {
                std::int64_t m, s, x;
                m = s = sd.wave[ 0 ][ sample + i ];
                x = sd.wave[ 1 ][ sample + i ];
                m += x;
                s -= x;
                m >>= 1; // TODO: OK?
                mid[ i ] = m;
                side[ i ] = s;
            }
            auto mid_subframe = EncodeSubframe( mid.get(), sd.bits_per_sample, this_blocksize );
            auto side_subframe = EncodeSubframe( side.get(), sd.bits_per_sample + 1, this_blocksize );
            std::uint64_t const mid_side_bits = std::get< 1 >( mid_subframe ) + std::get< 1 >( side_subframe );
            if( mid_side_bits < bits )
            {
                f.header.channel_assignment = FLAC::Frame::ChannelAssignment::MID_SIDE;
                f.subframes[ 0 ] = std::move( std::get< 0 >( mid_subframe ) );
                f.subframes[ 1 ] = std::move( std::get< 0 >( side_subframe ) );
                bits = mid_side_bits;
            }
        }
        std::size_t const pos = fbs.get_position();
        FLAC::WriteFrame( fbs, f );
        std::uint32_t const framesize = fbs.get_position() - pos;
        min_framesize = std::min( min_framesize, framesize );
        max_framesize = std::max( max_framesize, framesize );
        pro += this_blocksize;
    }
    return std::make_tuple( std::move( fbs ), min_framesize, max_framesize );
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
    si.min_blocksize = prog_blocksize;
    si.max_blocksize = prog_blocksize;
    si.min_framesize = std::numeric_limits< decltype( si.min_framesize ) >::max();
    si.max_framesize = 0;
    si.sample_rate = sd.sample_rate;
    si.channels = sd.wave.size();
    si.bits_per_sample = sd.bits_per_sample;
    si.total_sample = sd.samples;
    std::memset( si.md5sum, 0, sizeof( si.md5sum ) );
    
    progress pro( sd.samples );
    unsigned int const num_cpu = std::thread::hardware_concurrency();
    std::vector< std::future< std::tuple< buffer::bytestream<>, std::uint32_t, std::uint32_t > > > fuvec;
    for( unsigned int i = 0; i < num_cpu; ++i )
    {
        std::promise< decltype( fuvec[ 0 ].get() ) > p;
        fuvec.emplace_back( p.get_future() );
        std::thread( [ &, i, p = std::move( p ) ]() mutable
        {
            try
            {
                std::uint64_t const def_sample = sd.samples / prog_blocksize / num_cpu * prog_blocksize;
                auto enc = EncodePartial( sd, def_sample * i, i == num_cpu - 1 ? sd.samples - def_sample * i : def_sample, prog_blocksize, pro );
                p.set_value( std::move( enc ) );
            }
            catch( ... )
            {
                p.set_exception( std::current_exception() );
            }
        }).detach();
    }
    auto start_time = std::chrono::high_resolution_clock::now();
    while( !pro.is_end() )
    {
        pro.wait_for( std::chrono::seconds( 1 ) );
        auto now_time = std::chrono::high_resolution_clock::now();
        auto d = std::chrono::duration_cast< std::chrono::nanoseconds >( now_time - start_time );
        std::printf( "\r%6.2f%% ", static_cast< double >( pro.get() ) / pro.get_maxvalue() * 100 );
        if( pro.get() )
        {
            auto time = static_cast< double >( d.count() ) / pro.get() * ( pro.get_maxvalue() - pro.get() );
            auto sec = static_cast< std::uint64_t >( time * decltype( d )::period::num / decltype( d )::period::den );
            if( sec < 1e5 && !pro.is_end() )
                std::printf( "%5" PRIu64 "s", sec );
            else
                std::printf( "      " );
        }
        std::cout << std::flush;
    }
    std::cout << "\n" << "done!" << std::endl;
    std::vector< decltype( fuvec[ 0 ].get() ) > datavec;
    for( auto &&fu : fuvec )
    {
        auto encdata = fu.get();
        si.min_framesize = std::min( std::get< 1 >( encdata ), si.min_framesize );
        si.max_framesize = std::max( std::get< 2 >( encdata ), si.max_framesize );
        datavec.emplace_back( std::move( encdata ) );
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
    for( auto &&encdata : datavec )
        ofs.write( (char*)std::get< 0 >( encdata ).data(), std::get< 0 >( encdata ).get_position() );
}
catch( std::exception &e )
{
    std::cerr << e.what() << std::endl;
}
