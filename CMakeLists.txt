cmake_minimum_required( VERSION 2.8.0 )
project( anim1b )

set( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake-modules )

find_package( FreeImage )
find_package( Argp )

add_executable( anim1b cmdline.c main.c output.c )

target_compile_options( anim1b PUBLIC -Wall -Wextra -Werror )
target_include_directories( anim1b PUBLIC ${FREEIMAGE_INCLUDE_DIRS} ${ARGP_INCLUDE_DIRS} )
target_link_libraries( anim1b ${FREEIMAGE_LIBRARIES} ${ARGP_LIBRARIES} )
