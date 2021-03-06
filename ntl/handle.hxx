/**\file*********************************************************************
 *                                                                     \brief
 *  Handles support
 *
 ****************************************************************************
 */
#ifndef NTL__HANDLE
#define NTL__HANDLE
#pragma once

#include "stlx/cstdint.hxx"
#include "stlx/excptdef.hxx"
#include "stlx/cstddef.hxx"

namespace ntl
{

#if 0
namespace aux {
// this may give unresolved external symbol "bool __stdcall ntl::aux::is_valid<struct ntl::nt::_opaque const *>(struct ntl::nt::_opaque const *)"
template<typename T> __forceinline bool is_valid(T t) { return t != 0; }
}
#endif

/// handle RAII wrapper
template <class X,
          void(*Delete)(X),
          X(*Duplicate)(X)>
class basic_handle
{
#ifdef NTL_CXX_RV
  basic_handle(const basic_handle& ) __deleted;
  basic_handle& operator= (const basic_handle&) __deleted;
#endif

  public:
    typedef X element_type;

    explicit basic_handle(X h = X())  __ntl_nothrow : h(h) {}

#ifdef NTL_CXX_RV
    basic_handle(basic_handle&& a) __ntl_nothrow
      : h()
    {
      reset(a.release());
    }
    basic_handle& operator= (basic_handle&& a) __ntl_nothrow
    {
      reset(a.release());
      return *this;
    }

#else
    basic_handle(basic_handle & a)  __ntl_nothrow : h(a.release()) {}

    basic_handle & operator=(basic_handle & a) __ntl_nothrow
    {
      reset(a.release());
      return *this;
    }
#endif

    ~basic_handle() __ntl_nothrow { if ( X x = get() ) Delete(x); }

    //bool is_valid() const { return Validate(get()); }

    __explicit_operator_bool() const __ntl_nothrow { return __explicit_bool(get()); }

    X get() const __ntl_nothrow { return h; }
    X get() const volatile __ntl_nothrow { return h; }
    X release()   __ntl_nothrow { X tmp = get(); set(0); return tmp; }

    basic_handle duplicate() const __ntl_nothrow
    {
      return basic_handle( Duplicate(get()) );
    }

    void reset(X h = 0) __ntl_nothrow
    {
      X x = get();
      if (x && x != h ) Delete(x);
      set(h);
    }

    void swap(basic_handle& rhs)
    {
      X tmp = h;
      h = rhs.h;
      rhs.h = tmp;
    }

  ///////////////////////////////////////////////////////////////////////////
  private:

    X h;
    void set(X h) { this->h = h; }
};


}//namespace ntl

#endif//#ifndef NTL__HANDLE
