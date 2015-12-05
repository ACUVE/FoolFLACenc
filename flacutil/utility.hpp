#ifndef UTILITY_HPP
#define UTILITY_HPP

namespace utility
{

constexpr std::uint8_t bitcount( std::uint64_t num ) noexcept
{
    num = (num & 0x5555555555555555) + (num >>  1 & 0x5555555555555555);
    num = (num & 0x3333333333333333) + (num >>  2 & 0x3333333333333333);
    num = (num & 0x0f0f0f0f0f0f0f0f) + (num >>  4 & 0x0f0f0f0f0f0f0f0f);
    num = (num & 0x00ff00ff00ff00ff) + (num >>  8 & 0x00ff00ff00ff00ff);
    num = (num & 0x0000ffff0000ffff) + (num >> 16 & 0x0000ffff0000ffff);
    num = (num & 0x00000000ffffffff) + (num >> 32 & 0x00000000ffffffff);
    return static_cast< std::uint8_t >( num );
}
constexpr std::uint8_t clz( std::uint64_t num ) noexcept
{
    num |= num >>  1;
    num |= num >>  2;
    num |= num >>  4;
    num |= num >>  8;
    num |= num >> 16;
    num |= num >> 32;
    return bitcount( ~num );
}

} // namespace utility

#endif // UTILITY_HPP
