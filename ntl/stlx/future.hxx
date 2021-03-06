/**\file*********************************************************************
 *                                                                     \brief
 *  30.5 Futures [futures]
 *
 ****************************************************************************
 */
#ifndef NTL__STLX_FUTURE
#define NTL__STLX_FUTURE
#pragma once

#include "system_error.hxx"
#include "chrono.hxx"
#include "tuple.hxx"
#include "function.hxx"
#include "mutex.hxx"
#include "atomic.hxx"
#include "stdexception.hxx"
#include "exception2.hxx"
#include "system_error.hxx"
#include "smart_ptr_rv.hxx"
#include "../nt/event.hxx"
#include "thread.hxx"

namespace std
{
  /**
   *	@addtogroup threads 30 Thread support library [thread]
   *  @{
   *
   *	@defgroup thread_futures 30.5 Futures [futures]
   *
   *  Describes components that a C++ program can use to retrieve in one %thread the result
   *  (value or exception) from a function that has run in another %thread.
   *
   *  @{
   **/
#if 0
  template <class R> class future;
  template <class R> class future<R&>;
  template <>        class future<void>;
  
  template <class R> class shared_future;
  template <class R> class shared_future<R&>;
  template <>        class shared_future<void>;
  
  template <class R> class promise;
  template <class R> class promise<R&>;
  template <>        class promise<void>;
#else

  template <class R> class future;
  template <class R> class shared_future;
  template <class R> class promise;
#endif

  template <class> class packaged_task; // undefined

  ///\name 30.5.2 Error handling [futures.errors]
#if defined(NTL_CXX_ENUM) || defined(NTL_DOC)
  /// Futures error codes
  enum class future_errc {
    /** Indicates that promise is broken */
    broken_promise,
    /** Indicates that future value already retrieved from promise or packaged_task */
    future_already_retrieved,
    /** Indicates that the promise already have the associated state */
    promise_already_satisfied,
    /** Uninitialized future use */
    no_state
  #ifndef NTL_DOC
    ,__maximum_errc
  #endif
  };

  enum class launch {
    async     = 0x01,
    deferred  = 0x02,
  };

  enum class future_status {
    ready,
    timeout,
    deferred
  };

#else
  __class_enum(future_errc)
  {
    broken_promise,
    future_already_retrieved,
    promise_already_satisfied,
    no_state,
    __maximum_errc
  };};

  __class_enum(launch) {
    async     = 0x01,
    deferred  = 0x02,
  };};
  __ntl_bitmask_type(launch::type,);

  __class_enum(future_status) {
    ready,
    timeout,
    deferred
  };};
#endif

  /** The is_error_code_enum template specialization to indicate that such a type is eligible 
    for class error_code automatic */
  template <> struct is_error_code_enum<future_errc>: true_type {};

  /**
   *	30.5.3 Class future_error [futures.future_error]
   **/
  class future_error: 
    public logic_error
  {
    error_code ec;
    mutable string msg;
  public:
    future_error(error_code ec)
      :logic_error("future_error"), ec(ec)
    {}

    const error_code& code() const __ntl_nothrow { return ec; }
    const char* what() const __ntl_nothrow
    {
      if (msg.empty()) {
        __ntl_try
        {
          string tmp = logic_error::what();
          if (ec) {
            if (!tmp.empty() && tmp != "")
              tmp += ": ";
            tmp += ec.message();
          }
          msg = move(tmp);
        }
        #pragma warning(push)
        #pragma warning(disable:4127)
        __ntl_catch(...) {
          return logic_error::what();
        }
        #pragma warning(pop)
      }
      return msg.c_str();
    }
  };

  /** future_category points to a statically initialized object of a type derived from class error_category. */
  const error_category& future_category();

  inline constexpr error_code make_error_code(future_errc e)
  {
    return error_code(static_cast<int>(e), future_category());
  }

  inline constexpr error_condition make_error_condition(future_errc e)
  {
    return error_condition(static_cast<int>(e), future_category());
  }

  ///\name 30.5.7 Allocator templates [futures.allocators]

  /** Specialization of this trait informs other library components that promise can be constructed
  with an allocator, even though it does not have an \c allocator_type associated type. */
  template <class R, class Alloc>
  struct uses_allocator<promise<R>, Alloc>: true_type {};

  ///\}

  namespace __
  {
    class future_error_category:
      public error_category
    {
    public:
      future_error_category()
      {}

      const char* name() const { return "future"; }
      error_condition default_error_condition(int ev) const __ntl_nothrow { return error_condition(ev, *this); }
      bool equivalent(int code, const error_condition& condition) const __ntl_nothrow
      {
        return default_error_condition(code) == condition;
      }
      bool equivalent(const error_code& code, int condition) const __ntl_nothrow
      {
        return *this == code.category() && code.value() == condition;
      }

      virtual string message(int ev) const
      {
        static const char* const strings[] = 
        {
          "broken_promise",
          "future_already_retrieved",
          "promise_already_satisfied",
          "future_uninitialized"
        };
        static_assert(_countof(strings) == static_cast<size_t>(future_errc::__maximum_errc), "disparity between future_errc values and its string representation");
        return strings[ev];
      }
    };
  }

  inline const error_category& future_category()
  {
    static const __::future_error_category cat;
    return cat;
  }

  namespace __
  {
    template<typename R>
    struct future_result
    {
      typedef typename conditional<is_void<R>::value, type2type<void>,
        typename conditional<is_lvalue_reference<R>::value, type2type<R>, add_rvalue_reference<R> >::type
                                  >::type  weak_type;
      typedef typename weak_type::type type;
      typedef typename conditional<is_void<R>::value, type2type<void>, add_lvalue_reference<R> >::type::type rtype;
    };

    struct future_base
    {
      typedef lock_guard<mutex> lock_guard;
      mutable mutex mtx;
      ntl::nt::user_event event;
      ntl::atomic_t refcount;
      ntl::atomic::flag_t ready;
      
      exception_ptr exception;
      error_code error;

      future_base()
        :refcount(1), event(ntl::nt::NotificationEvent)
      {}
      virtual ~future_base()
      {}

      bool has_exception() const
      {
        lock_guard g(mtx);
        return exception;
      }

      bool has_error() const
      {
        lock_guard g(mtx);
        return static_cast<bool>(error);
      }

      void set_exception(exception_ptr ep)
      {
        lock_guard g(mtx);
        if(ready)
          __ntl_throw(future_error(make_error_code(future_errc::promise_already_satisfied)));
        exception = ep;
        mark_ready();
      }

      void set_error(const error_code& code, error_code& ec = throws())
      {
        lock_guard g(mtx);
        if(ready){
          error_code e = make_error_code(future_errc::promise_already_satisfied);
          if(&ec == &throws())
            __ntl_throw(future_error(e));
          else
            ec = e;
        }
        error = code;
        mark_ready();
      }

      void mark_ready()
      {
        //lock_guard g(data_guard);
        ready = true;
        event.set();
      }

      void mark_broken()
      {
        lock_guard g(mtx);
        if(!ready){
          error = make_error_code(future_errc::broken_promise);
        #if STLX_USE_EXCEPTIONS == 1
          exception = make_exception_ptr(future_error(error));
        #endif
          mark_ready();
        }
      }

      bool is_retrieved() const
      {
        // reset event to nonsignaled state 
        lock_guard g(mtx);
        return ready && !event.is_ready();
      }

      void wait() const
      {
        if(!ready)
          event.wait();
      }

      template <class Rep, class Period>
      future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const
      {
        if(ready)
          return future_status::ready;
        return ntl::nt::success(event.wait_for(rel_time)) ? future_status::ready : future_status::timeout;
      }

      template <class Clock, class Duration>
      bool wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const
      {
        if(ready)
          return future_status::ready;
        return ntl::nt::success(event.wait_until(abs_time))? future_status::ready : future_status::timeout;
      }

      virtual void dispose()
      {
        delete this;
      }
    };


    template<typename T>
    struct future_data:
      future_base
    {
      typename aligned_storage<sizeof(T), alignof(T)>::type data_buffer;
      typedef typename future_result<T>::type param_type;
      typedef typename future_result<T>::rtype result_type;

      T* data() { return reinterpret_cast<T*>(&data_buffer); }

      ~future_data()
      {
        lock_guard g(mtx);
        if(ready && !exception){
          data()->~T();
        }
      }

      result_type get(error_code& ec)
      {
        lock_guard g(mtx);
        if(error){
          if(&ec == &throws())
            __ntl_throw(future_error(error));
          else
            ec = error;
        }
        if(exception)
          rethrow_exception(exception);
        return /*std::move*/(*data());
      }

      void set(const T& value, error_code& ec)
      {
        lock_guard g(mtx);
        if(ready){
          error = make_error_code(future_errc::promise_already_satisfied);
          if(&ec == &throws())
            __ntl_throw(future_error(error));
          else
            ec = error;
          return;
        }
        new (data()) T(value);
        mark_ready();
      }

      void set(param_type value, error_code& ec)
      {
        lock_guard g(mtx);
        if(ready){
          error = make_error_code(future_errc::promise_already_satisfied);
          if(&ec == &throws())
            __ntl_throw(future_error(error));
          else
            ec = error;
          return;
        }
        new (data()) T(value);
        mark_ready();
      }
    };

    template<typename T>
    struct future_data<T&>:
      future_data<T*>
    {
      T& get(error_code& ec) { return *future_data<T*>::get(ec); }
      void set(T& value, error_code& ec) { future_data<T*>::set(&value, ec); }
    };

    template<>
    struct future_data<void>:
      future_base
    {
      void get(error_code& ec)
      {
        lock_guard g(mtx);
        if(error){
          if(&ec == &throws())
            __ntl_throw(future_error(error));
          else
            ec = error;
        }
        if(exception)
          rethrow_exception(exception);
      }

      void set(error_code& ec)
      {
        lock_guard g(mtx);
        if(ready){
          error = make_error_code(future_errc::promise_already_satisfied);
          if(&ec == &throws())
            __ntl_throw(future_error(error));
          else
            ec = error;
          return;
        }
        mark_ready();
      }
    };

    template<class R> class promise;
    template<class R, class Args> class packaged_task;
  }

  /**
   *	@brief 30.5.4 Class template future [futures.future]
   **/
  template <class R>
  class future 
  {
    typedef typename __::future_result<R>::rtype result_type;
    typedef shared_ptr< __::future_data<R> > data_ptr;
    future(const future& rhs) __deleted;
    future& operator=(const future& rhs) __deleted;

    friend class shared_future<R>;
  public:

    /** Constructs an empty future object that does not refer to an shared state. */
    future()
    {}

    /** Move constructs a future object whose associated state is the same as the state of \p x before. */
    future(__rvalue(future) x)
      :data(move(static_cast<future&>(x).data))
    {}

    /** Destroys \c *this and its associated state if no other object refers to that. */
    ~future()
    {}

    /** Moves the future object. */
    future& operator=(__rvalue(future) x)
    {
      data = move(x.data);
      return *this;
    }

    /** Transfers the shared state from \c *this to a shared_future and returns it. */
    shared_future<R> share(error_code& ec = throws())
    {
      if(!data) {
        error_code e = make_error_code(future_errc::no_state);
        if(&ec == &throws())
          __ntl_throw(future_error(e));
        else
          ec = e;
      }
      return shared_future<R>(std::move(*this));
    }

    /** Returns a value stored in the asynchronous result.
        @note The effect of calling get() a second time on the same future object is unspecified. */
    result_type get(error_code& ec)
    {
      wait(ec);
      return data->get(ec);
    }

    result_type get()
    {
      wait(throws());
      return data->get(throws());
    }

    ///\name functions to check state and wait for ready
    
    /** Returns \c true only if \c *this refers to a shared state. */
    bool valid() const { return data; }

    /** Returns \c true only if the associated state holds a value or an exception ready for retrieval.
        @note the return value is unspecified after a call to get(). */
    bool is_ready() const { return data && data->ready; }

    /** Returns \c true only if result is ready and the associated state contains an exception. */
    bool has_exception() const { return is_ready() && data->has_exception(); }

    /** Returns \c true only if result is ready and the associated state contains an error. */
    bool has_error() const { return is_ready() && data->has_error(); }

    /** Returns \c true only if result is ready and the associated state contains a value. */
    bool has_value() const { return is_ready() && !data->has_exception() && !data->has_error(); }
    
    /** Blocks the current %thread until the result is ready. */
    void wait(error_code& ec = throws()) const
    {
      if(!check(ec))
        return;
      data->wait();
    }
    
    /** Blocks the current %thread until the result is ready or until \c rel_time has elapsed. */
    template <class Rep, class Period>
    bool wait_for(const chrono::duration<Rep, Period>& rel_time, error_code& ec = throws()) const
    {
      if(!check(ec))
        return false;
      return data->wait_for(rel_time);
    }
    
    /** Same as wait_for(), except that it blocks until \c abs_time is reached if the associated state is not ready. */
    template <class Clock, class Duration>
    bool wait_until(const chrono::time_point<Clock, Duration>& abs_time, error_code& ec = throws()) const
    {
      if(!check(ec))
        return false;
      return data->wait_until(abs_time);
    }
    ///\}

  protected:
    friend class __::promise<R>;
    ///\cond __
    explicit future(data_ptr p)
      :data(p)
    {}

    bool check(error_code& ec) const
    {
      if(!data){
        const error_code error = make_error_code(future_errc::no_state);
        if(&ec == &throws())
          __ntl_throw(future_error(error));
        else
          ec = error;
      }
      return !!data;
    }
    ///\endcond
  private:
    data_ptr data;
  };


  /**
   *	@brief 30.5.5 Class template shared_future [future.shared_future]
   **/
  template <class R>
  class shared_future
  {
    typedef typename __::future_result<R>::rtype rt0;
    typedef typename conditional<is_void<R>::value||is_reference<R>::value,rt0,typename add_const<rt0>::type>::type result_type;

  public:
    /** Constructs an empty future object that does not refer to an shared state. */
    shared_future()
    {}

    /** Constructs a shared_future object that refers to the same shared state as \c x (if any). */
    shared_future(const shared_future& x)
      : data(x.data)
    {}

    /** Move constructs a shared_future object that refers to the shared state that was originally referred to by \c x (if any). */
    shared_future(__rvalue(future<R>) x)
      : data(move(static_cast<future<R>&>(x).data))
    {}

    /** Move constructs a shared_future object that refers to the shared state that was originally referred to by \c x (if any). */
    shared_future(__rvalue(shared_future<R>) x)
      : data(move(static_cast<shared_future<R>&>(x).data))
    {}

    /** Releases any shared state */
    ~shared_future()
    {}

    /** Assigns the contents of another shared_future. */
    shared_future& operator=(const shared_future& x)
    {
      data = x.data;
      return *this;
    }

    /** Assigns the contents of another shared_future. */
    shared_future& operator=(__rvalue(shared_future) x)
    {
      data = move(static_cast<shared_future&>(x).data);
      return *this;
    }

    /** Returns a value stored in the asynchronous result. */
    result_type get(error_code& ec = throws()) const
    {
      wait(ec);
      return data->get(ec);
    }
    
    ///\name functions to check state and wait for ready

    /** Returns \c true only if \c *this refers to a shared state. */
    bool valid() const { return data; }

    /** Returns \c true only if the associated state holds a value or an exception ready for retrieval.
        @note the return value is unspecified after a call to get(). */
    bool is_ready() const { return data && data->ready; }
    
    /** Returns \c true only if result is ready and the associated state contains an exception. */
    bool has_exception() const { return is_ready() && data->has_exception(); }

    /** Returns \c true only if result is ready and the associated state contains an error. */
    bool has_error() const { return is_ready() && data->has_error(); }

    /** Returns \c true only if result is ready and the associated state contains a value. */
    bool has_value() const { return is_ready() && !data->has_exception() && !data->has_error(); }
    
    /** Blocks the current %thread until the result is ready. */
    void wait(error_code& ec = throws()) const
    {
      if(!check(ec))
        return;
      data->wait();
    }
    
    /** Blocks the current %thread until the result is ready or until \c rel_time has elapsed. */
    template <class Rep, class Period>
    bool wait_for(const chrono::duration<Rep, Period>& rel_time, error_code& ec = throws()) const
    {
      if(!check(ec))
        return false;
      return data->wait_for(rel_time);
    }
    
    /** Same as wait_for(), except that it blocks until \c abs_time is reached if the associated state is not ready. */
    template <class Clock, class Duration>
    bool wait_until(const chrono::time_point<Clock, Duration>& abs_time, error_code& ec = throws()) const
    {
      if(!check(ec))
        return false;
      return data->wait_until(abs_time);
    }
    ///\}
  protected:
    ///\cond __
    bool check(error_code& ec) const
    {
      if(!data){
        const error_code error = make_error_code(future_errc::no_state);
        if(&ec == &throws())
          __ntl_throw(future_error(error));
        else
          ec = error;
      }
      return data;
    }
    ///\endcond
  private:
    shared_ptr<__::future_data<R> > data;
  };

  namespace __
  {
    template <class R>
    class promise
    {
      typedef __::future_result<R> feature_result;
      typedef shared_ptr<__::future_data<R> > data_ptr;

    public:
      typedef typename feature_result::type result_type;

    public:
      promise(const promise& rhs) __deleted;
      promise & operator=(const promise& rhs) __deleted;

      promise()
      {}
      promise(__rvalue(promise) x)
        : data(move(x.data))
        , retrieved(move(x.retrieved))
        , satisfied(move(x.satisfied))
      {}

      template <class Allocator>
      promise(allocator_arg_t, const Allocator& a);
      template <class Allocator>
      promise(allocator_arg_t, const Allocator& a, promise& rhs);

      /**
       *	@brief promise destructor
       *  @details Destroys \c *this and its associated state if no other object refers to it. 
       *  If another object refers to the associated state of \c *this and that state is not ready, 
       *  sets that state to ready and stores a future_error %exception with error code \c broken_promise as result.
       **/
      ~promise() __ntl_throws(future_error)
      {
        if(data)
          data->mark_broken();
      }

      ///\name assignment
      promise& operator=(__rvalue(promise) x)
      {
        if(this != &x) {
          data = move(x.data);
          retrieved = move(x.retrieved);
          satisfied = move(x.safisfied);
        }
        return *this;
      }

      void swap(promise& x)
      {
        if(this == &x)
          return;
        using std::swap;
        swap(data, x.data);
        swap(retrieved, x.retrieved);
        swap(satisfied, x.satisfied);
      }
      
      ///\name retrieving the result
      future<R> get_future(error_code& ec = throws()) __ntl_throws(future_error)
      {
        if(retrieved.test_and_set()) {
          error_code e = make_error_code(future_errc::future_already_retrieved);
          if(&ec == &throws())
            __ntl_throw(future_error(e));
          else
            ec = e;
          return future<R>(data_ptr());
        }
        check();
        return future<R>(data);
      }
      
      ///\name setting the result
      void set_exception(exception_ptr p)
      {
        std::error_code ec;
        if(accept(ec)) {
          check();
          data->set_exception(p);
        }
      }

      void set_error(const error_code& code, error_code& c = throws())
      {
        if(accept(c)) {
          check();
          data->set_error(code, c);
        }
      }
      ///\}
    protected:
      ///\cond __
      void check()
      {
        if(!data)
          data.reset(new __::future_data<R>);
      }

      bool accept(error_code& ec)
      {
        if(!satisfied.test_and_set())
          return true;

        error_code e = make_error_code(future_errc::promise_already_satisfied);
        if(&ec == &throws())
          __ntl_throw(future_error(e));
        else
          ec = e;
        return false;
      }

    protected:
      data_ptr data;
      ntl::atomic::flag_t retrieved, satisfied;
      ///\endcond
    };
  }

  /**
   *	@class __::promise
   *  @brief 30.5.6 Class template promise [futures.promise] implementation
   *  @details This is an alternative means of creating \e futures without a callable object - the value of type \c R to be returned 
   *  by the associated \e futures is specified directly by invoking the \c set_value() member function. 
   *  Alternatively, the %exception to be returned can be specified by invoking the \c set_exception() member function. 
   *  This allows the result to be %set from a callback invoked from code that has no knowledge of the promise or its associated \e futures.
   **/

  /**	30.5.6 Class template promise, default interface [futures.promise] */
  template<class R> class promise:
    public __::promise<R>
  {
  public:
    /** Stores \c r in the associated state and sets that state to ready. Any blocking waits on the
      associated state are woken up. */
    void set_value(const R& r, error_code& ec = throws()) __ntl_throws(future_error)
    {
      if(accept(ec)) {
        check();
        data->set(r, ec);
      }
    }
    ///\copydoc set_value
    void set_value(__rvalue(R) r, error_code& ec = throws()) __ntl_throws(future_error)
    {
      if(accept(ec)) {
        check();
        data->set(forward<R>(r), ec);
      }
    }

    /** Stores the value \c r in the shared state without making that state ready immediately. */
    //void set_value_at_thread_exit(const R& r, error_code& ec = throws()) __ntl_throws(future_error);

    /** Stores the value \c r in the shared state without making that state ready immediately. */
    //void set_value_at_thread_exit(__rvalue(R) r, error_code& ec = throws()) __ntl_throws(future_error);
  };

  /** promise class template specialization for reference to the result */
  template<class R> class promise<R&>:
    public __::promise<R>
  {
  public:
    /** Stores \c r in the associated state and sets that state to ready. Any blocking waits on the
      associated state are woken up. */
    void set_value(R& r, error_code& ec = throws()) __ntl_throws(future_error)
    {
      if(accept(ec)) {
        check();
        data->set(r, ec);
      }
    }

    /** Stores the value \c r in the shared state without making that state ready immediately. */
    //void set_value_at_thread_exit(R& r, error_code& ec = throws()) __ntl_throws(future_error);
  };

  /** promise class template specialization for \c void result type */
  template<> class promise<void>:
    public __::promise<void>
  {
  public:
    /** Sets that state to ready. Any blocking waits on the associated state are woken up. */
    void set_value(error_code& ec = throws()) __ntl_throws(future_error)
    {
      if(accept(ec)) {
        check();
        data->set(ec);
      }
    }

    /** Stores the value in the shared state without making that state ready immediately. */
    void set_value_at_thread_exit() __ntl_throws(future_error);
  };

  template<class R>
  inline void swap(promise<R>& x, promise<R>& y)
  {
    x.swap(y);
  }


  namespace __
  {
    template<class R, class Args = tuple<> >
    class packaged_task
    {
      typedef shared_ptr< __::future_data<R> > data_ptr;

    public:
      typedef R result_type;

      ///\name construction and destruction
      packaged_task()
      {}

      ~packaged_task() __ntl_throws(future_error)
      {} // ~promise() throws(broken_future)


      packaged_task(const packaged_task&) __deleted;
      packaged_task& operator=(const packaged_task&) __deleted;


      ///\name move support
      packaged_task(__rvalue(packaged_task) x)
        :data(move(x.data)), f(move(x.f))
      {} // TODO: must be atomic

      packaged_task& operator=(__rvalue(packaged_task) x)
      {
        if(this != &x){
          data = move(x.data);
          f = move(x.f);
        }
        return *this;
      }
      ///\}

      void swap(packaged_task& x)
      {
        // TODO: must be atomic
        using std::swap;
        swap(x.data);
        swap(data.f);
      }

      /** Returns \c true only if *this has an associated task. */
      __explicit_operator_bool() const
      {
        return static_cast<bool>(f);
      }

      /** Checks if the task object has a valid function. */
      bool valid() const { return static_cast<bool>(f); }

      // result retrieval
      /** Returns a future object associated with the result of the associated task of *this. */
#ifdef NTL_CXX_RV
      future<R>
#else
      _rvalue<future<R> >
#endif
        get_future(error_code& ec = throws()) __ntl_throws(future_error)
      {
        error_code e;
        future<R> r = data.get_future(e);
        if(e){
          if(&ec == &throws())
            __ntl_throw(future_error(e));
          else
            ec = e;
        }
        return move(r);
      }

      /** Resets the state abandoning any stored results of previous executions. */
      void reset()
      {
        promise<R> tmp;
        tmp.swap(data);
      }

    protected:
      ///\cond __
      void call(const Args& args)
      {
        //thread t(&packaged_task::call_thunk, this, args);
        callee(args, is_void<R>());
      }

      void call_thunk(const Args& args)
      {
        callee(args, is_void<R>());
      }

      void callee(const Args& args, false_type)
      {
        data.set_value(f(args));
      }

      void callee(const Args& args, true_type)
      {
        f(args);
        data.set_value();
      }

    protected:
      func::detail::function<R, Args> f;
      ///\endcond

    private:
      std::promise<R> data;
    };
  }
  
  /**
   *  @class __::packaged_task
   *	@brief 30.5.8 Class template packaged_task [futures.task] implementation
   *  @details This is a wrapper for a callable object with a return type of \c R. A packaged_task is itself a callable type - invoking it's function call operator 
   *  will invoke the encapsulated callable object. This still doesn't get you the value returned by the encapsulated object, though - to do that you need a 
   *  \c future<R> which you can obtain using the \c get_future() member function. 
   *
   *  Once the function call operator has been invoked, all futures associated with a particular packaged_task will become \e ready, 
   *  and will store either the value returned by the invocation of the encapsulated object, or the exception thrown by said invocation. 
   *
   *  packaged_task is intended to be used in cases where the user needs to build a %thread pool or similar structure because the standard facilities do not have the desired characteristics.
   **/

#ifdef NTL_CXX_VT

  template<class R, class... Args>
  class packaged_task<R(Args...)>:
    public __::packaged_task<R, NTL_FUNARGS(Args...)>
  {
  public:
    packaged_task()
    {}
    template <class F>
    explicit packaged_task(F&& f)
    {
      this->f = forward<F>(f);
    }

    inline void operator()(Args... args)
    {
      call(make_tuple(args...));
    }

    void mark_ready_at_thread_exit(Args... args);
  };

  template<class F, class... Args>
  inline void swap(packaged_task<F(Args...)>& x, packaged_task<F(Args...)>& y)
  {
    x.swap(y);
  }

#else

  /** packaged_task class template specialization for callable without arguments */
  template<class R>
  class packaged_task<R()>:
    public __::packaged_task<R>
  {
  public:
    packaged_task()
    {}
    template <class F>
    explicit packaged_task(F f)
    {this->f = forward<F>(f);}
    explicit packaged_task(R(*f)())
    {
      this->f = move(f);
    }
    template <class F>
    explicit packaged_task(__rvalue(F) f)
    {this->f = forward<F>(f);}

    inline void operator()()
    {
      call(tuple<>());
    }
  };

  /** packaged_task class template specialization for callable with one argument */
  template<class R, class A1>
  class packaged_task<R(A1)>:
    public __::packaged_task<R,NTL_FUNARGS(A1)>
  {
  public:
    packaged_task()
    {}
    template <class F>
    explicit packaged_task(F f)
    {this->f = forward<F>(f);}
    explicit packaged_task(R(*f)())
    {this->f = move(f);}
    template <class F>
    explicit packaged_task(__rvalue(F) f)
    {this->f = forward<F>(f);}

    void operator()(A1 a1)
    {
      call(make_tuple(a1));
    }
  };

  /** packaged_task class template specialization for callable with two arguments */
  template<class R, class A1, class A2>
  class packaged_task<R(A1,A2)>:
    public __::packaged_task<R,NTL_FUNARGS(A1,A2)>
  {
  public:
    packaged_task()
    {}
    template <class F>
    explicit packaged_task(F f)
    {this->f = forward<F>(f);}
    explicit packaged_task(R(*f)())
    {this->f = move(f);}
    template <class F>
    explicit packaged_task(__rvalue(F) f)
    {this->f = forward<F>(f);}
    void operator()(A1 a1, A2 a2)
    {
      call(make_tuple(a1,a2));
    }
  };

#endif // NTL_CXX_VT

  ///\name 30.6.8 Function template async [futures.async]
#if defined(NTL_CXX_VT) || defined(NTL_DOC)

  template <class F, class... Args>
  inline 
    future<typename result_of<typename decay<F>::type(typename decay<Args>::type...)>::type>
    async(F&& f, Args&&... args)
  {
    return async(launch::async|launch::deferred, __::decay_copy(f), std::forward<Args>(args)...);
  }

  template <class F, class... Args>
  inline 
    future<typename result_of<typename decay<F>::type(typename decay<Args>::type...)>::type>
    async(launch policy, F&& f, Args&&... args);

#elif defined(NTL_CXX_RV)

  template <class F>
  future<typename result_of<F()>::type> async(launch policy, F&& f);

#else

  template <class F>
  future<typename result_of<F()>::type> async(launch policy, F f);

  template <class F>
  typename enable_if<!is_same<F,launch>::value,future<typename result_of<F()>::type> >::type async(F f)
  {
    return async(launch::async|launch::deferred, f);
  }

#endif
  ///\}

  /** @} thread_futures */
  /** @} threads */
} // std
#endif // NTL__STLX_FUTURE
