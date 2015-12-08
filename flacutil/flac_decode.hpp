#ifndef FLACUTIL_DECODE_HPP
#define FLACUTIL_DECODE_HPP

#include <cstdint>
#include <memory>
#include "flac_struct.hpp"

namespace FLAC
{

std::unique_ptr< std::int64_t[] > DecodeConstant( Subframe::Constant const &c, std::uint16_t blocksize );
std::unique_ptr< std::int64_t[] > DecodeFixed   ( Subframe::Fixed const &f, std::uint16_t blocksize );
std::unique_ptr< std::int64_t[] > DecodeLPC     ( Subframe::LPC const &lpc, std::uint16_t blocksize );
std::unique_ptr< std::int64_t[] > DecodeVerbatim( Subframe::Verbatim const &v, std::uint16_t blocksize );
std::unique_ptr< std::int64_t[] > DecodeSubframe( Subframe::Subframe const &s, std::uint16_t blocksize );

void DecodeConstant( std::int64_t *buff, Subframe::Constant const &c, std::uint16_t blocksize ) noexcept;
void DecodeFixed   ( std::int64_t *buff, Subframe::Fixed const &f, std::uint16_t blocksize ) noexcept;
void DecodeLPC     ( std::int64_t *buff, Subframe::LPC const &lpc, std::uint16_t blocksize ) noexcept;
void DecodeVerbatim( std::int64_t *buff, Subframe::Verbatim const &v, std::uint16_t blocksize ) noexcept;
void DecodeSubframe( std::int64_t *buff, Subframe::Subframe const &s, std::uint16_t blocksize ) noexcept;

} // namespace FLAC

#endif // FLACUTIL_DECODE_HPP
