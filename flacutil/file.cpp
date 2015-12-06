#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <tuple>
#include <vector>

#include "buffer.hpp"
#include "file.hpp"
#include "flac_struct.hpp"

namespace file{

void print_sound_data( sound_data const &sd )
{
    std::cout << "sound_data" << std::endl;
    std::cout << std::dec;
    std::cout << "  bps         = " << (int)sd.bits_per_sample << std::endl;
    std::cout << "  length      = " <<      sd.samples         << std::endl;
    std::cout << "  sample_rate = " <<      sd.sample_rate     << std::endl;
    std::cout << "  channels    = " <<      sd.wave.size()     << std::endl;
}

static
std::tuple< std::unique_ptr< std::uint8_t[] >, std::size_t > read_file( char const *filename )
{
    std::ifstream file( filename, std::ios::binary | std::ios::ate );
    if( !file )
        throw FLAC::exception( "read_file: open file error" );
    std::streamsize size = file.tellg();
    file.seekg( 0, std::ios::beg );
    auto buff = std::make_unique< std::uint8_t[] >( size );
    if( !file.read( (char *)buff.get(), size ) )
        throw FLAC::exception( "read_file: read file error" );
    return std::make_tuple( std::move( buff ), size );
}

sound_data decode_wavefile( char const *filename )
{
    std::unique_ptr< std::uint8_t[] > buff;
    std::size_t filesize;
    std::tie( buff, filesize ) = read_file( filename );
    buffer::bytestream<> bs( buffer::buffer( std::move( buff ), filesize ) );
    auto le = buffer::make_bytestream_le( bs );
    sound_data sd;
    if( std::memcmp( bs.get_bytes( 4 ).get(), "RIFF", 4 ) != 0 )
        throw FLAC::exception( "decode_wavefile: not riff file" );
    bs.get_bytes( 4 );
    if( std::memcmp( bs.get_bytes( 4 ).get(), "WAVE", 4 ) != 0 )
        throw FLAC::exception( "decode_wavefile: not wave file" );
    if( std::memcmp( bs.get_bytes( 4 ).get(), "fmt ", 4 ) != 0 )
        throw FLAC::exception( "decode_wavefile: not wave file?" );
    if( le.get32() != 0x10 )
        throw FLAC::exception( "decode_wavefile: fmt's size must be 0x10" );
    if( le.get16() != 1 )
        throw FLAC::exception( "decode_wavefile: only support integer lpcm" );
    std::uint16_t const ch_num = le.get16();
    sd.sample_rate = le.get32();
    std::uint32_t const dataspeed = le.get32();
    std::uint16_t const blocksize = le.get16();
    std::uint16_t const bps = sd.bits_per_sample = le.get16();
    if( blocksize != bps / 8 * ch_num )
        throw FLAC::exception( "decode_wavefile: blocksize is wrong" );
    if( dataspeed != bps / 8 * ch_num * sd.sample_rate )
        throw FLAC::exception( "decode_wavefile: dataspeed is wrong" );
    if( std::memcmp( bs.get_bytes( 4 ).get(), "data", 4 ) != 0 )
        throw FLAC::exception( "decode_wavefile: not wave file??" );
    std::size_t const size = le.get32();
    if( size % blocksize != 0 )
        throw FLAC::exception( "decode_wavefile: data size is wrong" );
    std::size_t const sample_num = sd.samples = size / blocksize;
    for( std::uint16_t ch = 0; ch < ch_num; ++ch )
        sd.wave.emplace_back( std::make_unique< std::int64_t[] >( sample_num ) );
    for( std::size_t i = 0; i < sample_num; ++i )
        for( std::uint16_t ch = 0; ch < ch_num; ++ch )
            sd.wave[ ch ][ i ] = le.get_int( bps / 8 );
    return std::move( sd );
}

} // namespace
