/**\file*********************************************************************
 *                                                                     \brief
 *  Diagnostics library [19 diagnostics]
 *  Exception classes   [19.2 std.exceptions]
 *
 ****************************************************************************
 */
#ifndef NTL__STLX_STDEXCEPT
#define NTL__STLX_STDEXCEPT
#pragma once

#ifndef NTL__STLX_STDEXCEPT_FWD
#include "stdexcept_fwd.hxx"
#endif

namespace std
{
  namespace __
  {
    struct NTL_EXPORTAPI exstring
    {
      exstring()
        :msg(), len()
      {}
      inline explicit exstring(const char* s);
      //  :msg(s), len()
      //{}
      inline explicit exstring(const std::string& s);

    #ifdef NTL_CXX_RV
      exstring(exstring&& r)
        :msg(r.msg), len(r.len)
      {
        r.msg = 0, r.len = 0;
      }
    #endif
      inline exstring(const exstring& r);

      ~exstring()
      {
        if(len)
          delete[] msg;
      }

      const char* c_str() const __ntl_nothrow { return msg; }
        
    private:
      const char* msg;
      size_t len;
    };
  }
}

#ifndef NTL__STLX_EXCEPTION
# include "stdexception.hxx"
#endif

namespace std
{
/**\addtogroup lib_diagnostics 19 Diagnostics library [diagnostics]
 *@{*/
/**\addtogroup std_exceptions  19.2 Exception classes [std.exceptions]
 *@{*/


/// 19.2.1 Class logic_error [logic.error]
class NTL_EXPORTAPI logic_error : public exception
{
  public:
    explicit logic_error(const string& what_arg) : msg(what_arg) {}
    explicit logic_error(const char* what_arg )  : msg(what_arg) {}
    virtual const char* what() const __ntl_nothrow { return msg.c_str(); }
    ~logic_error() /*__ntl_nothrow*/ {}
  private:
    const __::exstring msg;
  //protected:
    logic_error operator=(const logic_error&) {/* do not use */ return *this; }
};

/// 19.2.2 Class domain_error [domain.error]
class NTL_EXPORTAPI domain_error : public logic_error
{
  public:
    explicit domain_error(const string& what_arg) : logic_error(what_arg) {}
    explicit domain_error(const char* what_arg )  : logic_error(what_arg) {}
};

/// 19.2.3 Class invalid_argument [invalid.argument]
class NTL_EXPORTAPI invalid_argument : public logic_error
{
  public:
    explicit invalid_argument(const string& what_arg) : logic_error(what_arg) {}
    explicit invalid_argument(const char* what_arg )  : logic_error(what_arg) {}
};

/// 19.2.4 Class length_error [length.error]
class NTL_EXPORTAPI length_error : public logic_error
{
  public:
    explicit length_error(const string& what_arg) : logic_error(what_arg) {}
    explicit length_error(const char* what_arg )  : logic_error(what_arg) {}
};

/// 19.2.5 Class out_of_range [lib.out.of.range]
class NTL_EXPORTAPI out_of_range : public logic_error
{
  public:
    explicit out_of_range(const string& what_arg) : logic_error(what_arg) {}
    explicit out_of_range(const char* what_arg )  : logic_error(what_arg) {}
};

/// 19.2.6 Class runtime_error [lib.runtime.error]
class NTL_EXPORTAPI runtime_error : public exception
{
  public:
    explicit runtime_error(const string& what_arg) : msg(what_arg) {}
    explicit runtime_error(const char* what_arg )  : msg(what_arg) {}
    virtual const char* what() const __ntl_nothrow { return msg.c_str(); }
    ~runtime_error() /*__ntl_nothrow*/ {}
  private:
    const __::exstring msg;
  //protected:
    runtime_error operator=(const runtime_error&) {/* do not use */ return *this; }
};

/// 19.2.7 Class range_error [lib.range.error]
class NTL_EXPORTAPI range_error : public runtime_error
{
  public:
    explicit range_error(const string& what_arg) : runtime_error(what_arg) {}
    explicit range_error(const char* what_arg )  : runtime_error(what_arg) {}
};

/// 19.2.8 Class overflow_error [lib.overflow.error]
class NTL_EXPORTAPI overflow_error : public runtime_error
{
  public:
    explicit overflow_error(const string& what_arg) : runtime_error(what_arg) {}
    explicit overflow_error(const char* what_arg )  : runtime_error(what_arg) {}
};

/// 19.2.9 Class underflow_error [lib.underflow.error]
class NTL_EXPORTAPI underflow_error : public runtime_error
{
  public:
    explicit underflow_error(const string& what_arg) : runtime_error(what_arg) {}
    explicit underflow_error(const char* what_arg )  : runtime_error(what_arg) {}
};

//////////////////////////////////////////////////////////////////////////
#if STLX_USE_EXCEPTIONS
///\name exception helpers
void __throw_out_of_range(const char* msg)      { __ntl_throw(out_of_range(msg)); }
void __throw_length_error(const char* msg)      { __ntl_throw(length_error(msg)); }
void __throw_invalid_argument(const char* msg)  { __ntl_throw(invalid_argument(msg)); }
} // std

#include "cstring.hxx"

namespace std {

__::exstring::exstring(const std::__::exstring& r)
{
  if(r.msg){
    len = r.len;
    if(!len)
      len = strlen(r.msg);
    msg = new char[len+1];
    char* const dest = const_cast<char*>(msg);
    strncpy(dest, r.msg, len);
    dest[len] = 0;
  }else{
    msg = 0, len = 0;
  }
}

// it should be always copyable due to such cases:
// runtime_error err ( string(...).c_str() ); throw err;
// 
__::exstring::exstring(const char* r)
{
  if(r){
    len = strlen(r);
    msg = new char[len+1];
    char* const dest = const_cast<char*>(msg);
    strncpy(dest, r, len);
    dest[len] = 0;
  }else{
    msg = 0, len = 0;
  }
}

#else

///\warnong Avoid unresolved external when compiling with /EH 
//__::exstring::exstring(const std::string& s){ assert(s.c_str()); }

#endif
///\}

/**@} std_exceptions */
/**@} lib_diagnostics */
}//namespace std
#endif//#ifndef NTL__STLX_STDEXCEPT
