#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <tuple>
#include <vector>

[[noreturn]]
inline
bool fatal_impl( void )
{
    std::cerr << std::endl;
    std::exit( -1 );
}

template< typename T, typename... Args >
[[noreturn]]
inline
bool fatal_impl( T &&t,  Args &&... args )
{
    std::cerr << std::forward< T >( t );
    fatal_impl( std::forward< Args >( args )... );
}

template< typename... Args >
[[noreturn]]
inline
bool fatal( Args &&... args )
{
    std::cerr << "fatal: ";
    fatal_impl( std::forward< Args >( args )... );
}

std::tuple< std::unique_ptr< std::uint8_t[] >, std::size_t > read_file( char const *filename ) noexcept;

#endif // UTILITY_HPP
