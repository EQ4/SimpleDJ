/*============================================================================*/
/*
  Copyright (C) 2008 by Vinnie Falco, this file is part of VFLib.
  See the file GNU_GPL_v2.txt for full licensing terms.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 51
  Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
/*============================================================================*/

#ifndef VF_CALLQUEUE_VFHEADER
#define VF_CALLQUEUE_VFHEADER

/*============================================================================*/
/**
  @ingroup vf_concurrent

  @brief A FIFO for calling functors asynchronously.

  This object is an alternative to traditional locking techniques used to
  implement concurrent systems. Instead of acquiring a mutex to change shared
  data, a functor is queued for later execution (usually on another thread). The
  execution of the functor applies the transformation to the shared state that
  was formerly performed within a lock (i.e. CriticalSection).

  For read operations on shared data, instead of acquiring a mutex and
  accessing the data directly, copies are made (one for each thread), and the
  thread accesses its copy without acquiring a lock. One thread owns the master
  copy of the shared state. Requests for changing shared state are made by other
  threads by posting functors to the master thread's CallQueue. The master
  thread notifies other threads of changes by posting functors to their
  respective associated CallQueue, using the Listeners interface.

  The purpose of the functor is to encapsulate one mutation of shared state to
  guarantee progress towards a consensus of the concurrent data among
  participating threads. Functors should execute quickly, ideally in constant
  time. Dynamically allocated objects of class type passed as functor parameters
  should, in general, be reference counted. The ConcurrentObject class is ideal
  for meeting this requirement, and has the additional benefit that the workload
  of deletion is performed on a separate, provided thread. This queue is not a
  replacement for a thread pool or job queue type system.

  A CallQueue is considered signaled when one or more functors are present.
  Functors are executed during a call to synchronize(). The operation of
  executing functors via the call to synchronize() is called synchronizing
  the queue. It can more generally be thought of as synchronizing multiple
  copies of shared data between threads.

  Although there is some extra work required to set up and maintain this
  system, the benefits are significant. Since shared data is only synchronized
  at well defined times, the programmer can reason and make strong statements
  about the correctness of the concurrent system. For example, if an
  AudioIODeviceCallback synchronizes the CallQueue only at the beginning of its
  execution, it is guaranteed that shared data will remain the same throughout
  the remainder of the function.

  Because shared data is accessed for reading without a lock, upper bounds
  on the run time performance can easily be calculated and assured. Compare
  this with the use of a mutex - the run time performance experiences a
  combinatorial explosion of possibilities depending on the complex interaction
  of multiple threads.

  Since a CallQueue is almost always used to invoke parameterized member
  functions of objects, the call() function comes in a variety of convenient
  forms to make usage easy:

  @code

  void func1 (int);

  struct Object
  {
    void func2 (void);
    void func3 (String name);

    static void func4 ();
  };

  CallQueue fifo ("Example");

  void example ()
  {
    fifo.call (func1, 42);               // same as: func1 (42)

    Object* object = new Object;

    fifo.call (&Object::func2, object);  // same as: object->func2 ()

    fifo.call (&Object::func3,           // same as: object->funcf ("Label")
                object,
                "Label");

    fifo.call (&Object::func4);          // even static members can be called.

    fifo.callf (bind (&Object::func2,    // same as: object->func2 ()
                      object));
  }

  @endcode

  @invariant Functors can be added from any thread at any time, to any queue
              which is not closed.

  @invariant When synchronize() is called, functors are called and deleted.

  @invariant The thread from which synchronize() is called is considered the
              thread associated with the CallQueue.

  @invariant Functors queued by the same thread always execute in the same
              order they were queued.

  @invariant Functors are guaranteed to execute. It is an error if the
              CallQueue is deleted while there are functors in it.

  Normally, you will not use CallQueue directly, but one of its subclasses
  instead. The CallQueue is one of a handful of objects that work together to
  implement this system of concurrent data access.

  For performance considerations, this implementation is wait-free for
  producers and mostly wait-free for consumers. It also uses a lock-free
  and wait-free (in the fast path) custom memory allocator.

  @see GuiCallQueue, ManualCallQueue, MessageThread

  @todo Standardize terminology: "functor" to represent the stored item.
*/
class CallQueue
{
public:
  //============================================================================

  typedef FifoFreeStoreType AllocatorType;

  /** @internal
      @ingroup vf_concurrent internal

      @brief Abstract nullary functor.
  */
  class Call : public LockFreeQueue <Call>::Node,
               public AllocatedBy <AllocatorType>
  {
  public:
    virtual ~Call () { }
    virtual void operator() () = 0;
  };

  //============================================================================

  /** @param  name  A string to identify the queue during debugging.
  */
  explicit CallQueue (String name);

  /** @details
  
      @invariant Destroying a queue that contains functors is undefined.
  */
  ~CallQueue ();

  //============================================================================

  /** Add a functor and possibly synchronize.

      Use this when you want to perform the bind yourself.

      @param f The functor to add, typically the return value of a call
               to bind().

      @see call
  */
  template <class Functor>
  void callf (Functor const& f)
  {
    callp (new (m_allocator) CallType <Functor> (f));
  }

/**
  Add a function call and possibly synchronize.

  Parameters are evaluated immediately and added to the queue as a packaged
  functor. If the current thread of execution is the same as the thread
  associated with the CallQueue, synchronize() is called automatically. This
  behavior can be avoided by using queue() instead.

  @param f The function to call followed by up to eight parameters, evaluated
            immediately. The parameter list must match the function signature.
            For class member functions, the first argument must be a pointer
            to the class object.

  @see queue

  @todo Provide an example of when synchronize() is needed in call().
*/
  template <class Fn>
  void call (Fn f)
  { callf (vf::bind (f)); }

  template <class Fn, typename  T1>
  void call (Fn f,    const T1& t1)
  { callf (vf::bind (f, t1)); }

  template <class Fn, typename  T1, typename  T2>
  void call (Fn f,    const T1& t1, const T2& t2)
  { callf (vf::bind (f, t1, t2)); }

  template <class Fn, typename  T1, typename  T2, typename  T3>
  void call (Fn f,    const T1& t1, const T2& t2, const T3& t3)
  { callf (vf::bind (f, t1, t2, t3)); }

  template <class Fn, typename  T1, typename  T2,
                      typename  T3, typename  T4>
  void call (Fn f,    const T1& t1, const T2& t2,
                      const T3& t3, const T4& t4)
  { callf (vf::bind (f, t1, t2, t3, t4)); }

  template <class Fn, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5>
  void call (Fn f,    const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5)
  { callf (vf::bind (f, t1, t2, t3, t4, t5)); }

  template <class Fn, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5, typename  T6>
  void call (Fn f,    const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5, const T6& t6)
  { callf (vf::bind (f, t1, t2, t3, t4, t5, t6)); }

  template <class Fn, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7>
  void call (Fn f,    const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7)
  { callf (vf::bind (f, t1, t2, t3, t4, t5, t6, t7)); }

  template <class Fn, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7, typename  T8>
  void call (Fn f,    const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7, const T8& t8)
  { callf (vf::bind (f, t1, t2, t3, t4, t5, t6, t7, t8)); }

/**
  Add a functor without synchronizing.

  Use this when you want to perform the bind yourself.

  @param f The functor to add, typically the return value of a call
            to bind().

  @see queue
*/
  template <class Functor>
  void queuef (Functor const& f)
  {
    queuep (new (m_allocator) CallType <Functor> (f));
  }

/**
  Add a function call without synchronizing.

  Parameters are evaluated immediately, then the resulting functor is added
  to the queue. This is used to postpone the call to synchronize() when
  there would be adverse side effects to executing the function immediately.
  In this example, we use queue() instead of call() to avoid a deadlock:

  @code

  struct SharedState;           // contains data shared between threads

  ConcurrentState <SharedState> sharedState;

  void stateChanged ()
  {
    ConcurrentState <SharedState>::ReadAccess state (sharedState);

    // (read state)
  }

  CallQueue fifo;

  void changeState ()
  {
    ConcurrentState <State>::WriteAccess state (sharedState);

    // (read and write state)

    fifo.call (&stateChanged);  // BUG: DEADLOCK because of the implicit synchronize().

    fifo.queue (&stateChanged); // Okay, synchronize() will be called later,
                                // after the write lock is released.
  }

  @endcode

  @param f The function to call followed by up to eight parameters,
            evaluated immediately. The parameter list must match the
            function signature. For non-static class member functions,
            the first argument must be a pointer an instance of the class.

  @see call
*/
  template <class Fn>
  void queue (Fn f)
  {
    queuef (vf::bind (f));
  }

  template <class Fn, typename  T1>
  void queue (Fn f,   const T1& t1)
  { queuef (vf::bind (f, t1)); }

  template <class Fn, typename  T1, typename  T2>
  void queue (Fn f,   const T1& t1, const T2& t2)
  { queuef (vf::bind (f, t1, t2)); }

  template <class Fn, typename  T1, typename  T2, typename  T3>
  void queue (Fn f,   const T1& t1, const T2& t2, const T3& t3)
  { queuef (vf::bind (f, t1, t2, t3)); }

  template <class Fn, typename  T1, typename  T2,
                      typename  T3, typename  T4>
  void queue (Fn f,   const T1& t1, const T2& t2,
                      const T3& t3, const T4& t4)
  { queuef (vf::bind (f, t1, t2, t3, t4)); }

  template <class Fn, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5>
  void queue (Fn f,   const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5)
  { queuef (vf::bind (f, t1, t2, t3, t4, t5)); }

  template <class Fn, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5, typename  T6>
  void queue (Fn f,   const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5, const T6& t6)
  { queuef (vf::bind (f, t1, t2, t3, t4, t5, t6)); }

  template <class Fn, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7>
  void queue (Fn f,   const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7)
  { queuef (vf::bind (f, t1, t2, t3, t4, t5, t6, t7)); }

  template <class Fn, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7, typename  T8>
  void queue (Fn f,   const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7, const T8& t8)
  { queuef (vf::bind (f, t1, t2, t3, t4, t5, t6, t7, t8)); }

protected:
  //============================================================================
  /** Synchronize the queue.

      A synchronize operation calls all functors in the queue.  If a functor
      causes additional functors to be added, they are eventually executed
      before synchronize() returns. Derived class call this when the queue is
      signaled, and optionally at any other time. Calling this function from
      more than one thread simultaneously is undefined.

      @return  true if any functors were executed.
  */
  bool synchronize ();

  /** Close the queue.

      Functors may not be added after this routine is called. This is used for
      diagnostics, to track down spurious calls during application shutdown
      or exit. Derived classes may call this if the appropriate time is known.

      The queue is synchronized after it is closed.
  */
  void close ();

  /** Called when the queue becomes signaled.

      A queue is signaled on the transition from empty to non-empty. Derived
      classes implement this function to perform a notification so that
      synchronize() will be called. For example, by triggering a WaitableEvent.

      @note Due to the implementation the queue can remain signaled for one
            extra cycle. This does not happen under load and is not an issue
            in practice.
  */
  virtual void signal () = 0;

  /** Called when the queue becomes non-signaled. */
  virtual void reset () = 0;

public:
  //============================================================================

  /** @internal
  
      @details Add a raw Call to the queue and synchronize if possible.

      @param c The call to add.
    */
  void callp (Call* c);

  /** @internal 

      @details Add a raw Call to the queue without synchronizing.

      @param c The call to add.
  */
  void queuep (Call* c);

  /** @internal
  
      @return The allocator to use when allocating a raw Call object.      
   */
  inline AllocatorType& getAllocator ()
  {
    return m_allocator;
  }

  /** @internal
  
      @return \c true if the calling thread of execution is associated with the
              queue.
  */
  bool isAssociatedWithCurrentThread () const;

  /** @internal
  
      @details This function helps provide diagnostics.
  
      @return \c true if the call stack contains synchronize() for this queue.
  */
  bool isBeingSynchronized () const { return m_isBeingSynchronized.isSignaled(); }

private:
  template <class Functor>
  class CallType : public Call
  {
  public:
    explicit CallType (Functor const& f) : m_f (f) { }
    void operator() () { m_f (); }

  private:
    Functor m_f;
  };

  bool doSynchronize ();

private:
  String const m_name;
  Thread::ThreadID m_id;
  LockFreeQueue <Call> m_queue;
  AtomicFlag m_closed;
  AtomicFlag m_isBeingSynchronized;
  AllocatorType m_allocator;
};

#endif
