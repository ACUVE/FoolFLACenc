#ifndef UTILITY_HPP
#define UTILITY_HPP

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

#endif // UTILITY_HPP
