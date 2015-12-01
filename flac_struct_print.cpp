#include <cstdint>
#include <iostream>
#include <iomanip>
#include "flac_struct.hpp"

namespace FLAC
{

void PrintStreamInfo( FLAC::MetaData::StreamInfo const &si )
{
    std::cout << "StreamInfo" << std::endl;
    std::cout << std::dec;
    std::cout << "  min_blocksize       = " <<      si.min_blocksize   << std::endl;
    std::cout << "  max_blocksize       = " <<      si.max_blocksize   << std::endl;
    std::cout << "  min_framesize       = " <<      si.min_framesize   << std::endl;
    std::cout << "  max_framesize       = " <<      si.max_framesize   << std::endl;
    std::cout << "  sample_rate         = " <<      si.sample_rate     << std::endl;
    std::cout << "  channels            = " << (int)si.channels        << std::endl;
    std::cout << "  bits_per_sample     = " << (int)si.bits_per_sample << std::endl;
    std::cout << "  total_sample        = " <<      si.total_sample    << std::endl;
    std::cout << "  md5sum              = ";
    for( int i = 0; i < 16; ++i )
        std::cout << std::hex << std::setw( 2 ) << std::setfill( '0' ) << (int)si.md5sum[ i ];
    std::cout << std::endl;
}
void PrintFrameHeader( FLAC::Frame::Header const &fh )
{
    std::cout << "Frame::Header" << std::endl;
    std::cout << std::dec;
    std::cout << "  blocksize           = " <<      fh.blocksize          << std::endl;
    std::cout << "  sample_rate         = " <<      fh.sample_rate        << std::endl;
    std::cout << "  channels            = " << (int)fh.channels           << std::endl;
    std::cout << "  channel_assignment  = " << (int)fh.channel_assignment << std::endl;
    std::cout << "  bits_per_sample     = " << (int)fh.bits_per_sample    << std::endl;
    std::cout << "  number_type         = " << (int)fh.number_type        << std::endl;
    switch( fh.number_type )
    {
    case FLAC::Frame::NumberType::FRAME_NUMBER:
        std::cout << "  frame_number        = " << fh.number.frame_number  << std::endl;
        break;
    case FLAC::Frame::NumberType::SAMPLE_NUMBER:
        std::cout << "  sample_number       = " << fh.number.sample_number << std::endl;
        break;
    }
    std::cout << "  crc                 = " << std::hex << std::setw( 2 ) << std::setfill( '0' ) << (int)fh.crc << std::endl;
}
void PrintFrameFooter( FLAC::Frame::Footer const &ff )
{
    std::cout << "Frame::Footer" << std::endl;
    std::cout << std::dec;
    std::cout << "  crc                 = " << std::hex << std::setw( 4 ) << std::setfill( '0' ) << ff.crc << std::endl;
}
void PrintSubframeHeader( FLAC::Subframe::Header const &sfh )
{
    std::cout << "Subframe::Header" << std::endl;
    std::cout << std::dec;
    std::cout << "  type                = " << (int)sfh.type        << std::endl;
    std::cout << "  wasted_bits         = " << (int)sfh.wasted_bits << std::endl;
}
void PrintPartitionedRice( FLAC::Subframe::PartitionedRice const &rice )
{
    std::cout << "Subframe::PartitionedRice" << std::endl;
    std::cout << std::dec;
    std::cout << "  order               = " << (int)rice.order << std::endl;
    int const size = 1u << rice.order;
    std::cout << "  parameters          = [";
    for( int i = 0; i < size; ++i )
        std::cout << (i != 0 ? " " : "") << (int)rice.parameters[ i ];
    std::cout << "]" << std::endl;
    std::cout << "  is_raw_bits         = [";
    for( int i = 0; i < size; ++i )
        std::cout << (i != 0 ? " " : "") << rice.is_raw_bits[ i ];
    std::cout << "]" << std::endl;
}
void PrintResidual( FLAC::Subframe::Residual const &res, std::uint16_t const residualsize )
{
    std::cout << "Subframe::Residual" << std::endl;
    std::cout << std::dec;
    std::cout << "  type                = " << (int)res.type << std::endl;
    if( res.data.is_having< FLAC::Subframe::PartitionedRice >() )
        PrintPartitionedRice( res.data.data< FLAC::Subframe::PartitionedRice >() );
    std::cout << "  residual            = [";
    for( int i = 0; i < (int)residualsize; ++i )
        std::cout << (i != 0 ? " " : "") << res.residual[ i ];
    std::cout << "]" << std::endl;
}
void PrintSubframeConstant( FLAC::Subframe::Constant const &c, std::uint16_t const blocksize)
{
    std::cout << "Subframe::Constant" << std::endl;
    std::cout << std::dec;
    std::cout << "  value               = " << (int)c.value << std::endl;
}
void PrintSubframeFixed( FLAC::Subframe::Fixed const &f, std::uint16_t const blocksize )
{
    std::cout << "Subframe::Fixed" << std::endl;
    std::cout << std::dec;
    std::cout << "  order               = " << (int)f.order << std::endl;
    std::cout << "  warmup              = [";
    for( int i = 0; i < (int)f.order; ++i )
        std::cout << (i != 0 ? " " : "") << f.warmup[ i ];
    std::cout << "]" << std::endl;
    PrintResidual( f.residual, blocksize - f.order ); ;
}
void PrintSubframeLPC( FLAC::Subframe::LPC const &lpc, std::uint16_t const blocksize )
{
    std::cout << "Subframe::LPC" << std::endl;
    std::cout << std::dec;
    std::cout << "  order               = " << (int)lpc.order               << std::endl;
    std::cout << "  qlp_coeff_precision = " << (int)lpc.qlp_coeff_precision << std::endl;
    std::cout << "  quantization_level  = " << (int)lpc.quantization_level  << std::endl;
    std::cout << "  qlp_coeff           = [";
    for( int i = 0; i < (int)lpc.order; ++i )
        std::cout << (i != 0 ? " " : "") << lpc.qlp_coeff[ i ];
    std::cout << "]" << std::endl;
    std::cout << "  warmup              = [";
    for( int i = 0; i < (int)lpc.order; ++i )
        std::cout << (i != 0 ? " ": "") << lpc.warmup[ i ];
    std::cout << "]" << std::endl;
    PrintResidual( lpc.residual, blocksize - lpc.order );
}
void PrintSubframeVerbatim( FLAC::Subframe::Verbatim const &v, std::uint16_t const blocksize )
{
    std::cout << "Subframe::Verbatim" << std::endl;
    std::cout << std::dec;
    std::cout << "  data                = [";
    for( int i = 0; i < (int)blocksize; ++i )
        std::cout << (i != 0 ? " " : "") << v.data[ i ];
    std::cout << "]" << std::endl;
}
void PrintSubframe( FLAC::Subframe::Subframe const &sf, std::uint16_t const blocksize )
{
    PrintSubframeHeader( sf.header );
    if( sf.data.is_having< FLAC::Subframe::Constant >() )
        PrintSubframeConstant( sf.data.data< FLAC::Subframe::Constant >(), blocksize );
    else if( sf.data.is_having< FLAC::Subframe::Fixed >() )
        PrintSubframeFixed( sf.data.data< FLAC::Subframe::Fixed >(), blocksize );
    else if( sf.data.is_having< FLAC::Subframe::LPC >() )
        PrintSubframeLPC( sf.data.data< FLAC::Subframe::LPC >(), blocksize );
    else if( sf.data.is_having< FLAC::Subframe::Verbatim >() )
        PrintSubframeVerbatim( sf.data.data< FLAC::Subframe::Verbatim >(), blocksize );
}
void PrintFrame( FLAC::Frame::Frame const &f )
{
    PrintFrameHeader( f.header );
    for( int i = 0; i < (int)f.header.channels; ++i )
        PrintSubframe( f.subframes[ i ], f.header.blocksize );
    PrintFrameFooter( f.footer );
}

} // namespace FLAC
