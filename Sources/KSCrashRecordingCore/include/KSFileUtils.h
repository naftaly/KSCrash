//
//  KSFileUtils.h
//
//  Created by Karl Stenerud on 2012-01-28.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
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

/* Basic file reading/writing functions.
 */

#ifndef HDR_KSFileUtils_h
#define HDR_KSFileUtils_h

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "KSCrashNamespace.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KSFU_MAX_PATH_LENGTH 500

/** Get the last entry in a file path. Assumes UNIX style separators.
 *
 * @param path The file path.
 *
 * @return the last entry in the path.
 */
const char *ksfu_lastPathEntry(const char *path);

/** Write bytes to a file descriptor.
 *
 * @param fd The file descriptor.
 *
 * @param bytes Buffer containing the bytes.
 *
 * @param length The number of bytes to write.
 *
 * @return true if the operation was successful.
 */
bool ksfu_writeBytesToFD(const int fd, const char *bytes, int length);

/** Read bytes from a file descriptor.
 *
 * @param fd The file descriptor.
 *
 * @param bytes Buffer to store the bytes in.
 *
 * @param length The number of bytes to read.
 *
 * @return true if the operation was successful.
 */
bool ksfu_readBytesFromFD(const int fd, char *bytes, int length);

/** Read an entire file. Returns a buffer of file size + 1, null terminated.
 *
 * @param path The path to the file.
 *
 * @param data Place to store a pointer to the loaded data (must be freed).
 *
 * @param length Place to store the length of the loaded data (can be NULL).
 *
 * @param maxLength the maximum amount of bytes to read. It will skip beginning
 *                  bytes if necessary, and always get the latter part of the file.
 *                  0 = no maximum.
 *
 * @return true if the operation was successful.
 */
bool ksfu_readEntireFile(const char *path, char **data, int *length, int maxLength);

/** Write a string to a file.
 *
 * @param fd The file descriptor.
 *
 * @param string The string to write.
 *
 * @return true if successful.
 */
bool ksfu_writeStringToFD(const int fd, const char *string);

/** Write a formatted string to a file.
 *
 * @param fd The file descriptor.
 *
 * @param fmt The format specifier, followed by its arguments.
 *
 * @return true if successful.
 */
bool ksfu_writeFmtToFD(const int fd, const char *fmt, ...);

/** Write a formatted string to a file.
 *
 * @param fd The file descriptor.
 *
 * @param fmt The format specifier.
 *
 * @param args The arguments list.
 *
 * @return true if successful.
 */
bool ksfu_writeFmtArgsToFD(const int fd, const char *fmt, va_list args);

/** Read a single line from a file.
 *
 * @param fd The file descriptor.
 *
 * @param buffer The buffer to read into.
 *
 * @param maxLength The maximum length to read.
 *
 * @return The number of bytes read.
 */
int ksfu_readLineFromFD(const int fd, char *buffer, int maxLength);

/** Make all directories in a path.
 *
 * @param absolutePath The full, absolute path to create.
 *
 * @return true if successful.
 */
bool ksfu_makePath(const char *absolutePath);

/** Remove a file or directory.
 *
 * @param path Path to the file to remove.
 *
 * @param mustExist If true, and the path doesn't exist, log an error.
 *
 * @return true if successful.
 */
bool ksfu_removeFile(const char *path, bool mustExist);

/** Delete the contents of a directory.
 *
 * @param path The path of the directory whose contents to delete.
 *
 * @return true if successful.
 */
bool ksfu_deleteContentsOfPath(const char *path);

/** Buffered writer structure. Everything inside should be considered internal use only. */
typedef struct {
    char *buffer;
    int bufferLength;
    int position;
    int fd;
} KSBufferedWriter;

/** Open a file for buffered writing.
 *
 * @param writer The writer to initialize.
 *
 * @param path The path of the file to open.
 *
 * @param writeBuffer Memory to use as the write buffer.
 *
 * @param writeBufferLength Length of the memory to use as the write buffer.
 *
 * @return True if the file was successfully opened.
 */
bool ksfu_openBufferedWriter(KSBufferedWriter *writer, const char *const path, char *writeBuffer,
                             int writeBufferLength);

/** Close a buffered writer.
 *
 * @param writer The writer to close.
 */
void ksfu_closeBufferedWriter(KSBufferedWriter *writer);

/** Write to a buffered writer.
 *
 * @param writer The writer to write to.
 *
 * @param data The data to write.
 *
 * @param length The length of the data to write.
 *
 * @return True if the data was successfully written.
 */
bool ksfu_writeBufferedWriter(KSBufferedWriter *writer, const char *restrict const data, const int length);

/** Flush a buffered writer, writing all uncommitted data to disk.
 *
 * @param writer The writer to flush.
 *
 * @return True if the buffer was successfully flushed.
 */
bool ksfu_flushBufferedWriter(KSBufferedWriter *writer);

/** Buffered reader structure. Everything inside should be considered internal use only. */
typedef struct {
    char *buffer;
    int bufferLength;
    int dataStartPos;
    int dataEndPos;
    int fd;
} KSBufferedReader;

/** Open a file for buffered reading.
 *
 * @param reader The reader to initialize.
 *
 * @param path The path to the file to open.
 *
 * @param readBuffer The memory to use for buffered reading.
 *
 * @param readBufferLength The length of the memory to use for buffered reading.
 *
 * @return True if the file was successfully opened.
 */
bool ksfu_openBufferedReader(KSBufferedReader *reader, const char *const path, char *readBuffer, int readBufferLength);

/** Close a buffered reader.
 *
 * @param reader The reader to close.
 */
void ksfu_closeBufferedReader(KSBufferedReader *reader);

/** Read from a buffered reader.
 *
 * @param reader The reader to read from.
 *
 * @param dstBuffer The buffer to read into.
 *
 * @param byteCount The number of bytes to read.
 *
 * @return The number of bytes actually read.
 */
int ksfu_readBufferedReader(KSBufferedReader *reader, char *dstBuffer, int byteCount);

/** Read from a buffered reader until the specified character is encountered.
 * All bytes up to and including the character will be read.
 *
 * @param reader The reader to read from.
 *
 * @param ch The character to look for.
 *
 * @param dstBuffer The buffer to read into.
 *
 * @param length in: The maximum number of bytes to read before giving up the search.
 *              out: The actual number of bytes read.
 *
 * @return True if the character was found before giving up.
 */
bool ksfu_readBufferedReaderUntilChar(KSBufferedReader *reader, int ch, char *dstBuffer, int *length);

/** Memory maps an entire file of size and returns the mapped pointer.
 *
 * @param path The path to the file.
 *
 * @param size The size of the map.
 *
 * @return the mapped pointer if successful, NULL otherwise.
 * The return value must be unmapped using `munmap` when done
 * with the returned pointer. It is ok to let the pointer live up to termination,
 * the system will unmap on termination if required.
 */
void *ksfu_mmap(const char *path, size_t size);

#ifdef __cplusplus
}
#endif

#endif  // HDR_KSFileUtils_h
