// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)
//
// Printf variants that place their output in a C++ string.
//
// Usage:
//      string result = StringPrintf("%d %s\n", 10, "hello");
//      SStringPrintf(&result, "%d %s\n", 10, "hello");
//      StringAppendF(&result, "%d %s\n", 20, "there");

#ifndef _STRINGPRINTF_H
#define _STRINGPRINTF_H

#include <stdarg.h>
#include "base/base.hpp"

// Return a C++ string
extern string StringPrintf(const char* format, ...);

// Store result into a supplied string and return it
extern const string& SStringPrintf(string* dst, const char* format, ...);

// Append result to a supplied string
extern void StringAppendF(string* dst, const char* format, ...);

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines are just convenience wrappers around it.
extern void StringAppendV(string* dst, const char* format, va_list ap);

#endif /* _STRINGPRINTF_H */
