//
//  KSDate.c
//
// Copyright 2016 Karl Stenerud.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "KSDate.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

void ksdate_utcStringFromTimestamp(time_t timestamp, char *buffer21Chars)
{
    struct tm result = { 0 };
    gmtime_r(&timestamp, &result);
    snprintf(buffer21Chars, 21, "%04d-%02d-%02dT%02d:%02d:%02dZ", result.tm_year + 1900, result.tm_mon + 1,
             result.tm_mday, result.tm_hour, result.tm_min, result.tm_sec);
}

void ksdate_utcStringFromMicroseconds(int64_t microseconds, char *buffer28Chars)
{
    struct tm result = { 0 };
    time_t curtime = (time_t)(microseconds / 1000000);
    long micros = (long)(microseconds % 1000000);

    gmtime_r(&curtime, &result);
    snprintf(buffer28Chars, 28, "%04d-%02d-%02dT%02d:%02d:%02d.%06ldZ", result.tm_year + 1900, result.tm_mon + 1,
             result.tm_mday, result.tm_hour, result.tm_min, result.tm_sec, micros);
}

uint64_t ksdate_microseconds(void)
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    uint64_t microseconds = ((uint64_t)tp.tv_sec) * 1000000 + (uint64_t)tp.tv_usec;
    return microseconds;
}
