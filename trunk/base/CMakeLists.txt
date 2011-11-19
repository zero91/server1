SET(base_srcs
	lookup3.c
	lookup8.c
	stringprintf.cpp
	)

add_library(server1_base ${base_srcs})

#StaticLibrary('base',
#  ['base.hpp',
#   'buffer.hpp',
#   'closure.hpp',
#   'lookup3.c',
#   'lookup8.c',
#   'stringprintf.hpp',
#   'stringprintf.cpp'])
#Test(env, 'stringprintf')
#Test(env, 'release_proxy')
