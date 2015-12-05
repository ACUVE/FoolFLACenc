#ifndef FLACUTIL_VARIANT_HPP
#define FLACUTIL_VARIANT_HPP

#include <limits>
#include <tuple>
#include <type_traits>

template<typename... Types>
class variant
{
public:
    static constexpr std::size_t type_size = sizeof...( Types );
    static constexpr std::size_t INVALID_WHICH = std::numeric_limits< std::size_t >::max();
    
private:
    std::aligned_union_t< 0, Types... > storage;
    std::size_t                         which_have = INVALID_WHICH;
    
    template< typename T >
    using remove_cv_ref_t = std::remove_cv_t< std::remove_reference_t< T > >;
    
    template< std::size_t I, typename T, typename Tuple >
    struct type_to_which_impl;
    template< std::size_t I, typename T, typename... Tail >
    struct type_to_which_impl< I, T, std::tuple< T, Tail...> >
    {
        static constexpr std::size_t value = I;
        static constexpr bool have = true;
    };
    template< std::size_t I, typename T, typename Head, typename... Tail >
    struct type_to_which_impl< I, T, std::tuple< Head, Tail... > >
        : type_to_which_impl< I + 1, T, std::tuple< Tail... > >
    {
    };
    template < std::size_t I, typename T >
    struct type_to_which_impl< I, T, std::tuple<> >
    {
        static constexpr bool have = false;
    };
    
    template< std::size_t I, typename Func, typename... Args >
    std::enable_if_t< I < type_size > apply_impl( Func &&func, Args &&... args )
    {
        if( which_have == I )
            func( data< I >(), std::forward< Args >( args )... );
        apply_impl< I + 1 >( std::forward< Func >( func ), std::forward< Args >( args )... );
    }
    template< std::size_t I, typename Func, typename... Args >
    std::enable_if_t< I < type_size > apply_impl( Func &&func, Args &&... args ) const
    {
        if( which_have == I )
            func( data< I >(), std::forward< Args >( args )... );
        apply_impl< I + 1 >( std::forward< Func >( func ), std::forward< Args >( args )... );
    }
    template< std::size_t I, typename Func, typename... Args >
    std::enable_if_t< I >= type_size > apply_impl( Func &&func, Args &&... args ) const
    {
    }
    
    struct Destruct
    {
        template< typename T >
        void operator()( T &t )
        {
            t.~T();
        }
    };
    
public:
    template< std::size_t I >
    using data_type = std::tuple_element_t< I , std::tuple< Types... > >;
    template< typename T >
    static constexpr std::size_t type_to_which = type_to_which_impl< 0, T, std::tuple< Types... > >::value;
    template< typename T >
    static constexpr bool have_type = type_to_which_impl< 0, T, std::tuple< Types... > >::have;
    
    template< std::size_t I >
    data_type< I > &data( void ) noexcept
    {
        return *reinterpret_cast< data_type< I > * >( &storage );
    }
    template< std::size_t I >
    data_type< I > const &data( void ) const noexcept
    {
        return *reinterpret_cast< data_type< I > const * >( &storage );
    }
    template< typename T >
    T &data( void ) noexcept
    {
        return data< type_to_which< T > >();
    }
    template< typename T >
    T const &data( void ) const noexcept
    {
        return data< type_to_which< T > >();
    }

    std::size_t which( void ) const noexcept
    {
        return which_have;
    }
    template< typename T >
    bool is_having( void ) const noexcept
    {
        return which_have == type_to_which< T >;
    }
    
    template< typename Func, typename... Args >
    void apply( Func &&func, Args &&... args )
    {
        apply_impl< 0 >( std::forward< Func >( func ), std::forward< Args >( args )... );
    }
    template< typename Func, typename... Args >
    void apply( Func &&func, Args &&... args ) const
    {
        apply_impl< 0 >( std::forward< Func >( func ), std::forward< Args >( args )... );
    }
    
    variant() noexcept
    {
    }
    template< typename T, typename dummy = std::enable_if_t< have_type< T > > >
    explicit variant( T &&x )
    {
        using TT = remove_cv_ref_t< T >;
        construct< TT >( std::forward< T >( x ) );
    }
    variant( variant const &right )
    {
        *this = right;
    }
    variant( variant &&right )
    {
        *this = std::move(right);
    }
    template< typename T, typename... Args >
    void construct( Args &&... args )
    {
        new( &data< T >() ) T( std::forward< Args >( args )... );
        which_have = type_to_which< T >;
    }
    
    ~variant()
    {
        destruct();
    }
    void destruct( void )
    {
        apply( Destruct{} );
        which_have = INVALID_WHICH;
    }

    template< typename T >
    std::enable_if_t< have_type< remove_cv_ref_t < T > >, variant > &operator=( T &&right )
    {
        using TT = remove_cv_ref_t< T >;
        if( which_have != type_to_which< remove_cv_ref_t< T > > )
        {
            destruct();
            construct< TT >( std::forward< T >( right ) );
        }else{
            data< TT >() = std::forward< T >( right );
        }
        return *this;
    }
    variant &operator=( variant const &right )
    {
        if( right.which_have == INVALID_WHICH )
        {
            destruct();
        }
        else
        {
            right.apply(
                []( auto const &right, variant *left )
                {
                    *left = right;
                }
            , this);
        }
        return *this;
    }
    variant &operator=( variant &&right )
    {
        if( right.which_have == INVALID_WHICH )
        {
            destruct();
        }
        else
        {
            right.apply(
                []( auto &right, variant *left )
                {
                    *left = std::move( right );
                }
            , this);
        }
        return *this;
    }
    
    bool vaild( void ) const noexcept
    {
        return which_have != INVALID_WHICH;
    }
    bool invalid( void ) const noexcept
    {
        return !vaild();
    }
};

#endif // FLACUTIL_VARIANT_HPP
