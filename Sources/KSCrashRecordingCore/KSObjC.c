//
//  KSObjC.c
//
//  Created by Karl Stenerud on 2012-08-30.
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

#include "KSObjC.h"

#include "KSLogger.h"
#include "KSMemory.h"
#include "KSObjCApple.h"
#include "KSString.h"

#if __IPHONE_OS_VERSION_MAX_ALLOWED > 70000
#include <objc/NSObjCRuntime.h>
#else
#if __LP64__ || (TARGET_OS_EMBEDDED && !TARGET_OS_IPHONE) || TARGET_OS_WIN32 || NS_BUILD_32_LIKE_64
typedef long NSInteger;
typedef unsigned long NSUInteger;
#else
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif
#endif
#include <CoreFoundation/CFBase.h>
#include <CoreGraphics/CGBase.h>
#include <inttypes.h>
#include <objc/runtime.h>

#include "TargetConditionals.h"

#define kMaxNameLength 128

//======================================================================
#pragma mark - Macros -
//======================================================================

// Compiler hints for "if" statements
#define likely_if(x) if (__builtin_expect(x, 1))
#define unlikely_if(x) if (__builtin_expect(x, 0))

//======================================================================
#pragma mark - Types -
//======================================================================

typedef enum {
    ClassSubtypeNone = 0,
    ClassSubtypeCFArray,
    ClassSubtypeNSArrayMutable,
    ClassSubtypeNSArrayImmutable,
    ClassSubtypeCFString,
} ClassSubtype;

typedef struct {
    const char *name;
    KSObjCClassType type;
    ClassSubtype subtype;
    bool isMutable;
    bool (*isValidObject)(const void *object);
    int (*description)(const void *object, char *buffer, int bufferLength);
    const void *class;
} ClassData;

//======================================================================
#pragma mark - Globals -
//======================================================================

// Forward references
static bool objectIsValid(const void *object);
static bool taggedObjectIsValid(const void *object);
static bool stringIsValid(const void *object);
static bool urlIsValid(const void *object);
static bool arrayIsValid(const void *object);
static bool dateIsValid(const void *object);
static bool numberIsValid(const void *object);
static bool taggedDateIsValid(const void *object);
static bool taggedNumberIsValid(const void *object);
static bool taggedStringIsValid(const void *object);

static int objectDescription(const void *object, char *buffer, int bufferLength);
static int taggedObjectDescription(const void *object, char *buffer, int bufferLength);
static int stringDescription(const void *object, char *buffer, int bufferLength);
static int urlDescription(const void *object, char *buffer, int bufferLength);
static int arrayDescription(const void *object, char *buffer, int bufferLength);
static int dateDescription(const void *object, char *buffer, int bufferLength);
static int numberDescription(const void *object, char *buffer, int bufferLength);
static int taggedDateDescription(const void *object, char *buffer, int bufferLength);
static int taggedNumberDescription(const void *object, char *buffer, int bufferLength);
static int taggedStringDescription(const void *object, char *buffer, int bufferLength);

// clang-format off
static ClassData g_classData[] =
{
    {"__NSCFString",           KSObjCClassTypeString,  ClassSubtypeNone,             true,  stringIsValid, stringDescription},
    {"NSCFString",             KSObjCClassTypeString,  ClassSubtypeNone,             true,  stringIsValid, stringDescription},
    {"__NSCFConstantString",   KSObjCClassTypeString,  ClassSubtypeNone,             true,  stringIsValid, stringDescription},
    {"NSCFConstantString",     KSObjCClassTypeString,  ClassSubtypeNone,             true,  stringIsValid, stringDescription},
    {"__NSArray0",             KSObjCClassTypeArray,   ClassSubtypeNSArrayImmutable, false, arrayIsValid,  arrayDescription},
    {"__NSArrayI",             KSObjCClassTypeArray,   ClassSubtypeNSArrayImmutable, false, arrayIsValid,  arrayDescription},
    {"__NSArrayM",             KSObjCClassTypeArray,   ClassSubtypeNSArrayMutable,   true,  arrayIsValid,  arrayDescription},
    {"__NSCFArray",            KSObjCClassTypeArray,   ClassSubtypeCFArray,          false, arrayIsValid,  arrayDescription},
    {"__NSSingleObjectArrayI", KSObjCClassTypeArray,   ClassSubtypeNSArrayImmutable, false, arrayIsValid,  arrayDescription},
    {"NSCFArray",              KSObjCClassTypeArray,   ClassSubtypeCFArray,          false, arrayIsValid,  arrayDescription},
    {"__NSDate",               KSObjCClassTypeDate,    ClassSubtypeNone,             false, dateIsValid,   dateDescription},
    {"NSDate",                 KSObjCClassTypeDate,    ClassSubtypeNone,             false, dateIsValid,   dateDescription},
    {"__NSCFNumber",           KSObjCClassTypeNumber,  ClassSubtypeNone,             false, numberIsValid, numberDescription},
    {"NSCFNumber",             KSObjCClassTypeNumber,  ClassSubtypeNone,             false, numberIsValid, numberDescription},
    {"NSNumber",               KSObjCClassTypeNumber,  ClassSubtypeNone,             false, numberIsValid, numberDescription},
    {"NSURL",                  KSObjCClassTypeURL,     ClassSubtypeNone,             false, urlIsValid,    urlDescription},
    {NULL,                     KSObjCClassTypeUnknown, ClassSubtypeNone,             false, objectIsValid, objectDescription},
};

static ClassData g_taggedClassData[] =
{
    {"NSAtom",            KSObjCClassTypeUnknown, ClassSubtypeNone, false, taggedObjectIsValid, taggedObjectDescription},
    {NULL,                KSObjCClassTypeUnknown, ClassSubtypeNone, false, taggedObjectIsValid, taggedObjectDescription},
    {"NSString",          KSObjCClassTypeString,  ClassSubtypeNone, false, taggedStringIsValid, taggedStringDescription},
    {"NSNumber",          KSObjCClassTypeNumber,  ClassSubtypeNone, false, taggedNumberIsValid, taggedNumberDescription},
    {"NSIndexPath",       KSObjCClassTypeUnknown, ClassSubtypeNone, false, taggedObjectIsValid, taggedObjectDescription},
    {"NSManagedObjectID", KSObjCClassTypeUnknown, ClassSubtypeNone, false, taggedObjectIsValid, taggedObjectDescription},
    {"NSDate",            KSObjCClassTypeDate,    ClassSubtypeNone, false, taggedDateIsValid,   taggedDateDescription},
    {NULL,                KSObjCClassTypeUnknown, ClassSubtypeNone, false, taggedObjectIsValid, taggedObjectDescription},
};
// clang-format on
static int g_taggedClassDataCount = sizeof(g_taggedClassData) / sizeof(*g_taggedClassData);

static const char *g_blockBaseClassName = "NSBlock";

//======================================================================
#pragma mark - Utility -
//======================================================================

#if OBJC_HAVE_TAGGED_POINTERS
static bool isTaggedPointer(const void *pointer) { return _objc_isTaggedPointer(pointer); }
static int getTaggedSlot(const void *pointer) { return (int)_objc_getTaggedPointerTag(pointer); }
static uintptr_t getTaggedPayload(const void *pointer) { return _objc_getTaggedPointerValue(pointer); }
static intptr_t getTaggedSignedPayload(const void *pointer) { return _objc_getTaggedPointerSignedValue(pointer); }
#else
static bool isTaggedPointer(__unused const void *pointer) { return false; }
static int getTaggedSlot(__unused const void *pointer) { return 0; }
static uintptr_t getTaggedPayload(const void *pointer) { return (uintptr_t)pointer; }
static intptr_t getTaggedSignedPayload(const void *pointer) { return (intptr_t)pointer; }
#endif

/** Get class data for a tagged pointer.
 *
 * @param object The tagged pointer.
 * @return The class data.
 */
static const ClassData *getClassDataFromTaggedPointer(const void *const object)
{
    int slot = getTaggedSlot(object);
    return &g_taggedClassData[slot];
}

static bool isValidTaggedPointer(const void *object)
{
    if (isTaggedPointer(object)) {
        if (getTaggedSlot(object) <= g_taggedClassDataCount) {
            const ClassData *classData = getClassDataFromTaggedPointer(object);
            return classData->type != KSObjCClassTypeUnknown;
        }
    }
    return false;
}

static const struct class_t *decodeIsaPointer(const void *const isaPointer)
{
#if ISA_TAG_MASK
    uintptr_t isa = (uintptr_t)isaPointer;
    if (isa & ISA_TAG_MASK) {
#if defined(__arm64__)
#if TARGET_OS_IOS
        if (floor(kCFCoreFoundationVersionNumber) <= kCFCoreFoundationVersionNumber_iOS_8_x_Max) {
            return (const struct class_t *)(isa & ISA_MASK_OLD);
        }
#endif

        return (const struct class_t *)(isa & ISA_MASK);
#else
        return (const struct class_t *)(isa & ISA_MASK);
#endif
    }
#endif
    return (const struct class_t *)isaPointer;
}

static const void *getIsaPointer(const void *const objectOrClassPtr)
{
    // This is wrong. Should not get class data here.
    //    if(ksobjc_isTaggedPointer(objectOrClassPtr))
    //    {
    //        return getClassDataFromTaggedPointer(objectOrClassPtr)->class;
    //    }

    const struct class_t *ptr = objectOrClassPtr;
    return decodeIsaPointer(ptr->isa);
}

static inline struct class_rw_t *getClassRW(const struct class_t *const class)
{
    uintptr_t ptr = class->data_NEVER_USE & FAST_DATA_MASK;
    return (struct class_rw_t *)ptr;
}

static inline const struct class_ro_t *getClassRO(const struct class_t *const class)
{
    class_rw_t *rw = getClassRW(class);
    uintptr_t ext_ptr = rw->ro_or_rw_ext;
    /* When objc_class_abi_version >= 1, it's a tagged union based on the low bit:
     * 0: class_ro_t  1: class_rw_ext_t
     * @see https://opensource.apple.com/source/objc4/objc4-781/runtime/objc-runtime-new.h */
    if (ext_ptr & 0x1UL) {
        ext_ptr &= ~0x1UL;
        struct class_rw_ext_t *rw_ext = (struct class_rw_ext_t *)ext_ptr;
        return rw_ext->ro;
    } else {
        struct class_ro_t *ro = (struct class_ro_t *)ext_ptr;
        return ro;
    }
}

static inline const void *getSuperClass(const void *const classPtr)
{
    const struct class_t *class = classPtr;
    return class->superclass;
}

static inline bool isMetaClass(const void *const classPtr) { return (getClassRO(classPtr)->flags & RO_META) != 0; }

static inline bool isRootClass(const void *const classPtr) { return (getClassRO(classPtr)->flags & RO_ROOT) != 0; }

static inline const char *getClassName(const void *classPtr)
{
    const struct class_ro_t *ro = getClassRO(classPtr);
    return ro->name;
}

/** Check if a tagged pointer is a number.
 *
 * @param object The object to query.
 * @return true if the tagged pointer is an NSNumber.
 */
static bool isTaggedPointerNSNumber(const void *const object) { return getTaggedSlot(object) == OBJC_TAG_NSNumber; }

/** Check if a tagged pointer is a string.
 *
 * @param object The object to query.
 * @return true if the tagged pointer is an NSString.
 */
static bool isTaggedPointerNSString(const void *const object) { return getTaggedSlot(object) == OBJC_TAG_NSString; }

/** Check if a tagged pointer is a date.
 *
 * @param object The object to query.
 * @return true if the tagged pointer is an NSDate.
 */
static bool isTaggedPointerNSDate(const void *const object) { return getTaggedSlot(object) == OBJC_TAG_NSDate; }

/** Extract an integer from a tagged NSNumber.
 *
 * @param object The NSNumber object (must be a tagged pointer).
 * @return The integer value.
 */
static int64_t extractTaggedNSNumber(const void *const object)
{
    intptr_t value = getTaggedSignedPayload(object);

    // The lower 4 bits encode type information so shift them out.
    return (int64_t)(value >> 4);
}

static int getTaggedNSStringLength(const void *const object)
{
    uintptr_t payload = getTaggedPayload(object);
    return (int)(payload & 0xf);
}

static int extractTaggedNSString(const void *const object, char *buffer, int bufferLength)
{
    int length = getTaggedNSStringLength(object);
    int copyLength = ((length + 1) > bufferLength) ? (bufferLength - 1) : length;
    uintptr_t payload = getTaggedPayload(object);
    uintptr_t value = payload >> 4;
    static char *alphabet = "eilotrm.apdnsIc ufkMShjTRxgC4013bDNvwyUL2O856P-B79AFKEWV_zGJ/HYX";
    if (length <= 7) {
        for (int i = 0; i < copyLength; i++) {
            // ASCII case, limit to bottom 7 bits just in case
            buffer[i] = (char)(value & 0x7f);
            value >>= 8;
        }
    } else if (length <= 9) {
        for (int i = 0; i < copyLength; i++) {
            uintptr_t index = (value >> ((length - 1 - i) * 6)) & 0x3f;
            buffer[i] = alphabet[index];
        }
    } else if (length <= 11) {
        for (int i = 0; i < copyLength; i++) {
            uintptr_t index = (value >> ((length - 1 - i) * 5)) & 0x1f;
            buffer[i] = alphabet[index];
        }
    } else {
        buffer[0] = 0;
    }
    buffer[length] = 0;

    return length;
}
#if OBJC_HAVE_TAGGED_POINTERS
/** Decodes the exponent of a tagged NSDate pointer.
 *
 * @param exp The 7-bit exponent value from the tagged NSDate pointer.
 * @return The decoded exponent value as a 64-bit unsigned integer.
 *
 * @note This function is based on the LLVM code in the Cocoa.cpp file:
 * https://github.com/apple/llvm-project/blob/5dc9d563e5a6cd2cdd44117697dead98955ccddf/lldb/source/Plugins/Language/ObjC/Cocoa.cpp#L934
 */
static uint64_t decodeExponent(uint64_t exp)
{
    // Bias value for tagged pointer exponents.
    // Recommended values:
    // 0x3e3: encodes all dates between distantPast and distantFuture
    //   except for the range within about 1e-28 second of the reference date.
    // 0x3ef: encodes all dates for a few million years beyond distantPast and
    //   distantFuture, except within about 1e-25 second of the reference date.
    static const int taggedDateExponentBias = 0x3ef;

    // Sign-extend the 7-bit exponent to 64 bits
    const uint64_t signBit = 1ULL << 6;
    const uint64_t extendMask = ~((1ULL << 7) - 1);
    exp = (exp ^ signBit) - signBit;
    exp &= (1ULL << 7) - 1;
    exp |= extendMask & -((exp & signBit) != 0);

    // Add the bias to the sign-extended exponent
    return exp + taggedDateExponentBias;
}

/** Extract a tagged NSDate's time value as an absolute time.
 *
 * @param object The NSDate object (must be a tagged pointer).
 * @return The date's absolute time.
 *
 * @note This function is based on the LLVM code in the Cocoa.cpp file:
 * https://github.com/apple/llvm-project/blob/5dc9d563e5a6cd2cdd44117697dead98955ccddf/lldb/source/Plugins/Language/ObjC/Cocoa.cpp#L913-L958
 */
static CFAbsoluteTime extractTaggedNSDate(const void *const object)
{
    uintptr_t payload = getTaggedPayload(object);

    if (kCFCoreFoundationVersionNumber >
        1600)  // https://github.com/apple/llvm-project/blob/5dc9d563e5a6cd2cdd44117697dead98955ccddf/lldb/source/Plugins/Language/ObjC/Cocoa.cpp#L1041
    {
        union {
            uintptr_t raw;
            struct {
                uint64_t fraction : 52;
                uint64_t exponent : 7;
                uint64_t sign : 1;
                uint64_t unused : 4;
            } bits;
        } encodedBits = { .raw = payload };

        if (encodedBits.raw == 0) return 0.0;
        if (encodedBits.raw == UINT64_MAX) return -0.0;

        union {
            CFAbsoluteTime value;
            struct {
                uint64_t fraction : 52;
                uint64_t exponent : 11;
                uint64_t sign : 1;
            } bits;
        } decodedBits = { .bits = { .fraction = encodedBits.bits.fraction,
                                    .exponent = decodeExponent(encodedBits.bits.exponent),
                                    .sign = encodedBits.bits.sign } };

        return decodedBits.value;
    } else {
        // Payload is a 60-bit float. Fortunately we can just cast across from
        // an integer pointer after shifting out the upper 4 bits.
        union {
            CFAbsoluteTime value;
            uintptr_t raw;
        } payloadBits = { .raw = payload << 4 };
        return payloadBits.value;
    }
}
#endif
/** Get any special class metadata we have about the specified class.
 * It will return a generic metadata object if the type is not recognized.
 *
 * Note: The Objective-C runtime is free to change a class address,
 * so I can't just blindly store class pointers at application start
 * and then compare against them later. However, comparing strings is
 * slow, so I've reached a compromise. Since I'm omly using this at
 * crash time, I can assume that the Objective-C environment is frozen.
 * As such, I can keep a cache of discovered classes. If, however, this
 * library is used outside of a frozen environment, caching will be
 * unreliable.
 *
 * @param class The class to examine.
 *
 * @return The associated class data.
 */
static ClassData *getClassData(const void *class)
{
    const char *className = getClassName(class);
    for (ClassData *data = g_classData;; data++) {
        unlikely_if(data->name == NULL) { return data; }
        unlikely_if(class == data->class) { return data; }
        unlikely_if(data->class == NULL && strcmp(className, data->name) == 0)
        {
            data->class = class;
            return data;
        }
    }
}

static inline const ClassData *getClassDataFromObject(const void *object)
{
    if (isTaggedPointer(object)) {
        return getClassDataFromTaggedPointer(object);
    }
    const struct class_t *obj = object;
    return getClassData(getIsaPointer(obj));
}

static int stringPrintf(char *buffer, int bufferLength, const char *fmt, ...)
{
    unlikely_if(bufferLength == 0) { return 0; }

    va_list args;
    va_start(args, fmt);
    int printLength = vsnprintf(buffer, bufferLength, fmt, args);
    va_end(args);

    unlikely_if(printLength < 0)
    {
        *buffer = 0;
        return 0;
    }
    unlikely_if(printLength > bufferLength) { return bufferLength - 1; }
    return printLength;
}

//======================================================================
#pragma mark - Validation -
//======================================================================

// Lookup table for validating class/ivar names and objc @encode types.
// An ivar name must start with a letter, and can contain letters & numbers.
// An ivar type can in theory be any combination of numbers, letters, and symbols
// in the ASCII range (0x21-0x7e).
#define INV 0  // Invalid.
#define N_C 5  // Name character: Valid for anything except the first letter of a name.
#define N_S 7  // Name start character: Valid for anything.
#define T_C 4  // Type character: Valid for types only.

static const unsigned int g_nameChars[] = {
    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, T_C, T_C, T_C, T_C, T_C, T_C, T_C, T_C, T_C, T_C, T_C,
    T_C, T_C, T_C, T_C, N_C, N_C, N_C, N_C, N_C, N_C, N_C, N_C, N_C, N_C, T_C, T_C, T_C, T_C, T_C, T_C, T_C, N_S,
    N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S,
    N_S, N_S, N_S, T_C, T_C, T_C, T_C, N_S, T_C, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S,
    N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, N_S, T_C, T_C, T_C, T_C, INV, INV, INV, INV, INV,
    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
};

#define VALID_NAME_CHAR(A) ((g_nameChars[(uint8_t)(A)] & 1) != 0)
#define VALID_NAME_START_CHAR(A) ((g_nameChars[(uint8_t)(A)] & 2) != 0)
#define VALID_TYPE_CHAR(A) ((g_nameChars[(uint8_t)(A)] & 7) != 0)

static bool isValidName(const char *const name, const int maxLength)
{
    if ((uintptr_t)name + (unsigned)maxLength < (uintptr_t)name) {
        // Wrapped around address space.
        return false;
    }

    char buffer[maxLength];
    int length = ksmem_copyMaxPossible(name, buffer, maxLength);
    if (length == 0 || !VALID_NAME_START_CHAR(name[0])) {
        return false;
    }
    for (int i = 1; i < length; i++) {
        unlikely_if(!VALID_NAME_CHAR(name[i]))
        {
            if (name[i] == 0) {
                return true;
            }
            return false;
        }
    }
    return false;
}

static bool isValidIvarType(const char *const type)
{
    char buffer[100];
    const int maxLength = sizeof(buffer);

    if ((uintptr_t)type + maxLength < (uintptr_t)type) {
        // Wrapped around address space.
        return false;
    }

    int length = ksmem_copyMaxPossible(type, buffer, maxLength);
    if (length == 0 || !VALID_TYPE_CHAR(type[0])) {
        return false;
    }
    for (int i = 0; i < length; i++) {
        unlikely_if(!VALID_TYPE_CHAR(type[i]))
        {
            if (type[i] == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool containsValidExtData(class_rw_t *rw)
{
    uintptr_t ext_ptr = rw->ro_or_rw_ext;
    if (ext_ptr & 0x1UL) {
        ext_ptr &= ~0x1UL;
        struct class_rw_ext_t *rw_ext = (struct class_rw_ext_t *)ext_ptr;
        if (!ksmem_isMemoryReadable(rw_ext, sizeof(*rw_ext))) {
            return false;
        }
    }
    return true;
}

static bool containsValidROData(const void *const classPtr)
{
    const struct class_t *const class = classPtr;
    if (!ksmem_isMemoryReadable(class, sizeof(*class))) {
        return false;
    }
    class_rw_t *rw = getClassRW(class);
    if (!ksmem_isMemoryReadable(rw, sizeof(*rw))) {
        return false;
    }
    if (!containsValidExtData(rw)) {
        return false;
    }
    const class_ro_t *ro = getClassRO(class);
    if (!ksmem_isMemoryReadable(ro, sizeof(*ro))) {
        return false;
    }
    return true;
}

static bool containsValidIvarData(const void *const classPtr)
{
    const struct class_ro_t *ro = getClassRO(classPtr);
    const struct ivar_list_t *ivars = ro->ivars;
    if (ivars == NULL) {
        return true;
    }
    if (!ksmem_isMemoryReadable(ivars, sizeof(*ivars))) {
        return false;
    }

    if (ivars->count > 0) {
        struct ivar_t ivar;
        uint8_t *ivarPtr = (uint8_t *)(&ivars->first) + ivars->entsizeAndFlags;
        for (uint32_t i = 1; i < ivars->count; i++) {
            if (!ksmem_copySafely(ivarPtr, &ivar, sizeof(ivar))) {
                return false;
            }
            if (!ksmem_isMemoryReadable(ivarPtr, (int)ivars->entsizeAndFlags)) {
                return false;
            }
            if (!ksmem_isMemoryReadable(ivar.offset, sizeof(*ivar.offset))) {
                return false;
            }
            if (!isValidName(ivar.name, kMaxNameLength)) {
                return false;
            }
            if (!isValidIvarType(ivar.type)) {
                return false;
            }
            ivarPtr += ivars->entsizeAndFlags;
        }
    }
    return true;
}

static bool containsValidClassName(const void *const classPtr)
{
    const struct class_ro_t *ro = getClassRO(classPtr);
    return isValidName(ro->name, kMaxNameLength);
}

static bool hasValidIsaPointer(const void *object)
{
    const struct class_t *isaPtr = getIsaPointer(object);
    return ksmem_isMemoryReadable(isaPtr, sizeof(*isaPtr));
}

static inline bool isValidClass(const void *classPtr)
{
    const class_t *class = classPtr;
    if (!ksmem_isMemoryReadable(class, sizeof(*class))) {
        return false;
    }
    if (!containsValidROData(class)) {
        return false;
    }
    if (!containsValidClassName(class)) {
        return false;
    }
    if (!containsValidIvarData(class)) {
        return false;
    }
    return true;
}

static inline bool isValidObject(const void *objectPtr)
{
    if (isTaggedPointer(objectPtr)) {
        return isValidTaggedPointer(objectPtr);
    }
    const class_t *object = objectPtr;
    if (!ksmem_isMemoryReadable(object, sizeof(*object))) {
        return false;
    }
    if (!hasValidIsaPointer(object)) {
        return false;
    }
    if (!isValidClass(getIsaPointer(object))) {
        return false;
    }
    return true;
}

//======================================================================
#pragma mark - Basic Objective-C Queries -
//======================================================================

const void *ksobjc_isaPointer(const void *const objectOrClassPtr) { return getIsaPointer(objectOrClassPtr); }

const void *ksobjc_superClass(const void *const classPtr) { return getSuperClass(classPtr); }

bool ksobjc_isMetaClass(const void *const classPtr) { return isMetaClass(classPtr); }

bool ksobjc_isRootClass(const void *const classPtr) { return isRootClass(classPtr); }

const char *ksobjc_className(const void *classPtr) { return getClassName(classPtr); }

const char *ksobjc_objectClassName(const void *objectPtr)
{
    if (isTaggedPointer(objectPtr)) {
        if (isValidTaggedPointer(objectPtr)) {
            const ClassData *class = getClassDataFromTaggedPointer(objectPtr);
            return class->name;
        }
        return NULL;
    }
    const void *isaPtr = getIsaPointer(objectPtr);
    return getClassName(isaPtr);
}

bool ksobjc_isClassNamed(const void *const classPtr, const char *const className)
{
    const char *name = getClassName(classPtr);
    if (name == NULL || className == NULL) {
        return false;
    }
    return strcmp(name, className) == 0;
}

bool ksobjc_isKindOfClass(const void *const classPtr, const char *const className)
{
    if (className == NULL) {
        return false;
    }

    const struct class_t *class = (const struct class_t *)classPtr;

    for (int i = 0; i < 20; i++) {
        const char *name = getClassName(class);
        if (name == NULL) {
            return false;
        }
        if (strcmp(className, name) == 0) {
            return true;
        }
        class = class->superclass;
        if (!containsValidROData(class)) {
            return false;
        }
    }
    return false;
}

const void *ksobjc_baseClass(const void *const classPtr)
{
    const struct class_t *superClass = classPtr;
    const struct class_t *subClass = classPtr;

    for (int i = 0; i < 20; i++) {
        if (isRootClass(superClass)) {
            return subClass;
        }
        subClass = superClass;
        superClass = superClass->superclass;
        if (!containsValidROData(superClass)) {
            return NULL;
        }
    }
    return NULL;
}

int ksobjc_ivarCount(const void *const classPtr)
{
    const struct ivar_list_t *ivars = getClassRO(classPtr)->ivars;
    if (ivars == NULL) {
        return 0;
    }
    return (int)ivars->count;
}

int ksobjc_ivarList(const void *const classPtr, KSObjCIvar *dstIvars, int ivarsCount)
{
    // TODO: Check this for a possible bad access.
    if (dstIvars == NULL) {
        return 0;
    }

    int count = ksobjc_ivarCount(classPtr);
    if (count == 0) {
        return 0;
    }

    if (ivarsCount < count) {
        count = ivarsCount;
    }
    const struct ivar_list_t *srcIvars = getClassRO(classPtr)->ivars;
    uintptr_t srcPtr = (uintptr_t)&srcIvars->first;
    const struct ivar_t *src = (void *)srcPtr;
    for (int i = 0; i < count; i++) {
        KSObjCIvar *dst = &dstIvars[i];
        dst->name = src->name;
        dst->type = src->type;
        dst->index = i;
        srcPtr += srcIvars->entsizeAndFlags;
        src = (void *)srcPtr;
    }
    return count;
}

bool ksobjc_ivarNamed(const void *const classPtr, const char *name, KSObjCIvar *dst)
{
    if (name == NULL) {
        return false;
    }
    const struct ivar_list_t *ivars = getClassRO(classPtr)->ivars;
    uintptr_t ivarPtr = (uintptr_t)&ivars->first;
    const struct ivar_t *ivar = (void *)ivarPtr;
    for (int i = 0; i < (int)ivars->count; i++) {
        if (ivar->name != NULL && strcmp(name, ivar->name) == 0) {
            dst->name = ivar->name;
            dst->type = ivar->type;
            dst->index = i;
            return true;
        }
        ivarPtr += ivars->entsizeAndFlags;
        ivar = (void *)ivarPtr;
    }
    return false;
}

bool ksobjc_ivarValue(const void *const objectPtr, int ivarIndex, void *dst)
{
#if OBJC_HAVE_TAGGED_POINTERS
    if (isTaggedPointer(objectPtr)) {
        // Naively assume they want "value".
        if (isTaggedPointerNSDate(objectPtr)) {
            CFTimeInterval value = extractTaggedNSDate(objectPtr);
            memcpy(dst, &value, sizeof(value));
            return true;
        }
        if (isTaggedPointerNSNumber(objectPtr)) {
            // TODO: Correct to assume 64-bit signed int? What does the actual ivar say?
            int64_t value = extractTaggedNSNumber(objectPtr);
            memcpy(dst, &value, sizeof(value));
            return true;
        }
        return false;
    }
#endif
    const void *const classPtr = getIsaPointer(objectPtr);
    const struct ivar_list_t *ivars = getClassRO(classPtr)->ivars;
    if (ivarIndex >= (int)ivars->count) {
        return false;
    }
    uintptr_t ivarPtr = (uintptr_t)&ivars->first;
    const struct ivar_t *ivar = (void *)(ivarPtr + (uintptr_t)ivars->entsizeAndFlags * (uintptr_t)ivarIndex);

    uintptr_t valuePtr = (uintptr_t)objectPtr + (uintptr_t)*ivar->offset;
    if (!ksmem_copySafely((void *)valuePtr, dst, (int)ivar->size)) {
        return false;
    }
    return true;
}

uintptr_t ksobjc_taggedPointerPayload(const void *taggedObjectPtr) { return getTaggedPayload(taggedObjectPtr); }

static inline bool isBlockClass(const void *class)
{
    const void *baseClass = ksobjc_baseClass(class);
    if (baseClass == NULL) {
        return false;
    }
    const char *name = getClassName(baseClass);
    if (name == NULL) {
        return false;
    }
    return strcmp(name, g_blockBaseClassName) == 0;
}

KSObjCType ksobjc_objectType(const void *objectOrClassPtr)
{
    if (objectOrClassPtr == NULL) {
        return KSObjCTypeUnknown;
    }

    if (isTaggedPointer(objectOrClassPtr)) {
        return KSObjCTypeObject;
    }

    if (!isValidObject(objectOrClassPtr) && !isValidClass(objectOrClassPtr)) {
        return KSObjCTypeUnknown;
    }

    const struct class_t *isa = getIsaPointer(objectOrClassPtr);

    if (isBlockClass(isa)) {
        return KSObjCTypeBlock;
    }
    if (!isMetaClass(isa)) {
        return KSObjCTypeObject;
    }

    return KSObjCTypeClass;
}

//======================================================================
#pragma mark - Unknown Object -
//======================================================================

static bool objectIsValid(__unused const void *object)
{
    // If it passed ksobjc_objectType, it's been validated as much as
    // possible.
    return true;
}

static bool taggedObjectIsValid(const void *object) { return isValidTaggedPointer(object); }

static int objectDescription(const void *object, char *buffer, int bufferLength)
{
    const void *class = getIsaPointer(object);
    const char *name = getClassName(class);
    uintptr_t objPointer = (uintptr_t)object;
    const char *fmt = sizeof(uintptr_t) == sizeof(uint32_t) ? "<%s: 0x%08x>" : "<%s: 0x%016x>";
    return stringPrintf(buffer, bufferLength, fmt, name, objPointer);
}

static int taggedObjectDescription(const void *object, char *buffer, int bufferLength)
{
    const ClassData *data = getClassDataFromTaggedPointer(object);
    const char *name = data->name;
    uintptr_t objPointer = (uintptr_t)object;
    const char *fmt = sizeof(uintptr_t) == sizeof(uint32_t) ? "<%s: 0x%08x>" : "<%s: 0x%016x>";
    return stringPrintf(buffer, bufferLength, fmt, name, objPointer);
}

//======================================================================
#pragma mark - NSString -
//======================================================================

static inline const char *stringStart(const struct __CFString *str)
{
    return (const char *)__CFStrContents(str) + (__CFStrHasLengthByte(str) ? 1 : 0);
}

static bool stringIsValid(const void *const stringPtr)
{
    const struct __CFString *string = stringPtr;
    struct __CFString temp;
    uint8_t oneByte;
    CFIndex length = -1;
    if (!ksmem_copySafely(string, &temp, sizeof(string->base))) {
        return false;
    }

    if (__CFStrIsInline(string)) {
        if (!ksmem_copySafely(&string->variants.inline1, &temp, sizeof(string->variants.inline1))) {
            return false;
        }
        length = string->variants.inline1.length;
    } else if (__CFStrIsMutable(string)) {
        if (!ksmem_copySafely(&string->variants.notInlineMutable, &temp, sizeof(string->variants.notInlineMutable))) {
            return false;
        }
        length = string->variants.notInlineMutable.length;
    } else if (!__CFStrHasLengthByte(string)) {
        if (!ksmem_copySafely(&string->variants.notInlineImmutable1, &temp,
                              sizeof(string->variants.notInlineImmutable1))) {
            return false;
        }
        length = string->variants.notInlineImmutable1.length;
    } else {
        if (!ksmem_copySafely(&string->variants.notInlineImmutable2, &temp,
                              sizeof(string->variants.notInlineImmutable2))) {
            return false;
        }
        if (!ksmem_copySafely(__CFStrContents(string), &oneByte, sizeof(oneByte))) {
            return false;
        }
        length = oneByte;
    }

    if (length < 0) {
        return false;
    } else if (length > 0) {
        if (!ksmem_copySafely(stringStart(string), &oneByte, sizeof(oneByte))) {
            return false;
        }
    }
    return true;
}

int ksobjc_stringLength(const void *const stringPtr)
{
    if (isTaggedPointer(stringPtr) && isTaggedPointerNSString(stringPtr)) {
        return getTaggedNSStringLength(stringPtr);
    }

    const struct __CFString *string = stringPtr;

    if (__CFStrHasExplicitLength(string)) {
        if (__CFStrIsInline(string)) {
            return (int)string->variants.inline1.length;
        } else {
            return (int)string->variants.notInlineImmutable1.length;
        }
    } else {
        return *((uint8_t *)__CFStrContents(string));
    }
}

#define kUTF16_LeadSurrogateStart 0xd800u
#define kUTF16_LeadSurrogateEnd 0xdbffu
#define kUTF16_TailSurrogateStart 0xdc00u
#define kUTF16_TailSurrogateEnd 0xdfffu
#define kUTF16_FirstSupplementaryPlane 0x10000u

static int copyAndConvertUTF16StringToUTF8(const void *const src, void *const dst, int charCount, int maxByteCount)
{
    const uint16_t *pSrc = src;
    uint8_t *pDst = dst;
    const uint8_t *const pDstEnd = pDst + maxByteCount - 1;  // Leave room for null termination.
    for (int charsRemaining = charCount; charsRemaining > 0 && pDst < pDstEnd; charsRemaining--) {
        // Decode UTF-16
        uint32_t character = 0;
        uint16_t leadSurrogate = *pSrc++;
        likely_if(leadSurrogate < kUTF16_LeadSurrogateStart || leadSurrogate > kUTF16_TailSurrogateEnd)
        {
            character = leadSurrogate;
        }
        else if (leadSurrogate > kUTF16_LeadSurrogateEnd)
        {
            // Inverted surrogate
            *((uint8_t *)dst) = 0;
            return 0;
        }
        else
        {
            uint16_t tailSurrogate = *pSrc++;
            if (tailSurrogate < kUTF16_TailSurrogateStart || tailSurrogate > kUTF16_TailSurrogateEnd) {
                // Invalid tail surrogate
                *((uint8_t *)dst) = 0;
                return 0;
            }
            character =
                ((leadSurrogate - kUTF16_LeadSurrogateStart) << 10) + (tailSurrogate - kUTF16_TailSurrogateStart);
            character += kUTF16_FirstSupplementaryPlane;
            charsRemaining--;
        }

        // Encode UTF-8
        likely_if(character <= 0x7f) { *pDst++ = (uint8_t)character; }
        else if (character <= 0x7ff)
        {
            if (pDstEnd - pDst >= 2) {
                *pDst++ = (uint8_t)(0xc0 | (character >> 6));
                *pDst++ = (uint8_t)(0x80 | (character & 0x3f));
            } else {
                break;
            }
        }
        else if (character <= 0xffff)
        {
            if (pDstEnd - pDst >= 3) {
                *pDst++ = (uint8_t)(0xe0 | (character >> 12));
                *pDst++ = (uint8_t)(0x80 | ((character >> 6) & 0x3f));
                *pDst++ = (uint8_t)(0x80 | (character & 0x3f));
            } else {
                break;
            }
        }
        // RFC3629 restricts UTF-8 to end at 0x10ffff.
        else if (character <= 0x10ffff)
        {
            if (pDstEnd - pDst >= 4) {
                *pDst++ = (uint8_t)(0xf0 | (character >> 18));
                *pDst++ = (uint8_t)(0x80 | ((character >> 12) & 0x3f));
                *pDst++ = (uint8_t)(0x80 | ((character >> 6) & 0x3f));
                *pDst++ = (uint8_t)(0x80 | (character & 0x3f));
            } else {
                break;
            }
        }
        else
        {
            // Invalid unicode.
            *((uint8_t *)dst) = 0;
            return 0;
        }
    }

    // Null terminate and return.
    *pDst = 0;
    return (int)(pDst - (uint8_t *)dst);
}

static int copy8BitString(const void *const src, void *const dst, int charCount, int maxByteCount)
{
    unlikely_if(maxByteCount == 0) { return 0; }
    unlikely_if(charCount == 0)
    {
        *((uint8_t *)dst) = 0;
        return 0;
    }

    unlikely_if(charCount >= maxByteCount) { charCount = maxByteCount - 1; }
    unlikely_if(!ksmem_copySafely(src, dst, charCount))
    {
        *((uint8_t *)dst) = 0;
        return 0;
    }
    uint8_t *charDst = dst;
    charDst[charCount] = 0;
    return charCount;
}

int ksobjc_copyStringContents(const void *stringPtr, char *dst, int maxByteCount)
{
    if (isTaggedPointer(stringPtr) && isTaggedPointerNSString(stringPtr)) {
        return extractTaggedNSString(stringPtr, dst, maxByteCount);
    }
    const struct __CFString *string = stringPtr;
    int charCount = ksobjc_stringLength(string);

    const char *src = stringStart(string);
    if (__CFStrIsUnicode(string)) {
        return copyAndConvertUTF16StringToUTF8(src, dst, charCount, maxByteCount);
    }

    return copy8BitString(src, dst, charCount, maxByteCount);
}

static int stringDescription(const void *object, char *buffer, int bufferLength)
{
    char *pBuffer = buffer;
    char *pEnd = buffer + bufferLength;

    pBuffer += objectDescription(object, pBuffer, (int)(pEnd - pBuffer));
    pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), ": \"");
    pBuffer += ksobjc_copyStringContents(object, pBuffer, (int)(pEnd - pBuffer));
    pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), "\"");

    return (int)(pBuffer - buffer);
}

static bool taggedStringIsValid(const void *const object)
{
    return isValidTaggedPointer(object) && isTaggedPointerNSString(object);
}

static int taggedStringDescription(const void *object, char *buffer, int bufferLength)
{
    return extractTaggedNSString(object, buffer, bufferLength);
}

//======================================================================
#pragma mark - NSURL -
//======================================================================

static bool urlIsValid(const void *const urlPtr)
{
    struct __CFURL url;
    if (!ksmem_copySafely(urlPtr, &url, sizeof(url))) {
        return false;
    }
    return stringIsValid(url._string);
}

int ksobjc_copyURLContents(const void *const urlPtr, char *dst, int maxLength)
{
    const struct __CFURL *url = urlPtr;
    return ksobjc_copyStringContents(url->_string, dst, maxLength);
}

static int urlDescription(const void *object, char *buffer, int bufferLength)
{
    char *pBuffer = buffer;
    char *pEnd = buffer + bufferLength;

    pBuffer += objectDescription(object, pBuffer, (int)(pEnd - pBuffer));
    pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), ": \"");
    pBuffer += ksobjc_copyURLContents(object, pBuffer, (int)(pEnd - pBuffer));
    pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), "\"");

    return (int)(pBuffer - buffer);
}

//======================================================================
#pragma mark - NSDate -
//======================================================================

static bool dateIsValid(const void *const datePtr)
{
    struct __CFDate temp;
    return ksmem_copySafely(datePtr, &temp, sizeof(temp));
}

CFAbsoluteTime ksobjc_dateContents(const void *const datePtr)
{
#if OBJC_HAVE_TAGGED_POINTERS
    if (isValidTaggedPointer(datePtr)) {
        return extractTaggedNSDate(datePtr);
    }
#endif
    const struct __CFDate *date = datePtr;
    return date->_time;
}

static int dateDescription(const void *object, char *buffer, int bufferLength)
{
    char *pBuffer = buffer;
    char *pEnd = buffer + bufferLength;

    CFAbsoluteTime time = ksobjc_dateContents(object);
    pBuffer += objectDescription(object, pBuffer, (int)(pEnd - pBuffer));
    pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), ": %f", time);

    return (int)(pBuffer - buffer);
}

static bool taggedDateIsValid(const void *const datePtr)
{
    return isValidTaggedPointer(datePtr) && isTaggedPointerNSDate(datePtr);
}

static int taggedDateDescription(const void *object, char *buffer, int bufferLength)
{
#if OBJC_HAVE_TAGGED_POINTERS
    char *pBuffer = buffer;
    char *pEnd = buffer + bufferLength;

    CFAbsoluteTime time = extractTaggedNSDate(object);
    pBuffer += taggedObjectDescription(object, pBuffer, (int)(pEnd - pBuffer));
    pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), ": %f", time);

    return (int)(pBuffer - buffer);
#else
    return 0;
#endif
}

//======================================================================
#pragma mark - NSNumber -
//======================================================================

#define NSNUMBER_CASE(CFTYPE, RETURN_TYPE, CAST_TYPE, DATA) \
    case CFTYPE: {                                          \
        RETURN_TYPE result;                                 \
        memcpy(&result, DATA, sizeof(result));              \
        return (CAST_TYPE)result;                           \
    }

#define EXTRACT_AND_RETURN_NSNUMBER(OBJECT, RETURN_TYPE)                    \
    if (isValidTaggedPointer(object)) {                                     \
        return (RETURN_TYPE)extractTaggedNSNumber(object);                  \
    }                                                                       \
    const struct __CFNumber *number = OBJECT;                               \
    CFNumberType cftype = CFNumberGetType((CFNumberRef)OBJECT);             \
    const void *data = &(number->_pad);                                     \
    switch (cftype) {                                                       \
        NSNUMBER_CASE(kCFNumberSInt8Type, int8_t, RETURN_TYPE, data)        \
        NSNUMBER_CASE(kCFNumberSInt16Type, int16_t, RETURN_TYPE, data)      \
        NSNUMBER_CASE(kCFNumberSInt32Type, int32_t, RETURN_TYPE, data)      \
        NSNUMBER_CASE(kCFNumberSInt64Type, int64_t, RETURN_TYPE, data)      \
        NSNUMBER_CASE(kCFNumberFloat32Type, Float32, RETURN_TYPE, data)     \
        NSNUMBER_CASE(kCFNumberFloat64Type, Float64, RETURN_TYPE, data)     \
        NSNUMBER_CASE(kCFNumberCharType, char, RETURN_TYPE, data)           \
        NSNUMBER_CASE(kCFNumberShortType, short, RETURN_TYPE, data)         \
        NSNUMBER_CASE(kCFNumberIntType, int, RETURN_TYPE, data)             \
        NSNUMBER_CASE(kCFNumberLongType, long, RETURN_TYPE, data)           \
        NSNUMBER_CASE(kCFNumberLongLongType, long long, RETURN_TYPE, data)  \
        NSNUMBER_CASE(kCFNumberFloatType, float, RETURN_TYPE, data)         \
        NSNUMBER_CASE(kCFNumberDoubleType, double, RETURN_TYPE, data)       \
        NSNUMBER_CASE(kCFNumberCFIndexType, CFIndex, RETURN_TYPE, data)     \
        NSNUMBER_CASE(kCFNumberNSIntegerType, NSInteger, RETURN_TYPE, data) \
        NSNUMBER_CASE(kCFNumberCGFloatType, CGFloat, RETURN_TYPE, data)     \
        default:                                                            \
            break;                                                          \
    }

Float64 ksobjc_numberAsFloat(const void *object)
{
    EXTRACT_AND_RETURN_NSNUMBER(object, Float64);
    return NAN;
}

int64_t ksobjc_numberAsInteger(const void *object)
{
    EXTRACT_AND_RETURN_NSNUMBER(object, int64_t);
    return 0;
}

bool ksobjc_numberIsFloat(const void *object) { return CFNumberIsFloatType((CFNumberRef)object); }

static bool numberIsValid(const void *const datePtr)
{
    struct __CFNumber temp;
    return ksmem_copySafely(datePtr, &temp, sizeof(temp));
}

static int numberDescription(const void *object, char *buffer, int bufferLength)
{
    char *pBuffer = buffer;
    char *pEnd = buffer + bufferLength;

    pBuffer += objectDescription(object, pBuffer, (int)(pEnd - pBuffer));

    if (ksobjc_numberIsFloat(object)) {
        Float64 value = ksobjc_numberAsFloat(object);
        pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), ": %lf", value);
    } else {
        int64_t value = ksobjc_numberAsInteger(object);
        pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), ": %" PRId64, value);
    }

    return (int)(pBuffer - buffer);
}

static bool taggedNumberIsValid(const void *const object)
{
    return isValidTaggedPointer(object) && isTaggedPointerNSNumber(object);
}

static int taggedNumberDescription(const void *object, char *buffer, int bufferLength)
{
    char *pBuffer = buffer;
    char *pEnd = buffer + bufferLength;

    int64_t value = extractTaggedNSNumber(object);
    pBuffer += taggedObjectDescription(object, pBuffer, (int)(pEnd - pBuffer));
    pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), ": %" PRId64, value);

    return (int)(pBuffer - buffer);
}

//======================================================================
#pragma mark - NSArray -
//======================================================================

/**
 * For old types
 */
struct NSArray {
    struct {
        void *isa;
        CFIndex count;
        id firstEntry;
    } basic;
};

/**
 * @struct NSArrayDescriptor
 * @brief Descriptor for new types like `__NSSingleObjectArrayI`, `__NSArrayM`, `__NSFrozenArrayM`.
 *
 * This structure is used to describe the internal representation of various mutable and single-object NSArray types.
 * It is adapted from the LLVM `NSArrayM` descriptor to provide compatibility with different types of arrays,
 * such as `__NSSingleObjectArrayI`, `__NSArrayM`, and `__NSFrozenArrayM`.
 *
 * @details This structure was inspired by the LLVM code found in the NSArray.cpp file:
 * https://github.com/apple/llvm-project/blob/29180d27e709b76965cc02c338188e37f2df9e7f/lldb/source/Plugins/Language/ObjC/NSArray.cpp#L148-L156
 * The first two fields, `_cow` (which often represents ISA) and `_data`, are also applicable for cases with
 * `__NSSingleObjectArrayI`.
 *
 * Many older versions of Foundation have different layouts and logic for different array types. Therefore, it is
 * crucial not to use these fields directly without inspecting Apple's code and making additional checks. This structure
 * is used here because it fits the current needs, but if something else is required (such as implementing mutable array
 * contents), it may require a different struct.
 *
 * @note The `packed` attribute ensures that there is no padding between the fields of the structure.
 */
typedef struct __attribute__((packed)) {
    uintptr_t _cow;
    uintptr_t _data;
    uint32_t _offset;
    uint32_t _size;
    uint32_t _muts;
    uint32_t _used;
} NSArrayDescriptor;

static inline bool nsarrayIsMutable(const void *const arrayPtr) { return getClassDataFromObject(arrayPtr)->isMutable; }

static inline bool nsarrayIsValid(const void *const arrayPtr)
{
    struct NSArray temp;
    return ksmem_copySafely(arrayPtr, &temp, sizeof(temp.basic));
}

/**
 * Get the count of elements in an NSArray.
 *
 * @note This function is based on the LLVM code in the NSArray.cpp file:
 * https://github.com/apple/llvm-project/blob/29180d27e709b76965cc02c338188e37f2df9e7f/lldb/source/Plugins/Language/ObjC/NSArray.cpp#L396-L412
 */
static inline int nsarrayCount(const void *const arrayPtr)
{
    const char *const className = ksobjc_objectClassName(arrayPtr);
    bool isMutable = kCFCoreFoundationVersionNumber > 1437 && strcmp(className, "__NSArrayM") == 0;
    bool isFrozen = kCFCoreFoundationVersionNumber > 1436 && strcmp(className, "__NSFrozenArrayM") == 0;

    if (isMutable || isFrozen) {
        NSArrayDescriptor descriptor = { 0 };
        if (ksmem_copySafely((const void *)((uintptr_t)arrayPtr + sizeof(uintptr_t)), &descriptor,
                             sizeof(NSArrayDescriptor))) {
            return (int)descriptor._used;
        }
    } else if (strcmp(className, "__NSSingleObjectArrayI") == 0) {
        return 1;
    } else if (strcmp(className, "__NSArray0") == 0) {
        return 0;
    } else {
        const struct NSArray *array = arrayPtr;
        return (array->basic.count >= 0) ? (int)array->basic.count : 0;
    }
    return 0;
}

static int nsarrayContents(const void *const arrayPtr, uintptr_t *contents, int count)
{
    int actualCount = nsarrayCount(arrayPtr);
    const char *const className = ksobjc_objectClassName(arrayPtr);

    if (actualCount < count) {
        if (actualCount <= 0) {
            return 0;
        }
        count = actualCount;
    }

    if (nsarrayIsMutable(arrayPtr)) {
        return 0;
    }

    const uintptr_t *entry = NULL;

    if (strcmp(className, "__NSSingleObjectArrayI") == 0) {
        const NSArrayDescriptor *arrayI = (const NSArrayDescriptor *)arrayPtr;
        // Using a temp variable to handle aligment of NSArrayDescriptor
        uintptr_t temp_data = arrayI->_data;
        entry = &temp_data;
    } else {
        const struct NSArray *array = (const struct NSArray *)arrayPtr;
        entry = (const uintptr_t *)&array->basic.firstEntry;
    }

    if (!ksmem_copySafely(entry, contents, (int)sizeof(*contents) * count)) {
        return 0;
    }

    return count;
}

static inline bool cfarrayIsValid(const void *const arrayPtr)
{
    struct __CFArray temp;
    if (!ksmem_copySafely(arrayPtr, &temp, sizeof(temp))) {
        return false;
    }
    const struct __CFArray *array = arrayPtr;
    if (__CFArrayGetType(array) == __kCFArrayDeque) {
        if (array->_store != NULL) {
            struct __CFArrayDeque deque;
            if (!ksmem_copySafely(array->_store, &deque, sizeof(deque))) {
                return false;
            }
        }
    }
    return true;
}

static inline const void *cfarrayData(const void *const arrayPtr) { return __CFArrayGetBucketsPtr(arrayPtr); }

static inline int cfarrayCount(const void *const arrayPtr)
{
    const struct __CFArray *array = arrayPtr;
    return array->_count < 0 ? 0 : (int)array->_count;
}

static int cfarrayContents(const void *const arrayPtr, uintptr_t *contents, int count)
{
    const struct __CFArray *array = arrayPtr;
    if (array->_count < (CFIndex)count) {
        if (array->_count <= 0) {
            return 0;
        }
        count = (int)array->_count;
    }

    const void *firstEntry = cfarrayData(array);
    if (!ksmem_copySafely(firstEntry, contents, (int)sizeof(*contents) * count)) {
        return 0;
    }
    return count;
}

static bool isCFArray(const void *const arrayPtr)
{
    const ClassData *data = getClassDataFromObject(arrayPtr);
    return data->subtype == ClassSubtypeCFArray;
}

int ksobjc_arrayCount(const void *const arrayPtr)
{
    if (isCFArray(arrayPtr)) {
        return cfarrayCount(arrayPtr);
    }
    return nsarrayCount(arrayPtr);
}

int ksobjc_arrayContents(const void *const arrayPtr, uintptr_t *contents, int count)
{
    if (isCFArray(arrayPtr)) {
        return cfarrayContents(arrayPtr, contents, count);
    }
    return nsarrayContents(arrayPtr, contents, count);
}

bool arrayIsValid(const void *object)
{
    if (isCFArray(object)) {
        return cfarrayIsValid(object);
    }
    return nsarrayIsValid(object);
}

static int arrayDescription(const void *object, char *buffer, int bufferLength)
{
    char *pBuffer = buffer;
    char *pEnd = buffer + bufferLength;

    pBuffer += objectDescription(object, pBuffer, (int)(pEnd - pBuffer));
    pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), ": [");

    if (pBuffer < pEnd - 1 && ksobjc_arrayCount(object) > 0) {
        uintptr_t contents = 0;
        if (ksobjc_arrayContents(object, &contents, 1) == 1) {
            pBuffer += ksobjc_getDescription((void *)contents, pBuffer, (int)(pEnd - pBuffer));
        }
    }
    pBuffer += stringPrintf(pBuffer, (int)(pEnd - pBuffer), "]");

    return (int)(pBuffer - buffer);
}

//======================================================================
#pragma mark - NSDictionary (BROKEN) -
//======================================================================

bool ksobjc_dictionaryFirstEntry(const void *dict, uintptr_t *key, uintptr_t *value)
{
    // TODO: This is broken.

    // Ensure memory is valid.
    struct __CFBasicHash copy;
    if (!ksmem_copySafely(dict, &copy, sizeof(copy))) {
        return false;
    }

    struct __CFBasicHash *ht = (struct __CFBasicHash *)dict;
    uintptr_t *keys = (uintptr_t *)ht->pointers + ht->bits.keys_offset;
    uintptr_t *values = (uintptr_t *)ht->pointers;

    // Dereference key and value pointers.
    if (!ksmem_copySafely(keys, &keys, sizeof(keys))) {
        return false;
    }

    if (!ksmem_copySafely(values, &values, sizeof(values))) {
        return false;
    }

    // Copy to destination.
    if (!ksmem_copySafely(keys, key, sizeof(*key))) {
        return false;
    }
    if (!ksmem_copySafely(values, value, sizeof(*value))) {
        return false;
    }
    return true;
}

// bool ksobjc_dictionaryContents(const void* dict, uintptr_t* keys, uintptr_t* values, CFIndex* count)
//{
//     struct CFBasicHash copy;
//     void* pointers[100];
//
//     if(!ksmem_copySafely(dict, &copy, sizeof(copy)))
//     {
//         return false;
//     }
//
//     struct CFBasicHash* ht = (struct CFBasicHash*)dict;
//     int values_offset = 0;
//     int keys_offset = copy.bits.keys_offset;
//     if(!ksmem_copySafely(&ht->pointers, pointers, sizeof(*pointers) * keys_offset))
//     {
//         return false;
//     }
//
//     return true;
// }

int ksobjc_dictionaryCount(const void *dict)
{
    // TODO: Implement me
#pragma unused(dict)
    return 0;
}

//======================================================================
#pragma mark - General Queries -
//======================================================================

int ksobjc_getDescription(void *object, char *buffer, int bufferLength)
{
    const ClassData *data = getClassDataFromObject(object);
    return data->description(object, buffer, bufferLength);
}

bool ksobjc_isTaggedPointer(const void *const pointer) { return isTaggedPointer(pointer); }

bool ksobjc_isValidTaggedPointer(const void *const pointer) { return isValidTaggedPointer(pointer); }

bool ksobjc_isValidObject(const void *object)
{
    if (!isValidObject(object)) {
        return false;
    }
    const ClassData *data = getClassDataFromObject(object);
    return data->isValidObject(object);
}

KSObjCClassType ksobjc_objectClassType(const void *object)
{
    const ClassData *data = getClassDataFromObject(object);
    return data->type;
}

//__NSArrayReversed
//__NSCFBoolean
//__NSCFDictionary
//__NSCFError
//__NSCFNumber
//__NSCFSet
//__NSCFString
//__NSDate
//__NSDictionaryI
//__NSDictionaryM
//__NSOrderedSetArrayProxy
//__NSOrderedSetI
//__NSOrderedSetM
//__NSOrderedSetReversed
//__NSOrderedSetSetProxy
//__NSPlaceholderArray
//__NSPlaceholderDate
//__NSPlaceholderDictionary
//__NSPlaceholderOrderedSet
//__NSPlaceholderSet
//__NSSetI
//__NSSetM
// NSArray
// NSCFArray
// NSCFBoolean
// NSCFDictionary
// NSCFError
// NSCFNumber
// NSCFSet
// NSCheapMutableString
// NSClassicHashTable
// NSClassicMapTable
// SConcreteHashTable
// NSConcreteMapTable
// NSConcreteValue
// NSDate
// NSDecimalNumber
// NSDecimalNumberPlaceholder
// NSDictionary
// NSError
// NSException
// NSHashTable
// NSMutableArray
// NSMutableDictionary
// NSMutableIndexSet
// NSMutableOrderedSet
// NSMutableRLEArray
// NSMutableSet
// NSMutableString
// NSMutableStringProxy
// NSNumber
// NSOrderedSet
// NSPlaceholderMutableString
// NSPlaceholderNumber
// NSPlaceholderString
// NSRLEArray
// NSSet
// NSSimpleCString
// NSString
// NSURL
