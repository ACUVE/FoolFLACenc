#ifndef FLACUTIL_FILE_HPP
#define FLACUTIL_FILE_HPP

#include <cstdint>
#include <memory>
#include <vector>

namespace file
{

struct sound_data
{
    std::vector< std::unique_ptr< std::int64_t[] > > wave;
    std::uint8_t                                     bits_per_sample;
    std::uint64_t                                    samples;
    std::uint32_t                                    sample_rate;
};

void print_sound_data( sound_data const &sd );
sound_data decode_wavefile( char const *filnemae );

} // namespace file

#endif // FLACUTIL_FILE_HPP
