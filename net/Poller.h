#ifndef BERT_POLLER_H
#define BERT_POLLER_H

#include <vector>
#include <memory>
#include <stdio.h>

namespace ananas {
///@brief namespace internal, not exposed to user.
namespace internal {

enum EventType {    // 事件类型
    eET_None  = 0,
    eET_Read  = 0x1 << 0,
    eET_Write = 0x1 << 1,
    eET_Error = 0x1 << 2,
};

class Channel : public std::enable_shared_from_this<Channel> {  // 继承std::enable_shared_from_this, 可以用shared_from_this了
public:
    Channel() {
        printf("New channel %p\n", (void*)this);
    }
    virtual ~Channel() {    // 虚析构函数
        printf("Delete channel %p\n", (void*)this);
    }

    Channel(const Channel& ) = delete;  // 不能拷贝或者赋值
    void operator=(const Channel& ) = delete;

    virtual int Identifier() const = 0; // 返回Channel的sockfd

    ///@brief The unique id, it'll not repeat in whole process.
    unsigned int GetUniqueId() const {
        return unique_id_;
    }
    ///@brief Set the unique id, it's called by library.
    void SetUniqueId(unsigned int id) {
        unique_id_ = id;
    }

    // 回调函数, 也是虚的函数
    virtual bool HandleReadEvent() = 0;
    virtual bool HandleWriteEvent() = 0;
    virtual void HandleErrorEvent() = 0;

private:
    unsigned int unique_id_ = 0; // dispatch by ioloop
};


struct FiredEvent { // 过期事件
    int   events;
    void* userdata;

    FiredEvent() : events(0), userdata(nullptr) {
    }
};

class Poller {  // 负责poll连接
 public:
    Poller() : multiplexer_(-1) {

    }
    virtual ~Poller() {

    }

    virtual bool Register(int fd, int events, void* userPtr) = 0;   // 注册fd到poll
    virtual bool Modify(int fd, int events, void* userPtr) = 0; // 修改fd的状态
    virtual bool Unregister(int fd, int events) = 0;

    virtual int Poll(std::size_t maxEv, int timeoutMs) = 0; // 获取活跃连接
    const std::vector<FiredEvent>& GetFiredEvents() const {
        return firedEvents_;
    }

 protected:
    int multiplexer_;
    std::vector<FiredEvent> firedEvents_;   // 活跃的事件列表
};

} // namespace internal
} // namespace ananas

#endif

