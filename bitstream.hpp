// License: GPLv3 or Later

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

class bitstream
{
private:
    template< std::size_t BIT >
    using bit_calc_type_t = 
        std::enable_if_t<
            1 <= BIT && BIT <= 64,
            std::conditional_t<
                BIT <= 8,
                std::uint8_t,
                std::conditional_t<
                    BIT <= 16,
                    std::uint16_t,
                    std::conditional_t<
                        BIT <= 32,
                        std::uint32_t,
                        std::uint64_t
                    >
                >
            >
        >;
    template< std::size_t BIT >
    using bit_calc_type_int_t =
        std::enable_if_t<
            1 <= BIT && BIT <= 64,
            std::conditional_t<
                BIT <= 8,
                std::int8_t,
                std::conditional_t<
                    BIT <= 16,
                    std::int16_t,
                    std::conditional_t<
                        BIT <= 32,
                        std::int32_t,
                        std::int64_t
                    >
                >
            >
        >;
    
    template< std::size_t BIT >
    static std::enable_if_t<
        BIT != 8 && BIT != 16 && BIT != 32 && BIT != 64,
        bit_calc_type_t< BIT >
    > mask_num( bit_calc_type_t< BIT > const num ) noexcept
    {
        return num & ((static_cast< bit_calc_type_t< BIT >>( 1 ) << BIT) - 1);
    }
    template< std::size_t BIT >
    static std::enable_if_t<
        BIT == 8 || BIT == 16 || BIT == 32 || BIT == 64,
        bit_calc_type_t< BIT >
    > mask_num( bit_calc_type_t< BIT > const num ) noexcept
    {
        return num;
    }
    
    static constexpr std::size_t BITS_IN_BYTE = 8;
    
    template< std::size_t ADD_BIT >
    std::size_t need_alloc_size( void ) const noexcept
    {
        return pos + (bitpos + ADD_BIT + (BITS_IN_BYTE - 1)) / BITS_IN_BYTE;
    }
    
    template< std::size_t BIT >
    std::enable_if_t< (BIT < BITS_IN_BYTE), bit_calc_type_t< BIT > > get_impl( void ) noexcept
    {
        using UInt = bit_calc_type_t< BIT >;
        UInt num = 0;
        std::size_t rest = BIT;
        if( bitpos )
        {
            std::size_t const readable = BITS_IN_BYTE - bitpos;
            if( BIT < readable )
            {
                num = ((p[ pos ] << bitpos) & 0xff) >> (BITS_IN_BYTE - BIT);
                bitpos += BIT;
                return num;
            }
            num = ((p[ pos++ ] << bitpos) & 0xff) >> bitpos;
            rest -= readable;
            bitpos = 0;
        }
        if( rest )
        {
            num <<= rest;
            num |= p[ pos ] >> (BITS_IN_BYTE - rest);
            bitpos = rest;
        }
        return num;
    }
    template< std::size_t BIT >
    std::enable_if_t< (BIT == BITS_IN_BYTE), bit_calc_type_t< BIT > > get_impl( void ) noexcept
    {
        using UInt = bit_calc_type_t< BIT >;
        UInt num = 0;
        if( bitpos )
        {
            std::size_t rest = BIT;
            std::size_t const readable = BITS_IN_BYTE - bitpos;
            num = ((p[ pos++ ] << bitpos) & 0xff) >> bitpos;
            rest -= readable;
            num <<= rest;
            num |= p[ pos ] >> (BITS_IN_BYTE - rest);
            bitpos = rest;
            return num;
        }
        else
        {
            return p[ pos++ ];
        }
    }
    template< std::size_t BIT >
    std::enable_if_t< (BIT > BITS_IN_BYTE), bit_calc_type_t< BIT > > get_impl( void ) noexcept
    {
        using UInt = bit_calc_type_t< BIT >;
        UInt num = 0;
        std::size_t rest = BIT;
        if( bitpos )
        {
            std::size_t const readable = BITS_IN_BYTE - bitpos;
            num = ((p[ pos++ ] << bitpos) & 0xff) >> bitpos;
            rest -= readable;
            bitpos = 0;
        }
        while( rest >= BITS_IN_BYTE )
        {
            num <<= BITS_IN_BYTE;
            num |= p[ pos++ ];
            rest -= BITS_IN_BYTE;
        }
        if( rest )
        {
            num <<= rest;
            num |= p[ pos ] >> (BITS_IN_BYTE - rest);
            bitpos = rest;
        }
        return num;
    }
    
    // need size >= alloc
    std::unique_ptr< std::uint8_t[] > copy_sized_buffer( std::size_t size ) noexcept
    {
        std::unique_ptr< std::uint8_t[] > buff;
        try{
            buff = std::make_unique< std::uint8_t[] >( size );
        }
        catch( ... )
        {
            return nullptr;
        }
        std::memcpy( buff.get(), p.get(), alloc );
        return std::move( buff );
    }
    
public:
    bitstream( void ) noexcept
    {
    }
    bitstream( std::unique_ptr< std::uint8_t[] > sp, std::size_t alloc_size ) noexcept
        : p( std::move( sp ))
        , alloc( alloc_size )
    {
    }
    bitstream( bitstream &&right ) noexcept = default;
    bitstream( bitstream const & ) = delete;
    
    bitstream &operator=( bitstream &&right ) noexcept = default;
    bitstream &operator=( bitstream const & ) = delete;
    
    ~bitstream( void ) noexcept = default;
    
    void reset( void ) noexcept
    {
        bitstream bs;
        std::swap( *this, bs );
    }
    void set_position( std::size_t spos, std::uint_fast8_t sbitpos = 0 ) noexcept
    {
        pos = spos;
        bitpos = sbitpos;
    }
    std::tuple< std::size_t, std::uint_fast8_t > get_position() const noexcept
    {
        return std::make_tuple( pos, bitpos );
    }
    std::unique_ptr< std::uint8_t[] > get_buffer( void ) & noexcept
    {
        return copy_sized_buffer( alloc );
    }
    std::unique_ptr< std::uint8_t[] > get_buffer( void ) && noexcept
    {
        return std::move( p );
    }
    void set_buffer( std::unique_ptr< std::uint8_t[] > sp, std::size_t alloc_size ) noexcept
    {
        reset();
        p = std::move( sp );
        alloc = alloc_size;
    }
    bool is_error( void ) const noexcept
    {
        return error;
    }
    void reset_error( void ) noexcept
    {
        error = false;
    }
    
    void reserve( std::size_t const size ) noexcept
    {
        if( error || alloc >= size )
        {
            return;
        }
        auto buff = copy_sized_buffer( size );
        if( !buff )
        {
            error = true;
            return;
        }
        p = std::move( buff );
        alloc = size;
    }
    template< std::size_t BIT >
    void put( bit_calc_type_t< BIT > num ) noexcept
    {
        reserve( need_alloc_size< BIT >() );
        if( error )
        {
            return;
        }
        num = mask_num< BIT >( num );
        std::size_t rest = BIT;
        if( bitpos )
        {
            std::size_t const free = BITS_IN_BYTE - bitpos;
            if( BIT < free )
            {
                p[ pos ] |= (num << (free - BIT)) & 0xff;
                bitpos += BIT;
                return;
            }
            p[ pos++ ] |= (num >> (BIT - free)) & 0xff;
            rest -= free;
            bitpos = 0;
        }
        while( rest >= BITS_IN_BYTE )
        {
            p[ pos++ ] = (num >> (rest -= BITS_IN_BYTE)) & 0xff;
        }
        if( rest )
        {
            p[ pos ] = (num << (BITS_IN_BYTE - rest)) & 0xff;
            bitpos = rest;
        }
    }
    
    template< std::size_t BIT >
    bool is_available( void ) const noexcept
    {
        return need_alloc_size< BIT >() <= alloc && !error;
    }
    template< std::size_t BIT >
    bit_calc_type_t< BIT > get( void ) noexcept
    {
        if( !is_available< BIT >() )
        {
            error = true;
            return 0;
        }
        return get_impl< BIT >();
    }
    
#define REPEAT(FUNC) FUNC( 1) FUNC( 2) FUNC( 3) FUNC( 4) FUNC( 5) FUNC( 6) FUNC( 7) FUNC( 8) FUNC( 9) FUNC(10) \
                     FUNC(11) FUNC(12) FUNC(13) FUNC(14) FUNC(15) FUNC(16) FUNC(17) FUNC(18) FUNC(19) FUNC(20) \
                     FUNC(21) FUNC(22) FUNC(23) FUNC(24) FUNC(25) FUNC(26) FUNC(27) FUNC(28) FUNC(29) FUNC(30) \
                     FUNC(31) FUNC(32) FUNC(33) FUNC(34) FUNC(35) FUNC(36) FUNC(37) FUNC(38) FUNC(39) FUNC(40) \
                     FUNC(41) FUNC(42) FUNC(43) FUNC(44) FUNC(45) FUNC(46) FUNC(47) FUNC(48) FUNC(49) FUNC(50) \
                     FUNC(51) FUNC(52) FUNC(53) FUNC(54) FUNC(55) FUNC(56) FUNC(57) FUNC(58) FUNC(59) FUNC(60) \
                     FUNC(61) FUNC(62) FUNC(63) FUNC(64)
    std::uint64_t get( std::size_t bit ) noexcept
    {
        assert( 1 <= bit && bit <= 64 );
        switch( bit )
        {
#define CASE(n) case n: return get< n >();
        REPEAT( CASE )
#undef CASE
        }
    }
    void put( std::size_t bit, std::uint64_t num ) noexcept
    {
        assert( 1 <= bit && bit <= 64 );
        switch( bit )
        {
#define CASE(n) case n: put< n >( num ); break;
        REPEAT( CASE )
#undef CASE
        }
    }
#undef REPEAT
    
    template< std::size_t BIT >
    bit_calc_type_int_t< BIT > get_int( void ) noexcept
    {
        using BIT_TYPE = bit_calc_type_t< BIT >;
        BIT_TYPE val = get< BIT >();
        constexpr BIT_TYPE SIGNMASK = static_cast< BIT_TYPE >( 1 ) << (BIT - 1);
        bool const sign = val & SIGNMASK; // true: -, false: +
        if( sign )
            return -1 * static_cast< bit_calc_type_int_t< BIT > >( SIGNMASK - (val & ~SIGNMASK) );
        else
            return val & ~SIGNMASK;
    }
#define REPEAT(FUNC) FUNC( 1) FUNC( 2) FUNC( 3) FUNC( 4) FUNC( 5) FUNC( 6) FUNC( 7) FUNC( 8) FUNC( 9) FUNC(10) \
                     FUNC(11) FUNC(12) FUNC(13) FUNC(14) FUNC(15) FUNC(16) FUNC(17) FUNC(18) FUNC(19) FUNC(20) \
                     FUNC(21) FUNC(22) FUNC(23) FUNC(24) FUNC(25) FUNC(26) FUNC(27) FUNC(28) FUNC(29) FUNC(30) \
                     FUNC(31) FUNC(32) FUNC(33) FUNC(34) FUNC(35) FUNC(36) FUNC(37) FUNC(38) FUNC(39) FUNC(40) \
                     FUNC(41) FUNC(42) FUNC(43) FUNC(44) FUNC(45) FUNC(46) FUNC(47) FUNC(48) FUNC(49) FUNC(50) \
                     FUNC(51) FUNC(52) FUNC(53) FUNC(54) FUNC(55) FUNC(56) FUNC(57) FUNC(58) FUNC(59) FUNC(60) \
                     FUNC(61) FUNC(62) FUNC(63) FUNC(64)
    std::int64_t get_int( std::size_t bit ) noexcept
    {
        assert( 1 <= bit && bit <= 64 );
        switch( bit )
        {
#define CASE(n) case n: get_int< n >(); break;
            REPEAT( CASE )
#undef CASE
        }
    }
#undef REPEAT
    
    std::uint64_t get_unary( void ) noexcept
    {
        std::uint64_t i = 0;
        while( !error )
            if( get< 1 >() )
                return i;
        return 0;
    }
    std::int64_t get_unary_int( void ) noexcept
    {
        std::uint64_t const v = get_unary();
        if( error )
            return 0;
        if( v & 1 )
            return -static_cast< std::int64_t >( (v + 1) >> 1 );
        return static_cast< std::int64_t >( v >> 1 );
    }
    void put_unary( std::uint64_t num ) noexcept
    {
        while( num-- && !error )
            put< 1 >( 0 );
        put< 1 >( 1 );
    }
    void put_unary_int( std::int64_t const num ) noexcept
    {
        if( num >= 0 )
            put_unary( static_cast< std::uint64_t >( num ) << 1 );
        else
            put_unary( (static_cast< std::uint64_t >( -num ) << 1) - 1 );
    }
    
    std::uint64_t get_rice( std::uint8_t const param ) noexcept
    {
        assert( 0 <= param && param < 64 );
        std::uint64_t const q = get_unary();
        if( error )
            return 0;
        if( param == 0 )
            return q;
        std::uint64_t const r = get( param );
        if( error )
            return 0;
        return r + (q << param);
    }
    std::int64_t get_rice_int( std::uint8_t const param ) noexcept
    {
        assert( 0 <= param && param < 64 );
        std::uint64_t const v = get_rice( param );
        if( error )
            return 0;
        if( v & 1 )
            return -static_cast< std::int64_t >( (v + 1) >> 1 );
        return static_cast< std::int64_t >( v >> 1 );
    }
    void put_rice( std::uint64_t const num, std::uint8_t param ) noexcept
    {
        assert( 0 <= param && param < 64 );
        std::uint64_t const q = num >> param;
        std::uint64_t const r = num & ((static_cast< std::uint64_t >( 1 ) << param) - 1);
        put_unary( q );
        if( param != 0 )
            put( r, param );
    }
    void put_rice_int( std::int64_t const num, std::uint8_t param ) noexcept
    {
        assert( 0 <= param && param < 64 );
        if( num >= 0 )
            put_rice( static_cast< std::uint64_t >( num ) << 1, param );
        else
            put_rice( (static_cast< std::uint64_t >( -num ) << 1) - 1, param );
    }
    
    template< std::size_t SIZE >
    std::array< std::uint8_t, SIZE > get_bytes( void ) noexcept
    {
        std::array< std::uint8_t, SIZE > buff;
        for( std::size_t i = 0; i < SIZE; ++i )
        {
            buff[ i ] = get< 8 >();
        }
        if( error )
            return {};
        return buff;
    }
    
    std::uint64_t get_utf8( void ) noexcept
    {
        std::uint64_t num = 0;
        std::uint8_t  len = 0;
        std::uint8_t  b   = get< 8 >();
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
        {
            error = true;
            return 0;
        }
        for( std::uint8_t v = 1; v < len; ++v )
        {
            b = get< 8 >();
            if( (b & 0b11000000) != 0b10000000 )
            {
                error = true;
                return 0;
            }
            num = (num << 6) | (b & 0b00111111);
        }
        static constexpr std::uint8_t const shift_table[] = { 7, 11, 16, 21, 26, 31 };
        if( len != 1 && (num & (0b11111ll << shift_table[ len - 2 ])) == 0 )
        {
            error = true;
            return 0;
        }
        if( error )
            return 0;
        return num;
    }
    
    bool is_byte_aligned( void ) noexcept
    {
        return bitpos == 0;
    }
    
    void skip_byte( std::size_t byte ) noexcept
    {
        pos += byte;
    }
    void skip_bit( std::size_t bit ) noexcept
    {
        bitpos += bit % BITS_IN_BYTE;
        if( bitpos >= BITS_IN_BYTE )
        {
            pos += 1;
            bitpos -= BITS_IN_BYTE;
        }
        pos += bit / BITS_IN_BYTE;
    }
    
    struct state
    {
        std::size_t pos;
        std::size_t bitpos;
        bool        error;
    };
    
    state get_state( void ) noexcept
    {
        return { pos, bitpos, error };
    }
    void set_state( state s ) noexcept
    {
        pos    = s.pos;
        bitpos = s.bitpos;
        error  = s.error;
    }
    
private:
    std::unique_ptr< std::uint8_t[] > p      = nullptr;
    std::size_t                       alloc  = 0;
    std::size_t                       pos    = 0;
    std::uint_fast8_t                 bitpos = 0;
    bool                              error  = false;
};

#endif // BITSTREAM_HPP
