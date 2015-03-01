// Copyright (c) 2013-2014 Sandstorm Development Group, Inc. and contributors
// Licensed under the MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// This file contains types which are intended to help detect incorrect usage at compile
// time, but should then be optimized down to basic primitives (usually, integers) by the
// compiler.

#ifndef CAPNP_COMMON_H_
#define CAPNP_COMMON_H_

#if defined(__GNUC__) && !CAPNP_HEADER_WARNINGS
#pragma GCC system_header
#endif

#include <kj/units.h>
#include <inttypes.h>
#include <kj/string.h>

namespace capnp {

#define CAPNP_VERSION_MAJOR 0
#define CAPNP_VERSION_MINOR 6
#define CAPNP_VERSION_MICRO 0

#define CAPNP_VERSION \
  (CAPNP_VERSION_MAJOR * 1000000 + CAPNP_VERSION_MINOR * 1000 + CAPNP_VERSION_MICRO)

#ifdef _MSC_VER
#define CAPNP_LITE 1
// MSVC only supports "lite" mode for now, due to missing C++11 features.
#endif

#ifndef CAPNP_LITE
#define CAPNP_LITE 0
#endif

typedef unsigned int uint;

struct Void {
  // Type used for Void fields.  Using C++'s "void" type creates a bunch of issues since it behaves
  // differently from other types.

  inline constexpr bool operator==(Void other) const { return true; }
  inline constexpr bool operator!=(Void other) const { return false; }
};

static KJ_CONSTEXPR(const) Void VOID = Void();
// Constant value for `Void`,  which is an empty struct.

inline kj::StringPtr KJ_STRINGIFY(Void) { return "void"; }

struct Text;
struct Data;

enum class Kind: uint8_t {
  PRIMITIVE,
  BLOB,
  ENUM,
  STRUCT,
  UNION,
  INTERFACE,
  LIST,

  OTHER
  // Some other type which is often a type parameter to Cap'n Proto templates, but which needs
  // special handling. This includes types like AnyPointer, Dynamic*, etc.
};

enum class ElementSize: uint8_t {
  // Size of a list element.

  VOID = 0,
  BIT = 1,
  BYTE = 2,
  TWO_BYTES = 3,
  FOUR_BYTES = 4,
  EIGHT_BYTES = 5,

  POINTER = 6,

  INLINE_COMPOSITE = 7
};

namespace schemas {

template <typename T>
struct EnumInfo;

}  // namespace schemas

namespace _ {  // private

template <typename T, typename = typename T::_capnpPrivate::IsStruct> uint8_t kindSfinae(int);
template <typename T, typename = typename T::_capnpPrivate::IsInterface> uint16_t kindSfinae(int);
template <typename T, typename = typename schemas::EnumInfo<T>::IsEnum> uint32_t kindSfinae(int);
template <typename T> uint64_t kindSfinae(...);

template <typename T>
struct MsvcWorkaround {
  // TODO(msvc): Remove this once MSVC supports expression SFINAE.
  enum { value = sizeof(kindSfinae<T>(0)) };
};

template <typename T, size_t s = MsvcWorkaround<T>::value> struct Kind_;

template <> struct Kind_<Void> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<bool> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<int8_t> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<int16_t> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<int32_t> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<int64_t> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<uint8_t> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<uint16_t> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<uint32_t> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<uint64_t> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<float> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<double> { static constexpr Kind kind = Kind::PRIMITIVE; };
template <> struct Kind_<Text> { static constexpr Kind kind = Kind::BLOB; };
template <> struct Kind_<Data> { static constexpr Kind kind = Kind::BLOB; };

template <typename T> struct Kind_<T, sizeof(uint8_t)> { static constexpr Kind kind = Kind::STRUCT; };
template <typename T> struct Kind_<T, sizeof(uint16_t)> { static constexpr Kind kind = Kind::INTERFACE; };
template <typename T> struct Kind_<T, sizeof(uint32_t)> { static constexpr Kind kind = Kind::ENUM; };

}  // namespace _ (private)

#if CAPNP_LITE

#define CAPNP_KIND(T) ::capnp::_::Kind_<T>::kind
// Avoid constexpr methods in lite mode (MSVC is bad at constexpr).

#else  // CAPNP_LITE

template <typename T, Kind k = _::Kind_<T>::kind>
inline constexpr Kind kind() {
  // This overload of kind() matches types which have a Kind_ specialization.

  return k;
}

#define CAPNP_KIND(T) ::capnp::kind<T>()
// Use this macro rather than kind<T>() in any code which must work in lite mode.

#endif  // CAPNP_LITE, else

template <typename T, Kind k = CAPNP_KIND(T)>
struct List;

#if _MSC_VER

template <typename T, Kind k>
struct List {};
// For some reason, without this declaration, MSVC will error out on some uses of List
// claiming that "T" -- as used in the default initializer for the second template param, "k" --
// is not defined. I do not understand this error, but adding this empty default declaration fixes
// it.

#endif

template <typename T> struct ListElementType_;
template <typename T> struct ListElementType_<List<T>> { typedef T Type; };
template <typename T> using ListElementType = typename ListElementType_<T>::Type;

namespace _ {  // private
template <typename T, Kind k> struct Kind_<List<T, k>, sizeof(uint64_t)> {
  static constexpr Kind kind = Kind::LIST;
};
}  // namespace _ (private)

template <typename T, Kind k = CAPNP_KIND(T)> struct ReaderFor_ { typedef typename T::Reader Type; };
template <typename T> struct ReaderFor_<T, Kind::PRIMITIVE> { typedef T Type; };
template <typename T> struct ReaderFor_<T, Kind::ENUM> { typedef T Type; };
template <typename T> struct ReaderFor_<T, Kind::INTERFACE> { typedef typename T::Client Type; };
template <typename T> using ReaderFor = typename ReaderFor_<T>::Type;
// The type returned by List<T>::Reader::operator[].

template <typename T, Kind k = CAPNP_KIND(T)> struct BuilderFor_ { typedef typename T::Builder Type; };
template <typename T> struct BuilderFor_<T, Kind::PRIMITIVE> { typedef T Type; };
template <typename T> struct BuilderFor_<T, Kind::ENUM> { typedef T Type; };
template <typename T> struct BuilderFor_<T, Kind::INTERFACE> { typedef typename T::Client Type; };
template <typename T> using BuilderFor = typename BuilderFor_<T>::Type;
// The type returned by List<T>::Builder::operator[].

template <typename T, Kind k = CAPNP_KIND(T)> struct PipelineFor_ { typedef typename T::Pipeline Type;};
template <typename T> struct PipelineFor_<T, Kind::INTERFACE> { typedef typename T::Client Type; };
template <typename T> using PipelineFor = typename PipelineFor_<T>::Type;

template <typename T, Kind k = CAPNP_KIND(T)> struct TypeIfEnum_;
template <typename T> struct TypeIfEnum_<T, Kind::ENUM> { typedef T Type; };

template <typename T>
using TypeIfEnum = typename TypeIfEnum_<kj::Decay<T>>::Type;

template <typename T>
using FromReader = typename kj::Decay<T>::Reads;
// FromReader<MyType::Reader> = MyType (for any Cap'n Proto type).

template <typename T>
using FromBuilder = typename kj::Decay<T>::Builds;
// FromBuilder<MyType::Builder> = MyType (for any Cap'n Proto type).

template <typename T>
using FromPipeline = typename kj::Decay<T>::Pipelines;
// FromBuilder<MyType::Pipeline> = MyType (for any Cap'n Proto type).

template <typename T>
using FromClient = typename kj::Decay<T>::Calls;
// FromReader<MyType::Client> = MyType (for any Cap'n Proto interface type).

template <typename T>
using FromServer = typename kj::Decay<T>::Serves;
// FromBuilder<MyType::Server> = MyType (for any Cap'n Proto interface type).

namespace _ {  // private

template <typename T, Kind k = CAPNP_KIND(T)>
struct PointerHelpers;

#if _MSC_VER

template <typename T, Kind k>
struct PointerHelpers {};
// For some reason, without this declaration, MSVC will error out on some uses of PointerHelpers
// claiming that "T" -- as used in the default initializer for the second template param, "k" --
// is not defined. I do not understand this error, but adding this empty default declaration fixes
// it.

#endif

}  // namespace _ (private)

struct MessageSize {
  // Size of a message.  Every struct type has a method `.totalSize()` that returns this.
  uint64_t wordCount;
  uint capCount;
};

// =======================================================================================
// Raw memory types and measures

using kj::byte;

class word { uint64_t content KJ_UNUSED_MEMBER; KJ_DISALLOW_COPY(word); public: word() = default; };
// word is an opaque type with size of 64 bits.  This type is useful only to make pointer
// arithmetic clearer.  Since the contents are private, the only way to access them is to first
// reinterpret_cast to some other pointer type.
//
// Copying is disallowed because you should always use memcpy().  Otherwise, you may run afoul of
// aliasing rules.
//
// A pointer of type word* should always be word-aligned even if won't actually be dereferenced as
// that type.

static_assert(sizeof(byte) == 1, "uint8_t is not one byte?");
static_assert(sizeof(word) == 8, "uint64_t is not 8 bytes?");

#if CAPNP_DEBUG_TYPES
// Set CAPNP_DEBUG_TYPES to 1 to use kj::Quantity for "count" types.  Otherwise, plain integers are
// used.  All the code should still operate exactly the same, we just lose compile-time checking.
// Note that this will also change symbol names, so it's important that the library and any clients
// be compiled with the same setting here.
//
// We disable this by default to reduce symbol name size and avoid any possibility of the compiler
// failing to fully-optimize the types, but anyone modifying Cap'n Proto itself should enable this
// during development and testing.

namespace _ { class BitLabel; class ElementLabel; struct WirePointer; }

template <uint width, typename T = uint>
using BitCountN = kj::Quantity<kj::Guarded<kj::maxValueForBits<width>(), T>, _::BitLabel>;
template <uint width, typename T = uint>
using ByteCountN = kj::Quantity<kj::Guarded<kj::maxValueForBits<width>(), T>, byte>;
template <uint width, typename T = uint>
using WordCountN = kj::Quantity<kj::Guarded<kj::maxValueForBits<width>(), T>, word>;
template <uint width, typename T = uint>
using ElementCountN = kj::Quantity<kj::Guarded<kj::maxValueForBits<width>(), T>, _::ElementLabel>;
template <uint width, typename T = uint>
using WirePointerCountN = kj::Quantity<kj::Guarded<kj::maxValueForBits<width>(), T>, _::WirePointer>;

typedef BitCountN<8, uint8_t> BitCount8;
typedef BitCountN<16, uint16_t> BitCount16;
typedef BitCountN<32, uint32_t> BitCount32;
typedef BitCountN<64, uint64_t> BitCount64;
typedef BitCountN<sizeof(uint) * 8, uint> BitCount;

typedef ByteCountN<8, uint8_t> ByteCount8;
typedef ByteCountN<16, uint16_t> ByteCount16;
typedef ByteCountN<32, uint32_t> ByteCount32;
typedef ByteCountN<64, uint64_t> ByteCount64;
typedef ByteCountN<sizeof(uint) * 8, uint> ByteCount;

typedef WordCountN<8, uint8_t> WordCount8;
typedef WordCountN<16, uint16_t> WordCount16;
typedef WordCountN<32, uint32_t> WordCount32;
typedef WordCountN<64, uint64_t> WordCount64;
typedef WordCountN<sizeof(uint) * 8, uint> WordCount;

typedef ElementCountN<8, uint8_t> ElementCount8;
typedef ElementCountN<16, uint16_t> ElementCount16;
typedef ElementCountN<32, uint32_t> ElementCount32;
typedef ElementCountN<64, uint64_t> ElementCount64;
typedef ElementCountN<sizeof(uint) * 8, uint> ElementCount;

typedef WirePointerCountN<8, uint8_t> WirePointerCount8;
typedef WirePointerCountN<16, uint16_t> WirePointerCount16;
typedef WirePointerCountN<32, uint32_t> WirePointerCount32;
typedef WirePointerCountN<64, uint64_t> WirePointerCount64;
typedef WirePointerCountN<sizeof(uint) * 8, uint> WirePointerCount;

template <uint width>
using BitsPerElementN = decltype(BitCountN<width>() / ElementCountN<width>());
template <uint width>
using BytesPerElementN = decltype(ByteCountN<width>() / ElementCountN<width>());
template <uint width>
using WordsPerElementN = decltype(WordCountN<width>() / ElementCountN<width>());
template <uint width>
using PointersPerElementN = decltype(WirePointerCountN<width>() / ElementCountN<width>());

using kj::guarded;
using kj::unguard;
using kj::unguardAs;
using kj::unguardMax;
using kj::unguardMaxBits;
using kj::assertMax;
using kj::assertMaxBits;
using kj::upgradeGuard;
using kj::ThrowOverflow;
using kj::assumeBits;
using kj::subtractChecked;

template <typename T, typename U>
inline constexpr U* operator+(U* ptr, kj::Quantity<T, U> offset) {
  return ptr + unguard(offset / kj::unit<kj::Quantity<T, U>>());
}
template <typename T, typename U>
inline constexpr const U* operator+(const U* ptr, kj::Quantity<T, U> offset) {
  return ptr + unguard(offset / kj::unit<kj::Quantity<T, U>>());
}
template <typename T, typename U>
inline constexpr U* operator+=(U*& ptr, kj::Quantity<T, U> offset) {
  return ptr = ptr + unguard(offset / kj::unit<kj::Quantity<T, U>>());
}
template <typename T, typename U>
inline constexpr const U* operator+=(const U*& ptr, kj::Quantity<T, U> offset) {
  return ptr = ptr + unguard(offset / kj::unit<kj::Quantity<T, U>>());
}

template <typename T, typename U>
inline constexpr U* operator-(U* ptr, kj::Quantity<T, U> offset) {
  return ptr - unguard(offset / kj::unit<kj::Quantity<T, U>>());
}
template <typename T, typename U>
inline constexpr const U* operator-(const U* ptr, kj::Quantity<T, U> offset) {
  return ptr - unguard(offset / kj::unit<kj::Quantity<T, U>>());
}
template <typename T, typename U>
inline constexpr U* operator-=(U*& ptr, kj::Quantity<T, U> offset) {
  return ptr = ptr - unguard(offset / kj::unit<kj::Quantity<T, U>>());
}
template <typename T, typename U>
inline constexpr const U* operator-=(const U*& ptr, kj::Quantity<T, U> offset) {
  return ptr = ptr - unguard(offset / kj::unit<kj::Quantity<T, U>>());
}

constexpr auto BITS = kj::unit<BitCountN<1>>();
constexpr auto BYTES = kj::unit<ByteCountN<1>>();
constexpr auto WORDS = kj::unit<WordCountN<1>>();
constexpr auto ELEMENTS = kj::unit<ElementCountN<1>>();
constexpr auto POINTERS = kj::unit<WirePointerCountN<1>>();

constexpr auto ZERO = kj::guarded<0>();
constexpr auto ONE = kj::guarded<1>();

// GCC 4.7 actually gives unused warnings on these constants in opt mode...
constexpr auto BITS_PER_BYTE KJ_UNUSED = guarded<8>() * BITS / BYTES;
constexpr auto BITS_PER_WORD KJ_UNUSED = guarded<64>() * BITS / WORDS;
constexpr auto BYTES_PER_WORD KJ_UNUSED = guarded<8>() * BYTES / WORDS;

constexpr auto BITS_PER_POINTER KJ_UNUSED = guarded<64>() * BITS / POINTERS;
constexpr auto BYTES_PER_POINTER KJ_UNUSED = guarded<8>() * BYTES / POINTERS;
constexpr auto WORDS_PER_POINTER KJ_UNUSED = ONE * WORDS / POINTERS;

constexpr auto POINTER_SIZE_IN_WORDS = ONE * POINTERS * WORDS_PER_POINTER;

constexpr uint SEGMENT_WORD_COUNT_BITS = 29;      // Number of words in a segment.
constexpr uint LIST_ELEMENT_COUNT_BITS = 29;      // Number of elements in a list.
constexpr uint STRUCT_DATA_WORD_COUNT_BITS = 16;  // Number of words in a Struct data section.
constexpr uint STRUCT_POINTER_COUNT_BITS = 16;    // Number of pointers in a Struct pointer section.
constexpr uint BLOB_SIZE_BITS = 29;               // Number of bytes in a blob.

typedef WordCountN<SEGMENT_WORD_COUNT_BITS> SegmentWordCount;
typedef ElementCountN<LIST_ELEMENT_COUNT_BITS> ListElementCount;
typedef WordCountN<STRUCT_DATA_WORD_COUNT_BITS, uint16_t> StructDataWordCount;
typedef WirePointerCountN<STRUCT_POINTER_COUNT_BITS, uint16_t> StructPointerCount;
typedef ByteCountN<BLOB_SIZE_BITS> BlobSize;

constexpr auto MAX_SEGMENT_WORDS =
    guarded<kj::maxValueForBits<SEGMENT_WORD_COUNT_BITS>()>() * WORDS;
constexpr auto MAX_LIST_ELEMENTS =
    guarded<kj::maxValueForBits<LIST_ELEMENT_COUNT_BITS>()>() * ELEMENTS;
constexpr auto MAX_STUCT_DATA_WORDS =
    guarded<kj::maxValueForBits<STRUCT_DATA_WORD_COUNT_BITS>()>() * WORDS;
constexpr auto MAX_STRUCT_POINTER_COUNT =
    guarded<kj::maxValueForBits<STRUCT_POINTER_COUNT_BITS>()>() *POINTERS;

using StructDataBitCount = decltype(WordCountN<STRUCT_POINTER_COUNT_BITS>() * BITS_PER_WORD);
using StructDataElementOffset = decltype(StructDataBitCount() * (ONE * ELEMENTS / BITS));
// Number of bits in a Struct data segment (should come out to BitCountN<22>).

constexpr uint MAX_TEXT_SIZE = kj::maxValueForBits<BLOB_SIZE_BITS>() - 1;
typedef kj::Quantity<kj::Guarded<MAX_TEXT_SIZE, uint>, byte> TextSize;
// Not including NUL terminator.

template <typename T>
inline KJ_CONSTEXPR() decltype(BYTES / ELEMENTS) bytesPerElement() {
  return guarded<sizeof(T)>() * BYTES / ELEMENTS;
}

template <typename T>
inline KJ_CONSTEXPR() decltype(BITS / ELEMENTS) bitsPerElement() {
  return guarded<sizeof(T)>() * 8 * BITS / ELEMENTS;
}

inline constexpr ByteCountN<sizeof(size_t) * 8, size_t>
    intervalLength(const byte* a, const byte* b) {
  return kj::guarded(b - a) * BYTES;
}
inline constexpr WordCountN<sizeof(size_t) * 8, size_t>
    intervalLength(const word* a, const word* b) {
  return kj::guarded(b - a) * WORDS;
}

#else

#error TODO

#endif

}  // namespace capnp

#endif  // CAPNP_COMMON_H_
