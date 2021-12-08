#ifdef __gnu_linux__

#include "Epoller.h"

#include <errno.h>
#include <unistd.h>

#include "AnanasDebug.h"

namespace ananas {
namespace internal {

namespace Epoll {
bool ModSocket(int epfd, int socket, uint32_t events, void* ptr);

bool AddSocket(int epfd, int socket, uint32_t events, void* ptr) {  // epoll注册socket
    if (socket < 0)
        return false;

    epoll_event  ev;
    ev.data.ptr= ptr;
    ev.events = 0;

    if (events & eET_Read)  // 设置事件
        ev.events |= EPOLLIN;
    if (events & eET_Write)
        ev.events |= EPOLLOUT;

    return 0 == epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &ev);    // epoll_ctl注册socket, event到 epfd
}

bool DelSocket(int epfd, int socket) {
    if (socket < 0)
        return false;

    epoll_event dummy;
    return 0 == epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &dummy) ;
}

bool ModSocket(int epfd, int socket, uint32_t events, void* ptr) {  // 修改socket, event类型epoll_ctl
    if (socket < 0)
        return false;

    epoll_event  ev;
    ev.data.ptr= ptr;
    ev.events = 0;

    if (events & eET_Read)
        ev.events |= EPOLLIN;
    if (events & eET_Write)
        ev.events |= EPOLLOUT;

    return 0 == epoll_ctl(epfd, EPOLL_CTL_MOD, socket, &ev);
}
}


Epoller::Epoller() {
    multiplexer_ = ::epoll_create(512); // 创建epoll_create
    ANANAS_DBG << "create epoll: " << multiplexer_;
}

Epoller::~Epoller() {
    if (multiplexer_ != -1) {
        ANANAS_DBG << "close epoll:  " << multiplexer_;
        ::close(multiplexer_);
    }
}

bool Epoller::Register(int fd, int events, void* userPtr) { // 新增socket于epoll中
    if (Epoll::AddSocket(multiplexer_, fd, events, userPtr))
        return true;

    return (errno == EEXIST) && Modify(fd, events, userPtr);
}

bool Epoller::Unregister(int fd, int events) {
    return Epoll::DelSocket(multiplexer_, fd);
}


bool Epoller::Modify(int fd, int events, void* userPtr) {
    if (events == 0)
        return Unregister(fd, 0);

    if (Epoll::ModSocket(multiplexer_, fd, events, userPtr))
        return  true;

    return  errno == ENOENT && Register(fd, events, userPtr);
}


int Epoller::Poll(size_t maxEvent, int timeoutMs) { // 获取活跃事件
    if (maxEvent == 0)
        return 0;

    while (events_.size() < maxEvent)
        events_.resize(2 * events_.size() + 1);

    int nFired = TEMP_FAILURE_RETRY(::epoll_wait(multiplexer_, &events_[0], maxEvent, timeoutMs));  // 活跃的事件,&events_[0]是事件列表第一个元素的地址 
    if (nFired == -1 && errno != EINTR && errno != EWOULDBLOCK)
        return -1;

    auto& events = firedEvents_;    // events是firedEvents_的引用
    if (nFired > 0)
        events.resize(nFired);

    for (int i = 0; i < nFired; ++ i) {
        FiredEvent& fired = events[i];  // fired是events[]的引用, 这一切都是在修改std::vector<FiredEvent> firedEvents_;
        fired.events   = 0;
        fired.userdata = events_[i].data.ptr;

        if (events_[i].events & EPOLLIN)
            fired.events  |= eET_Read;

        if (events_[i].events & EPOLLOUT)
            fired.events  |= eET_Write;

        if (events_[i].events & (EPOLLERR | EPOLLHUP))
            fired.events  |= eET_Error;
    }

    return nFired;
}

} // namespace internal
} // namespace ananas

#endif

