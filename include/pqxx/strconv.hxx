/* String conversion definitions.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/stringconv instead.
 *
 * Copyright (c) 2000-2019, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 */
#ifndef PQXX_H_STRINGCONV
#define PQXX_H_STRINGCONV

#include "pqxx/compiler-public.hxx"

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

#if __has_include(<charconv>)
#include <charconv>
#endif

#include "pqxx/except.hxx"


namespace pqxx
{
/**
 * @defgroup stringconversion String conversion
 *
 * The PostgreSQL server accepts and represents data in string form.  It has
 * its own formats for various data types.  The string conversions define how
 * various C++ types translate to and from their respective PostgreSQL text
 * representations.
 *
 * Each conversion is defined by a specialisation of the @c string_traits
 * template.  This template implements some basic functions to support the
 * conversion, ideally in both directions.
 *
 * If you need to convert a type which is not supported out of the box, define
 * your own @c string_traits specialisation for that type, similar to the ones
 * defined here.  Any conversion code which "sees" your specialisation will now
 * support your conversion.  In particular, you'll be able to read result
 * fields into a variable of the new type.
 *
 * There is a macro to help you define conversions for individual enumeration
 * types.  The conversion will represent enumeration values as numeric strings.
 */
//@{

// TODO: Do better!  Was having trouble with template variables.
/// A human-readable name for a type, used in error messages and such.
/** The default implementation falls back on @c std::type_info::name(), which
 * isn't necessarily human-friendly.
 */
template<typename TYPE> const std::string type_name{typeid(TYPE).name()};


/// @addtogroup stringconversion
//@{
/// Traits class for use in string conversions.
/** Specialize this template for a type for which you wish to add to_string
 * and from_string support.
 */
template<typename T, typename = void> struct string_traits;


/// Return a @c string_view representing value, plus terminating zero.
/** Produces a @c string_view, which will be null if @c value was null.
 * Otherwise, it will contain the PostgreSQL string representation for
 * @c value.  But in addition, if @c value is non-null then the result's
 * @c end() is guaranteed to be addressable and contain a zero.  This means
 * that you can also use its @c data() as a C-style string pointer.
 *
 * Uses the space from @c begin to @c end as a buffer, if needed.  The
 * returned string may lie somewhere in that buffer, or it may be a
 * compile-time constant, or it may be null if value was a null value.  Even
 * if the string is stored in the buffer, its @c begin() may or may not be
 * the same as @c begin.
 *
 * The @c string_view is guaranteed to be valid as long as the buffer from
 * @c begin to @c end remains accessible and unmodified.
 */
template<typename T> inline std::string_view
to_buf(char *begin, char *end, T value);


/// Value-to-string converter: represent value as a postgres-compatible string.
/** @warning This feature is experimental.  It may change, or disappear.
 * Turns a value of (more or less) any type into its PostgreSQL string
 * representation.  The string representation is only "alive" in memory while
 * the @c str object exists.
 *
 * If the value is null, the string value will be null.
 *
 * In situations where convenience matters more than performance, use the
 * @c to_string functions.  They create and return a @c std::string.  But if
 * performance is important, a @c std::string_view will be more efficient.
 *
 * @warning One thing you can @a not do with @c str is convert a value in a
 * temporary object, e.g. @c pqxx::str(value).view().  The result is a
 * @c std::string_view, which points to a buffer inside the @c str object.
 * By the time you make use of the result, the @c str and its buffer no longer
 * exist, and the @c string_view will be pointing to invalid memory.
 */
template<typename T> class str;
//@}
} // namespace pqxx


#include "pqxx/internal/conversions.hxx"


namespace pqxx
{
/// Helper class for defining enum conversions.
/** The conversion will convert enum values to numeric strings, and vice versa.
 *
 * To define a string conversion for an enum type, derive a @c string_traits
 * specialisation for the enum from this struct.
 *
 * There's usually an easier way though: the @c PQXX_DECLARE_ENUM_CONVERSION
 * macro.  Use @c enum_traits manually only if you need to customise your
 * traits type in more detail, e.g. if your enum has a "null" value built in.
 */
template<typename ENUM>
struct enum_traits
{
  using underlying_type = typename std::underlying_type<ENUM>::type;
  using underlying_traits = string_traits<underlying_type>;

  static constexpr bool has_null() noexcept { return false; }
  [[noreturn]] static ENUM null()
	{ internal::throw_null_conversion("enum type"); }

  static void from_string(std::string_view str, ENUM &obj)
  {
    underlying_type tmp;
    underlying_traits::from_string(str, tmp);
    obj = ENUM(tmp);
  }

  static std::string to_string(ENUM obj)
	{ return underlying_traits::to_string(underlying_type(obj)); }
};


/// Macro: Define a string conversion for an enum type.
/** This specialises the @c pqxx::string_traits template, so use it in the
 * @c ::pqxx namespace.
 *
 * For example:
 *
 *      #include <iostream>
 *      #include <pqxx/strconv>
 *      enum X { xa, xb };
 *      namespace pqxx { PQXX_DECLARE_ENUM_CONVERSION(x); }
 *      int main() { std::cout << to_string(xa) << std::endl; }
 */
#define PQXX_DECLARE_ENUM_CONVERSION(ENUM) \
template<> \
struct string_traits<ENUM> : pqxx::enum_traits<ENUM> \
{ \
  [[noreturn]] static ENUM null() \
	{ internal::throw_null_conversion(type_name<ENUM>); } \
}
//@}


/// Helper: declare a string_traits specialisation for a builtin type.
#define PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(TYPE) \
  template<> struct PQXX_LIBEXPORT string_traits<TYPE> : \
    internal::builtin_traits<TYPE> \
    { \
      [[noreturn]] static TYPE null() \
	{ pqxx::internal::throw_null_conversion(type_name<TYPE>); } \
    }

PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(bool);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(short);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(unsigned short);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(int);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(unsigned int);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(long);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(unsigned long);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(long long);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(unsigned long long);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(float);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(double);
PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION(long double);
#undef PQXX_DECLARE_STRING_TRAITS_SPECIALIZATION


/// Attempt to convert postgres-generated string to given built-in type.
/** If the form of the value found in the string does not match the expected
 * type, e.g. if a decimal point is found when converting to an integer type,
 * the conversion fails.  Overflows (e.g. converting "9999999999" to a 16-bit
 * C++ type) are also treated as errors.  If in some cases this behaviour should
 * be inappropriate, convert to something bigger such as @c long @c int first
 * and then truncate the resulting value.
 *
 * Only the simplest possible conversions are supported.  No fancy features
 * such as hexadecimal or octal, spurious signs, or exponent notation will work.
 * No whitespace is stripped away.  Only the kinds of strings that come out of
 * PostgreSQL and out of to_string() can be converted.
 */
template<typename T> inline void from_string(std::string_view str, T &obj)
{
  if (str.data() == nullptr)
    throw std::runtime_error{"Attempt to read null string."};
  string_traits<T>::from_string(str, obj);
}


template<typename T>
inline void from_string(const std::stringstream &str, T &obj)		//[t00]
	{ from_string(str.str(), obj); }


/// Convert built-in type to a readable string that PostgreSQL will understand
/** No special formatting is done, and any locale settings are ignored.  The
 * resulting string will be human-readable and in a format suitable for use in
 * SQL queries.
 */
template<typename T> std::string to_string(const T &obj)
	{ return string_traits<T>::to_string(obj); }

} // namespace pqxx
#endif
