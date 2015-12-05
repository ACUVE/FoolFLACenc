#ifndef HASH_HPP
#define HASH_HPP

#include <cstddef>
#include <cstdint>

namespace hash{

void crc8_update( std::uint8_t &crc, std::uint8_t data ) noexcept;
void crc8_update( std::uint8_t &crc, std::uint8_t const *data, std::size_t len ) noexcept;
std::uint8_t crc8( std::uint8_t const *data, std::size_t len ) noexcept;

void crc16_update( std::uint16_t &crc, std::uint8_t data ) noexcept;
void crc16_update( std::uint16_t &crc, std::uint8_t const *data, std::size_t len ) noexcept;
std::uint16_t crc16( std::uint8_t const *data, std::size_t len ) noexcept;

class crc8_hash
{
private:
    std::uint8_t crc = 0;
    
public:
    void update( std::uint8_t const val ) noexcept
    {
        crc8_update( crc, val );
    }
    std::uint8_t get() const noexcept
    {
        return crc;
    }
};

class crc16_hash
{
private:
    std::uint16_t crc = 0;
    
public:
    void update( std::uint8_t const val ) noexcept
    {
        crc16_update( crc, val );
    }
    std::uint16_t get() const noexcept
    {
        return crc;
    }
};

} // namespace hash

#endif // HASH_HPP
