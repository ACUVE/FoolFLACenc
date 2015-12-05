#ifndef BITSTREAM_HPP
#define BITSTREAM_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace buffer{

namespace detail
{
    template< std::size_t BIT >
    using bit_calc_type_t = std::enable_if_t< 1 <= BIT && BIT <= 64, std::conditional_t< BIT <= 8, std::uint8_t, std::conditional_t< BIT <= 16, std::uint16_t, std::conditional_t< BIT <= 32, std::uint32_t, std::uint64_t > > > >;
    template< std::size_t BIT >
    using bit_calc_type_int_t = std::enable_if_t< 1 <= BIT && BIT <= 64, std::conditional_t< BIT <= 8, std::int8_t, std::conditional_t< BIT <= 16, std::int16_t, std::conditional_t< BIT <= 32, std::int32_t, std::int64_t > > > >;
    constexpr std::size_t BITS_IN_BYTE = 8;
} // namespace detail

class exception : public std::exception
{
private:
    char const *w;

public:
    exception( char const *w ) noexcept
        : w( w )
    {
    }
    virtual char const *what( void ) const noexcept
    {
        return w;
    }
};

class buffer
{
private:
    std::unique_ptr< std::uint8_t[] > buff;
    std::size_t                       size;

public:
    buffer() noexcept
        : buff( nullptr )
        , size( 0 )
    {
    }
    buffer( std::unique_ptr< std::uint8_t[] > buff, std::size_t const size ) noexcept
        : buff( std::move( buff ) )
        , size( size )
    {
    }
    buffer( buffer const & ) = delete;
    buffer( buffer && ) noexcept = default;
    buffer &operator=( buffer const & ) = delete;
    buffer &operator=( buffer && ) noexcept = default;
    std::size_t get_size() const noexcept
    {
        return size;
    }
    void reserve( std::size_t const rsize )
    {
        if( rsize > size )
        {
            auto tmp = std::make_unique< std::uint8_t[] >( rsize );
            if( buff )
                std::memcpy( tmp.get(), buff.get(), size );
            std::memset( tmp.get() + size, 0, rsize - size );
            std::swap( buff, tmp );
            size = rsize;
        }
    }
    std::uint8_t &operator[]( std::size_t const index ) noexcept
    {
        return buff[ index ];
    }
    std::uint8_t const &operator[]( std::size_t const index ) const noexcept
    {
        return buff[ index ];
    }
    std::uint8_t const *get() const noexcept
    {
        return buff.get();
    }
};

template< typename... >
class bytestream;

template<>
class bytestream<> : private buffer
{
private:
    std::size_t pos = 0;
    
public:
    bytestream( void ) noexcept
        : buffer()
    {
    }
    bytestream( buffer buff ) noexcept
        : buffer( std::move( buff ) )
    {
    }
    bytestream( bytestream const & ) noexcept = delete;
    bytestream( bytestream && ) = default;
    bytestream &operator=( bytestream const & ) noexcept = delete;
    bytestream &operator=( bytestream && ) = default;
    std::uint8_t get_byte( void )
    {
        if( !is_available() )
            throw exception( "get_byte: out of range" );
        return (*this)[ pos++ ];
    }
    std::unique_ptr< std::uint8_t[] > get_bytes( std::size_t const size )
    {
        if( !is_available( size ) )
            throw exception( "get_bytes: out of range");
        auto buff = std::make_unique< std::uint8_t[] >( size );
        for( std::size_t i = 0; i < size; ++i )
            buff[ i ] = (*this)[ pos++ ];
        return std::move( buff );
    }
    void put_byte( std::uint8_t const data )
    {
        put_bytes( &data, 1 );
    }
    void put_bytes( std::uint8_t const *data, std::size_t const size )
    {
        reserve( pos + size );
        for( std::size_t i = 0; i < size; ++i )
            (*this)[ pos++ ] = data[ i ];
    }
    void set_position( std::size_t const spos ) noexcept
    {
        pos = spos;
    }
    std::size_t get_position( void ) const noexcept
    {
        return pos;
    }
    void reserve( std::size_t const size )
    {
        std::size_t rsize = buffer::get_size();
        if( rsize == 0 )
            rsize = 1;
        while( size > rsize )
            rsize *= 2;
        buffer::reserve( rsize );
    }
    bool is_available( std::size_t const size = 1 ) const noexcept
    {
        if( buffer::get_size() > pos + size - 1 )
            return true;
        return false;
    }
    template< typename AHASH >
    bytestream< AHASH > add_hash( void ) noexcept
    {
        return std::move( bytestream< AHASH >( *this ) );
    }
    std::uint8_t const *data() const noexcept
    {
        return buffer::get();
    }
};
template< typename Hash, typename... Hashes >
class bytestream< Hash, Hashes... >
{
    template< typename... >
    friend class bytestream;
    
private:
    Hash                     hash;
    bytestream< Hashes... > &under;
    
    bytestream( bytestream< Hashes... > &under ) noexcept
        : hash()
        , under( under )
    {
    }
    
public:
    bytestream( bytestream const & ) = delete;
    bytestream( bytestream && ) = default;
    bytestream &operator=( bytestream const & ) = delete;
    bytestream &operator=( bytestream && ) = default;
    std::uint8_t get_byte()
    {
        std::uint8_t ret = under.get_byte();
        hash.update( ret );
        return ret;
    }
    std::unique_ptr< std::uint8_t[] > get_bytes( std::size_t const size )
    {
        auto buff = under.get_bytes( size );
        for( std::size_t i = 0; i < size; ++i )
            hash.update( buff[ i ] );
        return std::move( buff );
    }
    void put_byte( std::uint8_t const data )
    {
        under.put_byte( data );
        hash.update( data );
    }
    void put_bytes( std::uint8_t const *data, std::size_t const size )
    {
        under.put_bytes( data, size );
        for( std::size_t i = 0; i < size; ++i )
            hash.update( data[ i ] );
    }
    void set_position( std::size_t const spos ) noexcept
    {
        under.set_position( spos );
    }
    std::size_t get_position( void ) const noexcept
    {
        return under.get_position();
    }
    void reserve( std::size_t const size )
    {
        under.reserve( size );
    }
    bool is_available( std::size_t size = 1 ) const noexcept
    {
        return under.is_available( size );
    }
    template< typename AHASH >
    bytestream< AHASH, Hash, Hashes... > add_hash( void ) noexcept
    {
        return std::move( bytestream< AHASH, Hash, Hashes... >( *this ) );
    }
    std::uint8_t const *data() const noexcept
    {
        return under.data();
    }
    Hash const &get_hash() const noexcept
    {
        return hash;
    }
};

template< typename AHASH, typename... Hashes >
bytestream< AHASH, Hashes... > make_hash_bytestream( bytestream< Hashes... > &bs )
{
    return bs.template add_hash< AHASH >();
}

template< typename ByteStream >
class bitstream
{
private:
    ByteStream  &bs;
    std::uint8_t cache  = 0;
    unsigned int bitpos = 0;
    
protected:
    static
    std::uint64_t mask_num( std::uint64_t const num, std::uint8_t bit ) noexcept
    {
        assert( 1 <= bit && bit <= 64 );
        if( bit == 64 )
            return num;
        return  num & ((static_cast< std::uint64_t >( 1 ) << bit) - 1);
    }

public:
    bitstream( ByteStream &bs ) noexcept
        : bs( bs )
    {
    }
    bitstream( bitstream const & ) noexcept = default;
    bitstream( bitstream && ) noexcept = default;
    bitstream &operator=( bitstream const & ) noexcept = default;
    bitstream &operator=( bitstream && ) noexcept = default;
    std::uint64_t get( std::uint8_t const bit )
    {
        assert( 1 <= bit && bit <= 64 );
        std::uint64_t num = 0;
        std::size_t rest = bit;
        if( bitpos )
        {
            std::size_t const readable = detail::BITS_IN_BYTE - bitpos;
            if( bit <= readable )
            {
                num = ((cache << bitpos) & 0xff) >> (detail::BITS_IN_BYTE - bit);
                bitpos += bit;
                if(bitpos >= detail::BITS_IN_BYTE) bitpos -= detail::BITS_IN_BYTE;
                return num;
            }
            num = ((cache << bitpos) & 0xff) >> bitpos;
            rest -= readable;
            bitpos = 0;
        }
        while( rest >= detail::BITS_IN_BYTE )
        {
            num <<= detail::BITS_IN_BYTE;
            num |= bs.get_byte();
            rest -= detail::BITS_IN_BYTE;
        }
        if( rest )
        {
            num <<= rest;
            cache = bs.get_byte();
            num |= cache >> (detail::BITS_IN_BYTE - rest);
            bitpos = rest;
        }
        return num;
    }
    void put( std::uint64_t num, std::uint8_t const bit )
    {
        assert( 1 <= bit && bit <= 64 );
        num = mask_num( num, bit );
        std::size_t rest = bit;
        if( bitpos )
        {
            std::size_t const free = detail::BITS_IN_BYTE - bitpos;
            if( bit < free )
            {
                cache |= (num << (free - bit)) & 0xff;
                bitpos += bit;
                return;
            }
            cache |= (num >> (bit - free)) & 0xff;
            bs.put_byte( cache );
            rest -= free;
            bitpos = 0;
        }
        while( rest >= detail::BITS_IN_BYTE )
        {
            std::uint8_t const val = (num >> (rest -= detail::BITS_IN_BYTE)) & 0xff;
            bs.put_byte( val );
        }
        if( rest )
        {
            cache = (num << (detail::BITS_IN_BYTE - rest)) & 0xff;
            bitpos = rest;
        }
    }
    bool is_available( std::uint8_t const bit ) const noexcept
    {
        if( bitpos )
        {
            if( bit + bitpos <= detail::BITS_IN_BYTE )
                return false;
            return bs.is_available( (bit + bitpos - 1) / detail::BITS_IN_BYTE );
        }
        return bs.is_available( (bit + detail::BITS_IN_BYTE - 1) / detail::BITS_IN_BYTE );
    }
    std::tuple< std::size_t, unsigned int > get_position( void ) const noexcept
    {
        return std::make_tuple( bs.get_position(), bitpos );
    }
    void set_position( std::tuple< std::size_t, unsigned int > const pos ) noexcept
    {
        bs.set_position( std::get< 0 >( pos ) );
        bitpos = std::get< 1 >( pos );
    }
    void reserve( std::size_t const size )
    {
        bs.reserve( size );
    }
    ByteStream &get_bytestream( void ) noexcept
    {
        return bs;
    }
    bitstream &get_bitstream( void ) noexcept
    {
        return *this;
    }
};

template< typename ByteStream >
auto make_bitstream( ByteStream &bs ) noexcept
{
    return bitstream< ByteStream >( bs );
}

template< typename BitStream >
class useful_bitstream 
{
private:
    BitStream &bs;
    
private:
    static
    std::int64_t uint2int( std::uint64_t const num ) noexcept
    {
        if( num & 1 )
            return -static_cast< std::int64_t >( (num + 1) >> 1 );
        return static_cast< std::int64_t >( num >> 1 );
    }
    static
    std::uint64_t int2uint( std::int64_t const num ) noexcept
    {
        if( num >= 0 )
            return static_cast< std::uint64_t >( num ) << 1;
        return (static_cast< std::uint64_t >( -num ) << 1) - 1;
    }

public:
    useful_bitstream( BitStream &bs ) noexcept
        : bs( bs )
    {
    }
    useful_bitstream( useful_bitstream const & ) = default;
    useful_bitstream( useful_bitstream && ) = default;
    useful_bitstream &operator=( useful_bitstream const & ) = default;
    useful_bitstream &operator=( useful_bitstream && ) = default;
    
    std::uint64_t get( std::uint8_t const bit )
    {
        return bs.get( bit );
    }
    std::int64_t get_int( std::uint8_t const bit )
    {
        assert( 1 <= bit && bit <= 64 );
        std::uint64_t const num = get( bit );
        std::uint64_t const sign_mask = static_cast< std::uint64_t >( 1 ) << (bit - 1);
        std::uint64_t const value_mask = sign_mask - 1;
        if( num & sign_mask )
            return -static_cast< std::int64_t >( sign_mask - (num & value_mask) );
        return static_cast< std::int64_t >( num );
    }
    void put( std::uint64_t const value, std::uint8_t const bit )
    {
        bs.put( value, bit );
    }
    void put_int( std::int64_t const value, std::uint8_t const bit )
    {
        // TODO: value が bit で表せないケースについて，要検討
        assert( 1 <= bit && bit <= 64 );
        std::uint64_t const sign_mask = static_cast< std::uint64_t >( 1 ) << (bit - 1);
        std::uint64_t const value_mask = sign_mask - 1;
        std::uint64_t num = 0;
        if( value < 0 )
        {
            num |= sign_mask;
            num |= (sign_mask - static_cast< std::uint64_t >( -value )) & value_mask;
        }
        else
            num = static_cast< std::uint64_t >( value ) & value_mask;
        put( num, bit );
    }
    std::uint64_t get_unary( void )
    {
        std::uint64_t i = 0;
        while( !get( 1 ) )
            ++i;
        return i;
    }
    std::int64_t get_unary_int( void )
    {
        return uint2int( get_unary() );
    }
    void put_unary( std::uint64_t num )
    {
        while( num-- )
            put( 0, 1 );
        put( 1, 1 );
    }
    void put_unary_int( std::int64_t const num )
    {
        put_unary( int2uint( num ) );
    }
    std::uint64_t get_rice( std::uint8_t const param )
    {
        assert( 0 <= param && param < 64 );
        std::uint64_t const q = get_unary();
        if( param == 0 )
            return q;
        std::uint64_t const r = get( param );
        return r + (q << param);
    }
    std::int64_t get_rice_int( std::uint8_t const param )
    {
        assert( 0 <= param && param < 64 );
        return uint2int( get_rice( param ) );
    }
    void put_rice( std::uint64_t const num, std::uint8_t const param )
    {
        assert( 0 <= param && param < 64 );
        std::uint64_t const q = num >> param;
        std::uint64_t const r = num & ((static_cast< std::uint64_t >( 1 ) << param) - 1);
        put_unary( q );
        if( param != 0 )
            put( r, param );
    }
    void put_rice_int( std::int64_t const num, std::uint8_t const param )
    {
        put_rice( int2uint( num ), param );
    }
    std::unique_ptr< std::uint8_t[] > get_bytes( std::size_t const size )
    {
        auto buff = std::make_unique< std::uint8_t[] >( size );
        for( std::size_t i = 0; i < size; ++i )
            buff[ i ] = get( 8 );
        return std::move( buff );
    }
    void put_bytes( std::uint8_t const *buff, std::size_t const size )
    {
        for( std::size_t i = 0; i < size; ++i )
            put( buff[ i ] , 8 );
    }
    std::uint64_t get_utf8( void )
    {
        std::uint64_t num = 0;
        std::uint8_t  len = 0;
        std::uint8_t  b   = get( 8 );
        if( (b & 0b10000000) == 0b00000000 )
        {
            len = 1;
            num = b & 0b01111111;
        }
        else if( (b & 0b11100000) == 0b11000000 )
        {
            len = 2;
            num = b & 0b00011111;
        }
        else if( (b & 0b11110000) == 0b11100000 )
        {
            len = 3;
            num = b & 0b00001111;
        }
        else if( (b & 0b11111000) == 0b11110000 )
        {
            len = 4;
            num = b & 0b00000111;
        }
        else if( (b & 0b11111100) == 0b11111000 )
        {
            len = 5;
            num = b & 0b00000011;
        }
        else if( (b & 0b11111110) == 0b11111100 )
        {
            len = 6;
            num = b & 0b00000001;
        }
        else if( (b & 0b11111111) == 0b11111110 )
        {
            len = 7;
            num = b & 0b00000000;
        }
        else
            throw exception( "get_utf8: not utf8" );
        for( std::uint8_t v = 1; v < len; ++v )
        {
            b = get( 8 );
            if( (b & 0b11000000) != 0b10000000 )
                throw exception( "get_utf8: not utf8" );
            num = (num << 6) | (b & 0b00111111);
        }
        static constexpr std::uint8_t const shift_table[] = { 7, 11, 16, 21, 26, 31 };
        if( len != 1 && (num & (0b11111ll << shift_table[ len - 2 ])) == 0 )
            throw exception( "get_utf8: not utf8" );
        return num;
    }
    void put_utf8( std::uint64_t const num )
    {
        if( num > 0xFFFFFFFFF )
            throw exception( "put_utf8: too big" );
        static constexpr std::uint64_t max_table[] = { 0x7F, 0x7FF, 0xFFFF, 0x1FFFFF, 0x3FFFFFF, 0x7FFFFFFF };
        std::size_t len = 0;
        for( ; len < sizeof( max_table ) / sizeof( max_table[ 0 ] ); ++len )
            if( num <= max_table[ len ] )
                break;
        static constexpr std::uint8_t mask_table[] = { 0x7F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00 };
        static constexpr std::uint8_t upmask_table[] = { 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
        put( upmask_table[ len ] | (mask_table[ len ] & (num >> (len * 6))) , 8 );
        for( std::size_t i = 0; i < len; ++i )
            put( 0x80 | (0x3F & (num >> ((len - i - 1) * 6))), 8 );
    }
    bool is_available( std::uint8_t bit ) const noexcept
    {
        return bs.is_available( bit );
    }
    std::tuple< std::size_t, unsigned int > get_position( void ) const noexcept
    {
        return bs.get_position();
    }
    void set_position( std::tuple< std::size_t, unsigned int > const pos ) noexcept
    {
        return bs.set_position( pos );
    }
    bool is_byte_aligned( void ) noexcept
    {
        return std::get< 1 >( get_position() ) == 0;
    }
    void skip_byte( std::size_t const byte ) noexcept
    {
        auto pos = get_position();
        std::get< 0 >( pos ) += byte;
        set_position( pos );
    }
    void skip_bit( std::size_t const bit ) noexcept
    {
        auto pos = get_position();
        std::get< 1 >( pos ) += bit & detail::BITS_IN_BYTE;
        if( std::get< 1 >( pos ) >= detail::BITS_IN_BYTE )
        {
            std::get< 0 >( pos ) += 1;
            std::get< 1 >( pos ) -= detail::BITS_IN_BYTE;
        }
        std::get< 0 >( pos ) += bit / detail::BITS_IN_BYTE;
        set_position( pos );
    }
    void reserve( std::size_t const size )
    {
        bs.reserve( size );
    }
    auto &get_bytestream( void ) noexcept
    {
        return bs.get_bytestream();
    }
    BitStream &get_bitstream( void ) noexcept
    {
        return bs;
    }
};

template< typename BitStream >
auto make_useful_bitstream( BitStream &bs ) noexcept
{
    return useful_bitstream< BitStream >( bs );
}
template< typename BitStream >
auto make_useful_bitstream( useful_bitstream< BitStream > &bs ) noexcept
{
    return useful_bitstream< BitStream >( bs.get_bitstream() );
}

} // namespace buffer

#endif // BITSTREAM_HPP
