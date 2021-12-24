
#include <cassert>
#include <thread>

#include "EventLoop.h"
#include "Application.h"

#include "Acceptor.h"
#include "Connection.h"
#include "Connector.h"
#include "DatagramSocket.h"

#if defined(__APPLE__)
#include "Kqueue.h"
#elif defined(__gnu_linux__)
#include "Epoller.h"
#else
#error "Only support osx and linux"
#endif

#include "AnanasDebug.h"
#include "util/Util.h"

namespace ananas {

static thread_local EventLoop* g_thisLoop = nullptr;

EventLoop* EventLoop::Self() {
    return g_thisLoop;
}

void EventLoop::SetMaxOpenFd(rlim_t maxfdPlus1) {
    if (ananas::SetMaxOpenFd(maxfdPlus1))
        s_maxOpenFdPlus1 = maxfdPlus1;
}

EventLoop::EventLoop() {
    assert (!g_thisLoop && "There must be only one EventLoop per thread");
    g_thisLoop = this;

    internal::InitDebugLog(logALL);

#if defined(__APPLE__)
    poller_.reset(new internal::Kqueue);
#elif defined(__gnu_linux__)
    poller_.reset(new internal::Epoller);
#else
#error "Only support mac os and linux"
#endif

    notifier_ = std::make_shared<internal::PipeChannel>();
    id_ = s_evId ++;
}

EventLoop::~EventLoop() {
}

bool EventLoop::Listen(const char* ip,
                       uint16_t hostPort,
                       NewTcpConnCallback newConnCallback) {
    SocketAddr addr;
    addr.Init(ip, hostPort);

    return Listen(addr, std::move(newConnCallback));
}

// eventloop的监听, 先创建一个Acceptor, 配置回调函数和Bind, Bind这里包括bind和listen
bool EventLoop::Listen(const SocketAddr& listenAddr,
                       NewTcpConnCallback newConnCallback) {
    using internal::Acceptor;

    auto s = std::make_shared<Acceptor>(this);// 创建Accptor对象, 指针用shared_ptr维护
    s->SetNewConnCallback(std::move(newConnCallback));  // 设置连接回调
    if (!s->Bind(listenAddr))
        return false;

    return true;
}

bool EventLoop::ListenUDP(const SocketAddr& listenAddr,
                          UDPMessageCallback mcb,
                          UDPCreateCallback ccb) {  // UDP设置回调函数, bin
    auto s = std::make_shared<DatagramSocket>(this);    
    s->SetMessageCallback(mcb);
    s->SetCreateCallback(ccb);
    if (!s->Bind(&listenAddr))
        return false;

    return true;
}

bool EventLoop::ListenUDP(const char* ip, uint16_t hostPort,
                          UDPMessageCallback mcb,
                          UDPCreateCallback ccb) {
    SocketAddr addr;
    addr.Init(ip, hostPort);

    return ListenUDP(addr, mcb, ccb);
}


bool EventLoop::CreateClientUDP(UDPMessageCallback mcb,
                                UDPCreateCallback ccb) {
    auto s = std::make_shared<DatagramSocket>(this);
    s->SetMessageCallback(mcb);
    s->SetCreateCallback(ccb);
    if (!s->Bind(nullptr))
        return false;

    return true;
}

bool EventLoop::Connect(const char* ip,
                        uint16_t hostPort,
                        NewTcpConnCallback nccb,
                        TcpConnFailCallback cfcb,
                        DurationMs timeout,
                        EventLoop* dstLoop) {
    SocketAddr addr;
    addr.Init(ip, hostPort);    // 用ip, hostPort构造addr

    return Connect(addr, nccb, cfcb, timeout, dstLoop); // 发起连接addr地址
}


bool EventLoop::Connect(const SocketAddr& dst,
                        NewTcpConnCallback nccb,
                        TcpConnFailCallback cfcb,
                        DurationMs timeout,
                        EventLoop* dstLoop) {
    using internal::Connector;

    auto cli = std::make_shared<Connector>(this);   // 尝试连接
    cli->SetFailCallback(cfcb);
    cli->SetNewConnCallback(nccb);

    if (!cli->Connect(dst, timeout, dstLoop))
        return false;

    return true;
}


thread_local unsigned int EventLoop::s_id = 0;

std::atomic<int> EventLoop::s_evId {0};

rlim_t EventLoop::s_maxOpenFdPlus1 = ananas::GetMaxOpenFd();

bool EventLoop::Register(int events, std::shared_ptr<internal::Channel> src) {  // 注册一个events，Channel到poll
    if (events == 0)
        return false;

    if (src->GetUniqueId() != 0)
        assert(false);

    /* man getrlimit:
     * RLIMIT_NOFILE
     * Specifies a value one greater than the maximum file descriptor number
     * that can be opened by this process.
     * Attempts (open(2), pipe(2), dup(2), etc.)
     * to exceed this limit yield the error EMFILE.
     */
    if (src->Identifier() + 1 >= static_cast<int>(s_maxOpenFdPlus1)) {
        ANANAS_ERR
                << "Register failed! Max open fd " << s_maxOpenFdPlus1
                << ", current fd " << src->Identifier();

        return false;
    }

    ++ s_id;
    if (s_id == 0) // wrap around
        s_id = 1;

    src->SetUniqueId(s_id);
    ANANAS_INF << "Register " << s_id << " to me " << pthread_self();

    if (poller_->Register(src->Identifier(), events, src.get()))    // 注册Channel到poller_
        return channelSet_.insert({src->GetUniqueId(), src}).second;    // 成功插入channelSet

    return false;
}

bool EventLoop::Modify(int events, std::shared_ptr<internal::Channel> src) {    // 修改poller的event和fd
    assert (channelSet_.count(src->GetUniqueId()));
    return poller_->Modify(src->Identifier(), events, src.get());
}

void EventLoop::Unregister(int events, std::shared_ptr<internal::Channel> src) {
    const int fd = src->Identifier();
    ANANAS_INF << "Unregister socket id " << fd;
    poller_->Unregister(fd, events);

    size_t nTask = channelSet_.erase(src->GetUniqueId());
    if (nTask != 1) {
        ANANAS_ERR << "Can not find socket id " << fd;
        assert (false);
    }
}

bool EventLoop::Cancel(TimerId id) {
    return timers_.Cancel(id);
}

void EventLoop::Run() { // 主循环
    assert (this->InThisLoop());

    const DurationMs kDefaultPollTime(10);
    const DurationMs kMinPollTime(1);   // 最短的poll时间

    Register(internal::eET_Read, notifier_);   // notifier channel注册可读到poller中, 这个用来唤醒epoll_wait 

    // 主循环,执行_Loop
    while (!Application::Instance().IsExit()) {
        auto timeout = std::min(kDefaultPollTime, timers_.NearestTimer());  // 最早的定时器时间
        timeout = std::max(kMinPollTime, timeout);

        _Loop(timeout);// 这个思想和redis类似, 在timeout时间下执行loop循环, 等超时了执行定时器
    }

    for (auto& kv : channelSet_) {  // 取消所有channelSet 
        poller_->Unregister(internal::eET_Read | internal::eET_Write,
                            kv.second->Identifier());
    }

    channelSet_.clear();
    poller_.reset();
}

bool EventLoop::_Loop(DurationMs timeout) { // 一个loop循环, timeout是最近的定时器时间
    ANANAS_DEFER {
        timers_.Update();   // 优先处理定时器, 保证不超时

        // do not block, 再处理任务队列的函数
        if (fctrMutex_.try_lock()) {
            // Use tmp : if f add callback to functors_
            decltype(functors_) funcs;
            funcs.swap(functors_);
            fctrMutex_.unlock();

            for (const auto& f : funcs) // 执行
                f();
        }
    };

    if (channelSet_.empty()) {
        std::this_thread::sleep_for(timeout);
        return false;
    }

    // muduo是把定时器timefd和pollfd统一处理, 这里只处理pollfd, 为了不超定时器的时间, 等之前设置epoll_wait最长等待时间不超过定时器最早时间
    const int ready = poller_->Poll(static_cast<int>(channelSet_.size()),
                                    static_cast<int>(timeout.count())); // 等待活跃channel, 设置超时时间为timeout
    if (ready < 0)
        return false;

    
    const auto& fired = poller_->GetFiredEvents();  // 得到活跃的事件

    // Consider stale event, DO NOT unregister another socket in event handler!

    std::vector<std::shared_ptr<internal::Channel>> sources(ready); // 处理活跃的events, 
    for (int i = 0; i < ready; ++ i) {
        auto src = (internal::Channel* )fired[i].userdata;  // src是一个channel
        sources[i] = src->shared_from_this();

        // 根据fired, 执行对应的回调函数, HandleReadEvent, HandleWriteEvent
        if (fired[i].events & internal::eET_Read) {
            if (!src->HandleReadEvent()) {
                src->HandleErrorEvent();
            }
        }

        if (fired[i].events & internal::eET_Write) {
            if (!src->HandleWriteEvent()) {
                src->HandleErrorEvent();
            }
        }

        if (fired[i].events & internal::eET_Error) {
            ANANAS_ERR << "eET_Error for " << src->Identifier();
            src->HandleErrorEvent();
        }
    }

    return ready >= 0;
}

bool EventLoop::InThisLoop() const {
    return this == g_thisLoop;
}

void EventLoop::ScheduleLater(std::chrono::milliseconds duration,
                              std::function<void()> f) {
    if (InThisLoop()) {
        ScheduleAfterWithRepeat<1>(duration, std::move(f));
    } else {
        Execute([=]() {
            ScheduleAfterWithRepeat<1>(duration, std::move(f));
        });
    }
}

// 执行某个函数f
void EventLoop::Schedule(std::function<void()> f) {
    Execute(std::move(f));
}

} // namespace ananas

