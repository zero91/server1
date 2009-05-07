#ifndef BASICTYPES_HPP_
#define BASICTYPES_HPP_
typedef signed char         schar;
typedef signed char         int8;
typedef short               int16;
typedef int                 int32;
#ifdef COMPILER_MSVC
typedef __int64             int64;
#else
typedef long long           int64;
#endif /* COMPILER_MSVC */
typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
#ifdef COMPILER_MSVC
typedef unsigned __int64   uint64;
#else
typedef unsigned long long uint64;
#endif /* COMPILER_MSVC */

#endif  // BASICTYPES_HPP_


