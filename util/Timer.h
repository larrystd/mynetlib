
#ifndef BERT_TIMERMANAGER_H
#define BERT_TIMERMANAGER_H

#include <map>
#include <chrono>
#include <functional>
#include <memory>
#include <ostream>

///@file Timer.h
namespace ananas {

using DurationMs = std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;    // 时间点class
using TimerId = std::shared_ptr<std::pair<TimePoint, unsigned int> >;   // timepoint和int的pair, TimeId作为定时时间点存在, 根据TimePoint的值排序

constexpr int kForever = -1;

inline std::ostream& operator<< (std::ostream& os, const TimerId& d) {
    os << "[TimerId:" << (void*)d.get() << "]";
    return os;
}

namespace internal {

///@brief TimerManager class 定时器
///
/// You should not used it directly, but via Eventloop
class TimerManager final {
public:
    TimerManager();
    ~TimerManager();

    TimerManager(const TimerManager& ) = delete;
    void operator= (const TimerManager& ) = delete;

    void Update();

    ///@brief Schedule timer at absolute timepoint then repeat with period
    ///@param triggerTime The absolute time when timer first triggered
    ///@param period After first trigger, will be trigger by this period repeated until RepeatCount
    ///@param f The function to execute
    ///@param args Args for f
    ///
    /// RepeatCount: Timer will be canceled after trigger RepeatCount times, kForever implies forever.
    // 某个时刻执行, Duration, F, Args都是模板参数
    template <int RepeatCount, typename Duration, typename F, typename... Args>
    TimerId ScheduleAtWithRepeat(const TimePoint& triggerTime, const Duration& period, F&& f, Args&&... args);
    ///@brief Schedule timer with period
    ///@param period: Timer will be triggered every period
    ///
    /// RepeatCount: Timer will be canceled after triggered RepeatCount times, kForever implies forever.
    /// PAY ATTENTION: Timer's first trigger isn't at once, but after period time

    template <int REpeatCount, typename Duration, typename F, typename... Args>
    TimerId ScheduleAfterWithRepeat(const Duration& period, F&& f, Args&&... args);
    ///@brief Schedule timer at timepoint
    ///@param triggerTime: The absolute time when timer trigger
    ///
    /// It'll trigger only once
    template <typename F, typename... Args>
    TimerId ScheduleAt(const TimePoint& triggerTime, F&& f, Args&&... args);

    ///@brief Schedule timer after duration
    ///@param duration: After duration, timer will be triggered
    ///
    /// It'll trigger only once
    template <typename Duration, typename F, typename... Args>
    TimerId ScheduleAfter(const Duration& duration, F&& f, Args&&... args);

    ///@brief Cancel timer, 
    bool Cancel(TimerId id);

    ///@brief how far the nearest timer will be trigger.
    DurationMs NearestTimer() const;

private:
    class Timer {   // 包装定时任务, TimerManage 管理的就算Timer对象
        friend class TimerManager;
    public:
        explicit
        Timer(const TimePoint& tp);

        // only move
        Timer(Timer&& timer);
        Timer& operator= (Timer&& );

        Timer(const Timer& ) = delete;
        void operator= (const Timer& ) = delete;

        template <typename F, typename... Args>
        void SetCallback(F&& f, Args&&... args);    // 定时任务

        void OnTimer();

        TimerId Id() const;
        unsigned int UniqueId() const;

    private:
        void _Move(Timer&& timer);

        TimerId id_;

        std::function<void ()> func_;   // 绑定的函数, 返回值为void, 没有要输入的值(输入的值需要用占位符std::placeholder::_1)
        DurationMs interval_;
        int count_; // 重复次数
    };

    // 定时器, 通过multimap, 根据TimePoint时间点排序
    // multimap可以允许重复key, 但不支持operator[]索引
    // TimePoint是要执行的时刻
    std::multimap<TimePoint, Timer> timers_;    // 定时任务, 按照TimePoint排序

    friend class Timer;

    // not thread-safe, but who cares?
    static unsigned int s_timerIdGen_;
};

// 对于函数模板,例如ScheduleAtWithRepeat, 实现也在.h中
template <int RepeatCount, typename Duration,typename F, typename... Args>
TimerId TimerManager::ScheduleAtWithRepeat(const TimePoint& triggerTime, const Duration& period, F&& f, Args&&... args) {
    static_assert(RepeatCount != 0, "RepeatCount cannot set zero!");

    using namespace std::chrono;
    Timer t(triggerTime);
    t.interval_ = std::max(DurationMs(1), duration_cast<DurationMs>(period));
    t.count_ = RepeatCount;
    TimerId id = t.Id();

    t.SetCallback(std::forward<F>(f), std::forward<Args>(args)...);
    timers_.insert(std::make_pair(triggerTime, std::move(t)));  // pair(triggerTime, timer)

    return id;
}

template<int RepeatCount, typename Duration, typename F, typename... Args>
TimerId TimerManager::ScheduleAfterWithRepeat(const Duration& period, F&& f, Args&&... args) {
    const auto now = std::chrono::steady_clock::now();  // RepeatCount执行次数
    return ScheduleAtWithRepeat<RepeatCount>(now + period,
                                             period,    // 周期执行
                                             std::forward<F>(f),
                                             std::forward<Args>(args)...);
}

template <typename F, typename... Args>
TimerId TimerManager::ScheduleAt(const TimePoint& triggerTime, F&& f, Args&&... args) {
    return ScheduleAtWithRepeat<1>(triggerTime,
                                   DurationMs(0), // dummy
                                   std::forward<F>(f),
                                   std::forward<Args>(args)...);
}

template <typename Duration, typename F, typename... Args>
TimerId TimerManager::ScheduleAfter(const Duration& duration, F&& f, Args&&... args) {
    const auto now = std::chrono::steady_clock::now();
    return ScheduleAt(now + duration,
                      std::forward<F>(f),
                      std::forward<Args>(args)...);
}

template <typename F, typename... Args>
void TimerManager::Timer::SetCallback(F&& f, Args&&... args) {
    func_ = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
}

} // end namespace internal
} // end namespace ananas

#endif

