#ifndef DECODE_HPP
#define DECODE_HPP

#include <cstdint>
#include <memory>
#include "flac_struct.hpp"

namespace FLAC
{

std::unique_ptr< std::int64_t[] > DecodeConstant( FLAC::Subframe::Constant const &c, std::uint16_t blocksize );
std::unique_ptr< std::int64_t[] > DecodeFixed   ( FLAC::Subframe::Fixed const &f, std::uint16_t blocksize );
std::unique_ptr< std::int64_t[] > DecodeLPC     ( FLAC::Subframe::LPC const &lpc, std::uint16_t blocksize );
std::unique_ptr< std::int64_t[] > DecodeVerbatim( FLAC::Subframe::Verbatim const &v, std::uint16_t blocksize );

void DecodeConstant( std::int64_t *buff, FLAC::Subframe::Constant const &c, std::uint16_t blocksize ) noexcept;
void DecodeFixed   ( std::int64_t *buff, FLAC::Subframe::Fixed const &f, std::uint16_t blocksize ) noexcept;
void DecodeLPC     ( std::int64_t *buff, FLAC::Subframe::LPC const &lpc, std::uint16_t blocksize ) noexcept;
void DecodeVerbatim( std::int64_t *buff, FLAC::Subframe::Verbatim const &v, std::uint16_t blocksize ) noexcept;

} // namespace FLAC

#endif // DECODE_HPP
