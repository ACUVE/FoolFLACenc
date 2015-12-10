#ifndef FLACUTIL_FLAC_ENCODE_HPP
#define FLACUTIL_FLAC_ENCODE_HPP

#include <cstdint>
#include "flac_struct.hpp"

namespace FLAC
{

std::tuple< Subframe::Constant, std::uint64_t > EncodeConstant( std::int64_t const *src, std::uint8_t bps, std::uint16_t blocksize );
std::tuple< Subframe::Fixed, std::uint64_t >    EncodeFixed   ( std::int64_t const *src, std::uint8_t bps, std::uint8_t order, std::uint16_t blocksize );
// std::tuple< Subframe::LPC, std::uint64_t >      EncodeLPC     ( std::int64_t const *src, std::uint8_t bps, ..., std::uint16_t blocksize );
std::tuple< Subframe::Verbatim, std::uint64_t > EncodeVerbatim( std::int64_t const *src, std::uint8_t bps, std::uint16_t blocksize );

} // namespace FLAC

#endif // FLACUTIL_FLAC_ENCODE_HPP
