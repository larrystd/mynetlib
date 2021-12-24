#ifndef BERT_FUTURE_H
#define BERT_FUTURE_H

#include <atomic>
#include <mutex>
#include <functional>
#include <type_traits>
#include <condition_variable>

#include "Helper.h"
#include "Try.h"
#include "ananas/util/Scheduler.h"

namespace ananas {

namespace internal {

enum class Progress {   
    None,
    Timeout,
    Done,
    Retrieved,
};

using TimeoutCallback = std::function<void ()>; // 超时回调

template <typename T>
struct State {  // 线程的共享变量, 为promise和future传递信息使用
    static_assert(std::is_same<T, void>::value ||
                std::is_copy_constructible<T>() ||
                std::is_move_constructible<T>() ||
                "must be copyable or movable or void"); // 检查类型, 必须是void, 或者可拷贝, 或者可move    
    State() : progress_(Progress::None), retrieved_ {false} {

    }
    std::mutex thenLock_;   // 互斥器
    using ValueType = typename TryWrapper<T>::Type; // T是共享变量类型
    ValueType value_;
    std::function<void (ValueType&& )> then_;   
    Progress progress_;

    std::function<void(TimeoutCallback&& )> onTimeout_; // 回调函数
    std::atomic<bool> retrieved_;
};

} // namespace internal


template <typename T>
class Future;   // 前向声明

using namespace internal;

template <typename T>
class Promise {
 public:
    // state_在创建Promise时也创建了
    Promise() : state_(std::make_shared<State<T>>()) {

    }

    Promise(const Promise&) = default;  // 允许拷贝构造
    Promise& operator= (const Promise& ) = default;

    Promise(Promise&& pm) = default;
    Promise& operator= (Promise&& pm) = default;

    void SetException(std::exception_ptr exp) {
        std::unique_lock<std::mutex> guard(state_->thenLock_);

        if (state_->progress_ != Progress::None)
            return;
        state_->progress_ = Progress::Done;
        state_->value_ = typename State<T>::ValueType(std::move(exp));
        guard.unlock();

        if (state_->then_) 
            state_->then(std::move(state_->value_));
    }

    template <typename SHIT = T>    // SHIT类型不是void
    typename std::enable_if<!std::is_void<SHIT>::value, void>::type
    SetValue(SHIT&& t) {
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ != Progress::None)
            return;
        state_->progress_ = Progress::Done;
        state_->value_ = std::forward<SHIT>(t); // 设置value到state_中

        guard.unlock();

        if (state_->then_)
            state_->then_(std::move(state_->value_));   // 执行state_->value函数, value是参数
    }

    template <typename SHIT = T>    // SHIT为void
    typename std::enable_if<std::is_void<SHIT>::value, void>::type
    SetValue() {
        std::unqiue_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ != Progress::None)
            return;

        state_->progress_ = Progress::Done;
        state_->value_ = Try<void>();   // 设置value

        guard.unlock();
        if (state_->then_)  // 存在state_->then_函数, 则state_->value作为参数
            state_->then_(std::move(state_->value_));
    }

    Future<T> GetFuture() { // 获得future对象
        bool expect = false;
        if (!state_->retrieved_.compare_exchange_strong(expect, true)) {
            throw std::runtime_error("Future already retrieved");
        }
        return Future<T>(state_);   // 用state和T封装成Future对象
    }

    bool IsReady() const {
        return state_->progress_ != Progress::None;
    }

 private:
    std::shared_ptr<State<T>> state_;   // 全局变量状态state
};

template <typename T2>
Future<T2> MakeExceptionFuture(std::exception_ptr&& );

template <typename T>
class Future {  // Fututr对象, 封装多线程之共享变量State
public:
    using InnerType = T;

    template <typename U>
    friend class Future;

    Future() = default;

    Future(const Future&) = delete;
    void operator= (const Future&) = delete;

    Future(Future&& fut) = default;
    Future& operator= (Future&& fut) = default;

    explicit
    Future(std::shared_ptr<State<T>> state) :   // 用共享变量state构造Future
        state_(std::move(state)) {
    }

    bool
    valid() const {
        return state_ != nullptr;
    }

    // The blocking interface
    // PAY ATTENTION to deadlock: Wait thread must NOT be same as promise thread!!!
    // 等待
    typename State<T>::ValueType
    Wait(const std::chrono::milliseconds& timeout = std::chrono::milliseconds(24*3600*1000)) {
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        switch (state_->progress_) {
            case Progress::None:    // 继续向下执行
                break;
            
            case Progress::Timeout:
                throw std::runtime_error("Future timeout");
            case Progress::Done:
                state_->progress_ = Progress::Retrieved;
                return std::move(state_->value_);   // 线程执行完, 返回value
            default:
                throw std::runtime_error("Future already retrieved");
        }
        // Progress::None, 执行函数
        guard.unlock();

        auto cond(std::make_shared<std::condition_variable>());
        auto mutex(std::make_shared<std::mutex>());

        bool ready = false;
        typename State<T>::ValueType value;

        // 调用Future->Then
        this->Then([&value, &ready,
                    wcond = std::weak_ptr<std::condition_variable>(cond),
                    wmutex = std::weak_ptr<std::mutex>(mutex)]
                    (typename State<T>::ValueType&& v)
                {
                    auto cond = wcond.lock();
                    auto mutex = wmutex.lock();
                    if (!cond || !mutex)
                        return;
                    std::unique_lock<std::mutex> guard(*mutex);
                    value = std::move(v);
                    ready = true;
                    cond->notify_one;
                });
        
        std::unique_lock<std::mutex> waiter(*mutex);

        // 等待直到返回ready=true或者超时
        bool success = cond->wait_for(wait, timeout, [&ready]() { return ready; });
        
        if (success)
            return std::move(value);
        else
            return std::runtime_error("Future wait_for timeout");
    }

    // T is of type Future<InnerType>
    template <typename SHIT = T>
    typename std::enable_if<IsFuture<SHIT>::value, SHIT>::type
    Unwrap() {
        using InnerType = typename IsFuture<SHIT>::Inner;

        static_assert(std::is_same<SHIT, Future<InnerType>>::value, "Kidding me?");

        Promise<InnerType> prom;
        Future<InnerType> fut = prom.GetFuture();

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ == Progress::Timeout) {
            throw std::runtime_error("Wrong state : Timeout");
        } else if (state_->progress_ == Progress::Done) {
            try {
                auto innerFuture = std::move(state_->value_);
                return std::move(innerFuture.Value());
            } catch(const std::exception& e) {
                return MakeExceptionFuture<InnerType>(std::current_exception());
            }
        } else {
            _SetCallback([pm = std::move(prom)](typename TryWrapper<SHIT>::Type&& innerFuture) mutable {
                try {
                    SHIT future = std::move(innerFuture);
                    future._SetCallback([pm = std::move(pm)](typename TryWrapper<InnerType>::Type&& t) mutable {
                        // No need scheduler here, think about this code:
                        // `outer.Unwrap().Then(sched, func);`
                        // outer.Unwrap() is the inner future, the below line
                        // will trigger func in sched thread.
                        pm.SetValue(std::move(t));
                    });
                } catch (...) {
                    pm.SetException(std::current_exception());
                }
            });
        }

        return fut;
    }

    // 似乎封装成一个_ThenImpl<F, R>
    template <typename F,
            typename R = CallableResult<F, T>>
    auto Then(F&& f) -> typename R::ReturnFutureType {
        return _ThenImpl<F, R>(nullptr, std::forward<F>(f), Arguments());
    }

    // f will be called in sched 多传入一个参数Scheduler
    template <typename F,
              typename R = CallableResult<F, T> >
    auto Then(Scheduler* sched, F&& f) -> typename R::ReturnFutureType {
        typedef typename R::Arg Arguments;
        return _ThenImpl<F, R>(sched, std::forward<F>(f), Arguments());
    }

    //1. F does not return future type
    template <typename F, typename R, typename... Args>
    typename std::enable_if<!R::IsReturnFuture::value, typename R::ReturnFutureType>::Type
    _ThenImpl(Scheduler* sched, F&& f, ResultOfWrapper<F, Args...> ) {
        static_assert(sizeof... (Args) <= 1,  "Then must take zero/one argument");

        using FReturnType = typename R::IsReturnsFuture::Inner;

        Promise<FReturnType> pm;
        auto nextFuture = pm.GetFuture();   // future对象

        using FuncType = typename std::decay<F>::type;  // 退化&, *符号

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ == Progress::Timeout) {
            throw std::runtime_error("Wrong state : Timeout");
        } else if (state_->progress_ == Progress::Done) {   // Progress::Done, promise向state_ set了value
            typename TryWrapper<T>::Type t;
            try {
                t = std::move(state_ -> value_);    // 获得state_->value_, t
            } catch (const std::exception& e) {
                t = (typename TryWrapper<T>::Type)(std::current_exception());
            }

            guard.unlock();

            if (sched) {
                // 额是通过调用sched->Schedule
                sched->Schedule([t = std::move(t),
                                f = std::forward<FuncType>(f),
                                pm = std::move(pm)] () mutable {
                                    auto result = WrapWithTry(f, std::move(t));
                                    pm.SetValue(std::move(result));
                                })
            } else {
                auto result = WrapWithTry(f, std::move(t));
                pm.SetValue(std::move(result));
            }
        } else {
            // set this future's then callback
            _SetCallback([sched,
                         func = std::forward<FuncType>(f),
                         prom = std::move(pm)](typename TryWrapper<T>::Type&& t) mutable {
                if (sched) {
                    sched->Schedule([func = std::move(func),
                                     t = std::move(t),
                                     prom = std::move(prom)]() mutable {
                                         // run callback, T can be void, thanks to folly
                                         auto result = WrapWithTry(func, std::move(t));
                                         // set next future's result
                                         prom.SetValue(std::move(result));
                                  });
                } else {
                    // run callback, T can be void, thanks to folly Try<>
                    auto result = WrapWithTry(func, std::move(t));
                    // set next future's result
                    prom.SetValue(std::move(result));   // 设置变量(promise可以访问了)
                }
            });
        }

        return std::move(nextFuture);
    }

    //2. F return another future type
    template <typename F, typename R, typename... Args>
    typename std::enable_if<R::IsReturnsFuture::value, typename R::ReturnFutureType>::type
    _ThenImpl(Scheduler* sched, F&& f, ResultOfWrapper<F, Args... >) {
        static_assert(sizeof... (Args) <= 1,"Then must take zero/one argument");

        using FReturnType = typename R::IsReturnsFuture::Inner;

        Promise<FReturnType> pm;
        auto nextFuture = pm.GetFuture();

        using FuncType = typename std::decay<F>::type;

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ == Progress::Timeout) {
            throw std::runtime_error("Wrong state : Timeout");
        }else if (state_->progress_ == Progress::Done) {
            typename TryWrapper<T>::Type t;
            try {
                t = std::move(state_->value_);
            } catch (const std::exception& e) {
                t = dectype(t)(std::current_exception());
            }

            guard.unlock();

            auto cb = [res = std::move(t),
                       f = std::forward<FuncType>(f),
                       prom = std::move(pm)]() mutable {
                // because func return another future: innerFuture, when innerFuture is done, nextFuture can be done
                decltype(f(res.template Get<Args>()...)) innerFuture;
                if (res.HasException()) {
                    // Failed if Args... is void
                    innerFuture = f(typename TryWrapper<typename std::decay<Args...>::type>::Type(res.Exception()));
                } else {
                    innerFuture = f(res.template Get<Args>()...);
                }

                if (!innerFuture.valid()) {
                    return;
                }

                std::unique_lock<std::mutex> guard(innerFuture.state_->thenLock_);
                if (innerFuture.state_->progress_ == Progress::Timeout) {
                    throw std::runtime_error("Wrong state : Timeout");
                } else if (innerFuture.state_->progress_ == Progress::Done) {
                    typename TryWrapper<FReturnType>::Type t;
                    try {
                        t = std::move(innerFuture.state_->value_);
                    } catch(const std::exception& e) {
                        t = decltype(t)(std::current_exception());
                    }

                    guard.unlock();
                    prom.SetValue(std::move(t));
                } else {
                    innerFuture._SetCallback([prom = std::move(prom)](typename TryWrapper<FReturnType>::Type&& t) mutable {
                        prom.SetValue(std::move(t));
                    });
                }
            };

            if (sched) 
                sched->Schedule(std::move(cb));
            else 
                cb();
        }else {
            // set this future's then callback
            _SetCallback([sched = sched,
                         func = std::forward<FuncType>(f),
                         prom = std::move(pm)](typename TryWrapper<T>::Type&& t) mutable {

                auto cb = [func = std::move(func), t = std::move(t), prom = std::move(prom)]() mutable {
                    // because func return another future: innerFuture, when innerFuture is done, nextFuture can be done
                    decltype(func(t.template Get<Args>()...)) innerFuture;
                    if (t.HasException()) {
                        // Failed if Args... is void
                        innerFuture = func(typename TryWrapper<typename std::decay<Args...>::type>::Type(t.Exception()));
                    } else {
                        innerFuture = func(t.template Get<Args>()...);
                    }

                    if (!innerFuture.valid()) {
                        return;
                    }
                    std::unique_lock<std::mutex> guard(innerFuture.state_->thenLock_);
                    if (innerFuture.state_->progress_ == Progress::Timeout) {
                        throw std::runtime_error("Wrong state : Timeout");
                    } else if (innerFuture.state_->progress_ == Progress::Done) {
                        typename TryWrapper<FReturnType>::Type t;
                        try {
                            t = std::move(innerFuture.state_->value_);
                        } catch(const std::exception& e) {
                            t = decltype(t)(std::current_exception());
                        }

                        guard.unlock();
                        prom.SetValue(std::move(t));
                    } else {
                        innerFuture._SetCallback([prom = std::move(prom)](typename TryWrapper<FReturnType>::Type&& t) mutable {
                            prom.SetValue(std::move(t));
                        });
                    }
                };

                if (sched)
                    sched->Schedule(std::move(cb));
                else
                    cb();
            });
        }

        return std::move(nextFuture);
    }

    /*
     * When register callbacks and timeout for a future like this:
     *      Future<int> f;
     *      f.Then(xx).OnTimeout(yy);
     *
     * There will be one future object created except f, we call f as root future.
     * The yy callback is registed on the last future, here are the possiblities:
     * 1. xx is called, and yy is not called.
     * 2. xx is not called, and yy is called.
     *
     * BUT BE CAREFUL BELOW:
     *
     *      Future<int> f;
     *      f.Then(xx).Then(yy).OnTimeout(zz);
     *
     * There will be 3 future objects created except f, we call f as root future.
     * The zz callback is registed on the last future, here are the possiblities:
     * 1. xx is called, and zz is called, yy is not called.
     * 2. xx and yy are called, and zz is called, aha, it's rarely happend but...
     * 3. xx and yy are called, it's the normal case.
     * So, you may shouldn't use OnTimeout with chained futures!!!
     */
    // 超时回调函数, 调用TimeoutCallback f
    void OnTimeout(std::chrono::milliseconds duration,
                TimeoutCallback f,
                Scheduler* scheduler) {
        
        scheduler->ScheduleLater(duration, [state = state_, cb = std::move(f)]() mutable {
            std::unique_lock<std::mutex> guard(state->thenLock_);
            if (state->progress_ != Progress::None)
                return;

            state->progress_ = Progress::Timeout;
            guard.unlock();

            cb();
        });
    }

private:
    // 设置回调函数, state_->then_
    void _SetCallback(std::function<void (typename TryWrapper<T>::Type&& )>&& func) {
        state_->then_ = std::move(func);
    }

    // 超时回调函数
    void _SetOnTimeout(std::function<void (TimeoutCallback&& )>&& func) {
        state_->onTimeout_ = std::move(func);
    }

    std::shared_ptr<State<T>> state_;   // Future内部维护的共享变量state
};

// Make ready future, 用T2 创建Promise, 返回future对象
// 函数模板, 模板需要匹配所有<>中的元素, 包括T2, typename std::decay<T2>::type
template <typename T2>  
inline Future<typename std::decay<T2>::type> MakeReadyFuture(T2&& value) {
    Promise<typename std::decay<T2>::type> pm;
    auto f(pm.GetFuture());
    pm.SetValue(std::forward<T2>(value));

    return f;
}

// 函数模板特化, 模板满足void
inline Future<void> MakeReadyFuture() { 
    Promise<void> pm;
    auto f(pm.GetFuture());
    pm.SetValue();

    return f;
}

// Make exception future
template <typename T2, typename E>
inline Future<T2> MakeExceptionFuture(E&& exp) {
    Promise<T2> pm;
    pm.SetException(std::make_exception_ptr(std::forward<E>(exp)));

    return pm.GetFuture();
}

template <typename T2>
inline Future<T2> MakeExceptionFuture(std::exception_ptr&& eptr) {
    Promise<T2> pm;
    pm.SetException(std::move(eptr));

    return pm.GetFuture();
}


// When All
template <typename... FT>
typename CollectAllVariadicContext<typename std::decay<FT>::type::InnerType...>::FutureType
WhenAll(FT&&... futures) {
    auto ctx = std::make_shared<CollectAllVariadicContext<typename std::decay<FT>::type::InnerType...>>();

    CollectVariadicHelper<CollectAllVariadicContext>(
        ctx, std::forward<typename std::decay<FT>::type>(futures)...);

    return ctx->pm.GetFuture();
}


template <class InputIterator>
Future<
      std::vector<typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>
      >
WhenAll(InputIterator first, InputIterator last) {
    using TryT = typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type;
    if (first == last)
        return MakeReadyFuture(std::vector<TryT>());

    struct AllContext {
        AllContext(int n) : results(n) {}
        ~AllContext() {
            // I think this line is useless.
            // pm.SetValue(std::move(results));
        }

        Promise<std::vector<TryT>> pm;
        std::vector<TryT> results;
        std::atomic<size_t> collected{0};
    };

    auto ctx = std::make_shared<AllContext>(std::distance(first, last));

    for (size_t i = 0; first != last; ++first, ++i) {
        first->Then([ctx, i](TryT&& t) {
            ctx->results[i] = std::move(t);
            if (ctx->results.size() - 1 ==
                    std::atomic_fetch_add (&ctx->collected, std::size_t(1))) {
                ctx->pm.SetValue(std::move(ctx->results));
            }
        });
    }

    return ctx->pm.GetFuture();
}

// When Any
template <class InputIterator>
Future<
      std::pair<size_t, typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>
      >
WhenAny(InputIterator first, InputIterator last) {
    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;
    using TryT = typename TryWrapper<T>::Type;

    if (first == last) {
        return MakeReadyFuture(std::make_pair(size_t(0), TryT()));
    }

    struct AnyContext {
        AnyContext() {};
        Promise<std::pair<size_t, TryT>> pm;
        std::atomic<bool> done{false};
    };

    auto ctx = std::make_shared<AnyContext>();
    for (size_t i = 0; first != last; ++first, ++i) {
        first->Then([ctx, i](TryT&& t) {
            if (!ctx->done.exchange(true)) {
                ctx->pm.SetValue(std::make_pair(i, std::move(t)));
            }
        });
    }

    return ctx->pm.GetFuture();
}


// When N
template <class InputIterator>
Future<
     std::vector<std::pair<size_t, typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>>
      >
WhenN(size_t N, InputIterator first, InputIterator last) {
    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;
    using TryT = typename TryWrapper<T>::Type;

    size_t nFutures = std::distance(first, last);
    const size_t needCollect = std::min<size_t>(nFutures, N);

    if (needCollect == 0) {
        return MakeReadyFuture(std::vector<std::pair<size_t, TryT>>());
    }

    struct NContext {
        NContext(size_t _needs) : needs(_needs) {}
        Promise<std::vector<std::pair<size_t, TryT>>> pm;

        std::mutex mutex;
        std::vector<std::pair<size_t, TryT>> results;
        const size_t needs;
        bool done {false};
    };

    auto ctx = std::make_shared<NContext>(needCollect);
    for (size_t i = 0; first != last; ++first, ++i) {
        first->Then([ctx, i](TryT&& t) {
            std::unique_lock<std::mutex> guard(ctx->mutex);
            if (ctx->done)
                return;

            ctx->results.push_back(std::make_pair(i, std::move(t)));
            if (ctx->needs == ctx->results.size()) {
                ctx->done = true;
                guard.unlock();
                ctx->pm.SetValue(std::move(ctx->results));
            }
        });
    }

    return ctx->pm.GetFuture();
}

// When If Any
template <class InputIterator>
Future<
      std::pair<size_t, typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>
      >
WhenIfAny(InputIterator first, InputIterator last,
          std::function<bool (const Try<typename std::iterator_traits<InputIterator>::value_type::InnerType>& )> cond) {

    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;
    using TryT = typename TryWrapper<T>::Type;

    if (first == last) {
        return MakeReadyFuture(std::make_pair(size_t(0), TryT()));
    }

    const size_t nFutures = std::distance(first, last);

    struct IfAnyContext {
        IfAnyContext() {};
        Promise<std::pair<size_t, TryT>> pm;
        std::atomic<size_t> returned {0};  // including fail response, eg, cond(rsp) == false
        std::atomic<bool> done{false};
    };

    auto ctx = std::make_shared<IfAnyContext>();
    for (size_t i = 0; first != last; ++first, ++i) {
        first->Then([ctx, i, nFutures, cond](TryT&& t) {
            if (ctx->done) {
                ctx->returned.fetch_add(1);
                return;
            }
            if (!cond(t)) {
                const size_t returned = ctx->returned.fetch_add(1) + 1;
                if (returned == nFutures) {
                    // If some success future done, below if statement will be false
                    if (!ctx->done.exchange(true)) {
                        // FAILED...
                        try {
                            throw std::runtime_error("WhenIfAny Failed, no true condition.");
                        } catch(...) {
                            ctx->pm.SetException(std::current_exception());
                        }
                    }
                }

                return;
            }
            if (!ctx->done.exchange(true)) {
                ctx->pm.SetValue(std::make_pair(i, std::move(t)));
            }
            ctx->returned.fetch_add(1);
        });
    }

    return ctx->pm.GetFuture();
}


// When if N
template <class InputIterator>
Future<
     std::vector<std::pair<size_t, typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>>
      >
WhenIfN(size_t N, InputIterator first, InputIterator last,
        std::function<bool (const Try<typename std::iterator_traits<InputIterator>::value_type::InnerType>& )> cond) {

    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;
    using TryT = typename TryWrapper<T>::Type;

    size_t nFutures = std::distance(first, last);
    const size_t needCollect = std::min<size_t>(nFutures, N);

    if (needCollect == 0) {
        return MakeReadyFuture(std::vector<std::pair<size_t, TryT>>());
    }

    struct IfNContext {
        IfNContext(size_t _needs) : needs(_needs) {}
        Promise<std::vector<std::pair<size_t, TryT>>> pm;

        std::mutex mutex;
        std::vector<std::pair<size_t, TryT>> results;
        size_t returned {0}; // including fail response, eg, cond(rsp) == false
        const size_t needs;
        bool done {false};
    };

    auto ctx = std::make_shared<IfNContext>(needCollect);
    for (size_t i = 0; first != last; ++first, ++i) {
        first->Then([ctx, i, nFutures, cond](TryT&& t) {
            std::unique_lock<std::mutex> guard(ctx->mutex);
            ++ctx->returned;
            if (ctx->done)
                return;

            if (!cond(t)) {
                if (ctx->returned == nFutures) {
                    // Failed: all returned, but not enough true cond(t)!
                    // Should I return partial result ???
                    try {
                        throw std::runtime_error("WhenIfN Failed, not enough true condition.");
                    } catch(...) {
                        ctx->done = true;
                        guard.unlock();
                        ctx->pm.SetException(std::current_exception());
                    }
                }

                return;
            }

            ctx->results.push_back(std::make_pair(i, std::move(t)));
            if (ctx->needs == ctx->results.size()) {
                ctx->done = true;
                guard.unlock();
                ctx->pm.SetValue(std::move(ctx->results));
            }
        });
    }

    return ctx->pm.GetFuture();
}

} // namespace ananas

#endif

