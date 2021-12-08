
#ifndef BERT_EVENTLOOP_H
#define BERT_EVENTLOOP_H

#include <map>
#include <memory>
#include <sys/resource.h>

#include "Poller.h"
#include "PipeChannel.h"
#include "Typedefs.h"
#include "ananas/util/Timer.h"
#include "ananas/util/Scheduler.h"
#include "ananas/future/Future.h"

namespace ananas {

struct SocketAddr;

namespace internal {
class Connector;
}

///@brief EventLoop class
///
/// One thread should at most has one EventLoop object.
class EventLoop : public Scheduler {
public:
    ///@brief Constructor
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop& ) = delete;  // 不可拷贝和移动
    void operator= (const EventLoop& ) = delete;
    EventLoop(EventLoop&& ) = delete;
    void operator= (EventLoop&& ) = delete;

    // listener
    bool Listen(const SocketAddr& addr, NewTcpConnCallback cb); 
    bool Listen(const char* ip, uint16_t hostPort, NewTcpConnCallback cb);
    bool ListenUDP(const SocketAddr& listenAddr,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb);
    bool ListenUDP(const char* ip,
                   uint16_t hostPort,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb);

    // udp client
    bool CreateClientUDP(UDPMessageCallback mcb,
                         UDPCreateCallback ccb);


    // connector
    bool Connect(const SocketAddr& dst,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max(),
                 EventLoop* dstLoop = nullptr);
    bool Connect(const char* ip,
                 uint16_t hostPort,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max(),
                 EventLoop* dstLoop = nullptr);

    ///@brief timer : NOT thread-safe
    // See [ScheduleAtWithRepeat](@ref Timer::ScheduleAtWithRepeat)
    template <int RepeatCount, typename Duration, typename F, typename... Args>
    TimerId ScheduleAtWithRepeat(const TimePoint& , const Duration& , F&& , Args&&...);
    ///@brief timer : NOT thread-safe
    /// See [ScheduleAfterWithRepeat](@ref Timer::ScheduleAfterWithRepeat)
    template <int RepeatCount, typename Duration, typename F, typename... Args>
    TimerId ScheduleAfterWithRepeat(const Duration& , F&& , Args&&...);
    ///@brief Cancel timer
    bool Cancel(TimerId id);

    /// See `Timer::ScheduleAt`
    /// See [ScheduleAt](@ref Timer::ScheduleAt)
    template <typename F, typename... Args>
    TimerId ScheduleAt(const TimePoint& , F&& , Args&&...);

    ///@brief timer : NOT thread-safe
    /// See [ScheduleAfter](@ref Timer::ScheduleAfter)
    template <typename Duration, typename F, typename... Args>
    TimerId ScheduleAfter(const Duration& , F&& , Args&&...);

    ///@brief Internal use for future
    ///
    /// thread-safe
    void ScheduleLater(std::chrono::milliseconds , std::function<void ()> ) override;
    void Schedule(std::function<void ()> ) override;

    ///@brief Execute work in this loop
    /// thread-safe, and F return non-void
    ///
    /// Usage:
    ///@code
    /// loop.Execute(my_work_func, some_args)
    ///     .Then(process_work_result);
    ///@endcode
    ///my_work_func will be executed ASAP, but you SHOULD assure that
    ///it will NOT block, otherwise EventLoop will be hang!
    // 执行函数, 返回值是Future
    template <typename F, typename... Args,
        typename = typename std::enable_if<!std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type, typename Dummy = void>
    auto Execute(F&&, Args&&...) -> Future<typename std::result_of<F (Args...)>::type>; // std::result_of用来推导类型

    ///@brief Execute work in this loop
    /// thread-safe, and F return void
    template <typename F, typename... Args,
              typename = typename std::enable_if<std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type>
    auto Execute(F&& , Args&&...) -> Future<void>;

    ///@brief Run application
    ///
    /// It's a infinite loop, until Application stopped
    void Run();

    bool Register(int events, std::shared_ptr<internal::Channel> src);  // 注册channel到poller
    bool Modify(int events, std::shared_ptr<internal::Channel> src);
    void Unregister(int events, std::shared_ptr<internal::Channel> src);

    ///@brief Connection size
    std::size_t Size() const {
        return channelSet_.size();
    }

    ///@brief If the caller thread run this loop?
    ///Usage:
    ///@code
    /// if (loop.InThisLoop())
    ///     do_something_in_this_loop(); // direct run it
    /// else
    ///     loop.Execute(do_something_in_this_loop); // schedule to this loop
    ///@endcode
    bool InThisLoop() const;

    ///@brief Unique id
    int Id() const {
        return id_;
    }

    ///@brief EventLoop in this thread
    ///
    /// There will be at most one EventLoop in each thread, return it for current thread
    static EventLoop* Self();

    static void SetMaxOpenFd(rlim_t maxfdPlus1);

private:
    bool _Loop(DurationMs timeout);

    std::unique_ptr<internal::Poller> poller_;  // eventloop与之对应的poller_

    std::shared_ptr<internal::PipeChannel> notifier_;   // 这个是eventfd,用来唤醒reactor阻塞的epoll_wait

    internal::TimerManager timers_;

    // channelSet_ must be destructed before timers_
    std::map<unsigned int, std::shared_ptr<internal::Channel> > channelSet_;    // channel集合, map fd->channel

    std::mutex fctrMutex_;  // 互斥器
    std::vector<std::function<void ()> > functors_;     // 要处理的函数任务

    int id_;
    static std::atomic<int> s_evId;

    static thread_local unsigned int s_id;

    // max open fd + 1
    static rlim_t s_maxOpenFdPlus1;
};

template <int RepeatCount, typename Duration, typename F, typename... Args>
TimerId EventLoop::ScheduleAtWithRepeat(const TimePoint& triggerTime,
                                        const Duration& period,
                                        F&& f, Args&&... args) {
    assert (InThisLoop());
    return timers_.ScheduleAtWithRepeat<RepeatCount>(triggerTime,
                                                     period,
                                                     std::forward<F>(f),
                                                     std::forward<Args>(args)...);  // 定时器处理函数
}

template <int RepeatCount, typename Duration, typename F, typename... Args>
TimerId EventLoop::ScheduleAfterWithRepeat(const Duration& period,
                                           F&& f, Args&&... args) {
    assert (InThisLoop());
    return timers_.ScheduleAfterWithRepeat<RepeatCount>(period,
                                                        std::forward<F>(f),
                                                        std::forward<Args>(args)...);
}

template <typename F, typename... Args>
TimerId EventLoop::ScheduleAt(const TimePoint& triggerTime,
                              F&& f, Args&&... args) {
    assert (InThisLoop());
    return timers_.ScheduleAt(triggerTime,
                              std::forward<F>(f),
                              std::forward<Args>(args)...); // 某个时刻执行F, 调用timers_
}

template <typename Duration, typename F, typename... Args>
TimerId EventLoop::ScheduleAfter(const Duration& duration,
                                 F&& f, Args&&... args) {
    assert (InThisLoop());
    return timers_.ScheduleAfter(duration,
                                 std::forward<F>(f),
                                 std::forward<Args>(args)...);
}

// If F return something not void, or return Future
template <typename F, typename... Args, typename, typename >

Future<typename std::result_of<F (Args...)>::type>
EventLoop::Execute(F&& f, Args&&... args) {

    using resultType = typename std::result_of<F (Args...)>::type;

    Promise<resultType> promise;
    auto future = promise.GetFuture();

    if (InThisLoop()) {
        promise.SetValue(std::forward<F>(f)(std::forward<Args>(args)...));  // promise.SetValue
    } else {
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...); // 任务
        auto func = [t = std::move(task), pm = std::move(promise)]() mutable {
            try {
                pm.SetValue(Try<resultType>(t()));
            } catch(...) {
                pm.SetException(std::current_exception());
            }
        };

        {
            std::unique_lock<std::mutex> guard(fctrMutex_);
            functors_.emplace_back(std::move(func));    // func加入要执行线程functors_列表
        }

        notifier_->Notify();    // 写一些东西使fd可读, 唤醒epoll_wait
    }

    return future;
}

// F return void, loop执行函数
template <typename F, typename... Args, typename >
Future<void> EventLoop::Execute(F&& f, Args&&... args) {

    using resultType = typename std::result_of<F (Args...)>::type;
    static_assert(std::is_void<resultType>::value, "must be void");

    Promise<void> promise;
    auto future = promise.GetFuture();  // 从promise获得future对象

    if (InThisLoop()) {
        std::forward<F>(f)(std::forward<Args>(args)...);
        promise.SetValue();
    } else {
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...); // 要执行的任务
        auto func = [t = std::move(task), pm = std::move(promise)]() mutable {
            try {
                t();
                pm.SetValue();
            } catch(...) {
                pm.SetException(std::current_exception());
            }
        };

        {
            std::unique_lock<std::mutex> guard(fctrMutex_);
            functors_.emplace_back(std::move(func));
        }

        notifier_->Notify();
    }

    return future;
}


} // namespace ananas

#endif

