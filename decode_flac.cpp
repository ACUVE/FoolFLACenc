#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <experimental/optional>

#include "flacutil/buffer.hpp"
#include "flacutil/flac_decode.hpp"
#include "flacutil/flac_struct.hpp"

#include "utility.hpp"

struct sound_data
{
    std::vector< std::unique_ptr< std::int64_t[] > > wave;
    std::uint8_t                                     bps;
    std::uint64_t                                    length;
    std::uint32_t                                    sample_rate;
};

sound_data decode_flacfile( char const *filename )
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
    
    sound_data sound;
    sound.length = si->total_sample;
    sound.sample_rate = si->sample_rate;
    sound.bps = si->bits_per_sample;
    for( std::uint8_t ch = 0; ch < si->channels; ++ch )
        sound.wave.emplace_back( std::make_unique< std::int64_t[] >( si->total_sample ) );
    
    std::uint64_t sample = 0;
    while( true )
    {
        auto frame = FLAC::ReadFrame( bs, *si );
        for( std::uint8_t ch = 0; ch < frame.header.channels; ++ch )
        {
            switch( frame.subframes[ ch ].header.type )
            {
            case FLAC::Subframe::Type::CONSTANT:
                FLAC::DecodeConstant( &sound.wave[ ch ][ sample ], frame.subframes[ ch ].data.data< FLAC::Subframe::Constant >(), frame.header.blocksize );
                break;
            case FLAC::Subframe::Type::FIXED:
                FLAC::DecodeFixed( &sound.wave[ ch ][ sample ], frame.subframes[ ch ].data.data< FLAC::Subframe::Fixed >(), frame.header.blocksize );
                break;
            case FLAC::Subframe::Type::LPC:
                FLAC::DecodeLPC( &sound.wave[ ch ][ sample ], frame.subframes[ ch ].data.data< FLAC::Subframe::LPC >(), frame.header.blocksize );
                break;
            case FLAC::Subframe::Type::VERBATIM:
                FLAC::DecodeVerbatim( &sound.wave[ ch ][ sample ], frame.subframes[ ch ].data.data< FLAC::Subframe::Verbatim >(), frame.header.blocksize );
                break;
            default:
                fatal( filename, " has unknown subframe type" );
            }
        }
        switch( frame.header.channel_assignment )
        {
        case FLAC::Frame::ChannelAssignment::INDEPENDENT:
            // do nothing
            break;
        case FLAC::Frame::ChannelAssignment::LEFT_SIDE:
            if( frame.header.channels != 2 )
                fatal( "the number of channel is wrong" );
            for( std::uint16_t i = 0; i < frame.header.blocksize; ++i )
                sound.wave[ 1 ][ sample + i ] = sound.wave[ 0 ][ sample + i ] - sound.wave[ 1 ][ sample + i ];
            break;
        case FLAC::Frame::ChannelAssignment::RIGHT_SIDE:
            if( frame.header.channels != 2 )
                fatal( "the number of channel is wrong" );
            for( std::uint16_t i = 0; i < frame.header.blocksize; ++i )
                sound.wave[ 0 ][ sample + i ] += sound.wave[ 1 ][ sample + i ];
            break;
        case FLAC::Frame::ChannelAssignment::MID_SIDE:
            if( frame.header.channels != 2 )
                fatal( "the number of channel is wrong" );
            for( std::uint16_t i = 0; i < frame.header.blocksize; ++i )
            {
                std::int64_t mid = sound.wave[ 0 ][ sample + i ];
                std::int64_t side = sound.wave[ 1 ][ sample + i ];
                mid = static_cast< std::uint64_t >( mid ) << 1; // TODO: OK?
                mid |= (side & 1); // TODO: OK?
                sound.wave[ 0 ][ sample + i ] = (mid + side) >> 1; // TODO: OK?
                sound.wave[ 1 ][ sample + i ] = (mid - side) >> 1; // TODO: OK?
            }
            break;
        }
        
        if( bs.get_position() >= size )
            break;
        sample += frame.header.blocksize;
    }
    
    return std::move( sound );
}

int main( int argc, char **argv )
try
{
    if( argc <= 2 )
        fatal( "No filename" );
    sound_data sound = decode_flacfile( argv[ 1 ] );
    
    std::uint16_t i2; std::uint32_t i4;
    std::ofstream ofs( argv[ 2 ] );
                                                                ofs << "RIFF";
    i4 = sound.wave.size() * sound.bps / 8 * sound.length;      ofs.write( (char*)&i4, 4);
                                                                ofs << "WAVE";
                                                                ofs << "fmt ";
    i4 = 16;                                                    ofs.write( (char*)&i4, 4);
    i2 = 1;                                                     ofs.write( (char*)&i2, 2);
    i2 = sound.wave.size();                                     ofs.write( (char*)&i2, 2);
    i4 = sound.sample_rate;                                     ofs.write( (char*)&i4, 4);
    i4 = sound.sample_rate * sound.wave.size() * sound.bps / 8; ofs.write( (char*)&i4, 4);
    i2 = sound.bps / 8 * sound.wave.size();                     ofs.write( (char*)&i2, 2);
    i2 = sound.bps;                                             ofs.write( (char*)&i2, 2);
                                                                ofs << "data";
    i4 = sound.bps / 8 * sound.wave.size() * sound.length;      ofs.write( (char*)&i4, 4);
    for( std::uint64_t i = 0; i < sound.length; ++i )
    {
        for( std::uint8_t ch = 0; ch < sound.wave.size(); ++ch )
        {
            i4 = sound.wave[ ch ][ i ];
            ofs.write( (char*)&i4, sound.bps / 8 );
        }
    }
}
catch( std::exception &e )
{
    std::cerr << e.what() << std::endl;
}
