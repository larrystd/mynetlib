#include <vector>
#include <cassert>
#include "Timer.h"

namespace ananas {
namespace internal {

unsigned int TimerManager::s_timerIdGen_ = 0;

TimerManager::TimerManager() {
}

TimerManager::~TimerManager() {
}

void TimerManager::Update() {   // 这里由loop调用, 目的是处理定时器的任务
    if (timers_.empty())
        return;

    const auto now = std::chrono::steady_clock::now();

    for (auto it(timers_.begin()); it != timers_.end(); ) { // 访问timers_ mulitmap
        if (it->first > now)    // 时刻不能晚于now
            return;

        // support cancel self
        it->second.OnTimer();

        // steal and erase it
        Timer timer(std::move(it->second));
        it = timers_.erase(it);

        if (timer.count_ != 0) {    // 重新加入定时器
            // need reschedule
            const auto tp = timer.id_->first;
            auto itNew = timers_.insert(std::make_pair(tp, std::move(timer)));  // 插入到定时器
            if (it == timers_.end() || itNew->first < it->first)
                it = itNew;
        }
    }
}

bool TimerManager::Cancel(TimerId id) { // 根据id删除timer_列表的pair
    //time point + uid
    auto begin = timers_.lower_bound(id->first);
    if (begin == timers_.end())
        return false;

    auto end = timers_.upper_bound(id->first);
    for (auto it(begin); it != end; ++ it) {
        if (it->second.UniqueId() == id->second) {
            // lazy delete, in case cancel another timer during on timer call.
            it->second.count_ = 0;
            return true;
        }
    }

    return false;
}

DurationMs TimerManager::NearestTimer() const { // 最近的一个Timer, poll等待时刻必须少于near Timer, 防止定时器超时
    if (timers_.empty())
        return DurationMs::max();

    const auto& timer = timers_.begin()->second;
    auto now = std::chrono::steady_clock::now();
    if (now > timer.Id()->first)
        return DurationMs::min();
    else
        return std::chrono::duration_cast<DurationMs>(timer.Id()->first - now);
}

TimerManager::Timer::Timer(const TimePoint& tp) :
    id_(std::make_shared<std::pair<TimePoint, unsigned int>>(tp, ++ TimerManager::s_timerIdGen_)),
    count_(kForever) {
}

TimerManager::Timer::Timer(Timer&& timer) {
    _Move(std::move(timer));
}

TimerManager::Timer& TimerManager::Timer::operator= (Timer&& timer) {
    if (this != &timer)
        _Move(std::move(timer));

    return *this;
}

void TimerManager::Timer::_Move(Timer&& timer) {
    this->id_ = std::move(timer.id_);
    this->func_ = std::move(timer.func_);
    this->interval_ = std::move(timer.interval_);
    this->count_ = timer.count_;
}


void TimerManager::Timer::OnTimer() {   // Timer的
    if (!func_ || count_ == 0)
        return;

    if (count_ == kForever || count_-- > 0) {   // 还有重复次数, 执行
        func_();
        id_->first += interval_;
    } else {
        count_ = 0; // in case if count_ other than -1
    }
}

TimerId TimerManager::Timer::Id() const {
    return id_;
}

unsigned int TimerManager::Timer::UniqueId() const {
    return id_->second;
}

} // end namespace internal
} // end namespace ananas

