/**\file*********************************************************************
 *                                                                     \brief
 *  Assertions [lib.assertions 19.2]
 *
 ****************************************************************************
 */
#ifndef NTL__STLX_CASSERT
//#define NTL__STLX_CASSERT//done below

#ifndef NTL__STLX_CSTDDEF
#include "cstddef.hxx"
#endif

namespace ntl
{
  typedef void (*assert_handler)(const char* expr, const char* func, const char* file, int line);

#ifdef _MSC_VER
  __declspec(selectany)
#endif
  assert_handler __assert_handler = 0;

  inline assert_handler set_assert_handler(assert_handler handler)
  {
    assert_handler const a = __assert_handler;
    __assert_handler = handler;
    return a;
  }
} // ntl

/// Custom \c assert handler routine
typedef ntl::assert_handler __assert_handler;

/** Sets a custom \c assert handler */
inline __assert_handler __set_assert_handler(__assert_handler handler)
{
  return ntl::set_assert_handler(handler);
}

/** \c assert caller */
inline void __ntl_assert(const char* expr, const char* func, const char* file, int line)
{
  ntl::assert_handler handler = ntl::__assert_handler;
  if(handler)
    handler(expr, func, file, line);
  else
    __debugbreak();
}

#endif//#ifndef NTL__STLX_CASSERT


/// ISO C 7.2/1 The assert macro is redefined according to the current state
///             of NDEBUG each time that <assert.h> is included.
#undef assert

/// \c assert macros
#ifdef NDEBUG
  #define assert(expr) __noop
  #define _assert_msg(msg) __noop
  #define _assert_string(msg) __noop
#else
  #define assert(expr) \
  if ( !!(expr) ) {;} else if(ntl::__assert_handler) {\
      __ntl_assert("Assertion ("#expr") failed in ", __name__,__FILE__,__LINE__);\
    }else {__debugbreak(); }\
    ((void)0)
#define _assert_msg(msg) \
  if(ntl::__assert_handler){\
    __ntl_assert("Assertion (" msg ") failed in ", __name__,__FILE__,__LINE__);\
  }else { __debugbreak(); }\
      ((void)0)
#define _assert_string(msg) \
  if(ntl::__assert_handler)\
  __ntl_assert(msg,__name__,__FILE__,__LINE__);\
  else __debugbreak();\
  ((void)0)
#endif

#define __unreferenced(expr) (void)(expr)
#define __unused(expr) (void)(expr)

#ifndef NTL__STLX_CASSERT
#define NTL__STLX_CASSERT

#if defined(_MSC_VER) && !defined(__ICL)
namespace std
{
  namespace __
  {
#pragma warning(push)
#pragma warning(disable:4127)//conditional expression is constant
    extern "C" inline void __cdecl purecall_handler(void)
    {
      assert("pure virtual function called");
    }
    static void (__cdecl *__pchandler__)() = &purecall_handler;
#pragma warning(pop)
  }
}
#ifdef _M_X64
# pragma comment(linker, "/alternatename:_purecall=purecall_handler")
#else
# pragma comment(linker, "/alternatename:__purecall=_purecall_handler")
#endif

#endif // msc

#endif//#ifndef NTL__STLX_CASSERT
