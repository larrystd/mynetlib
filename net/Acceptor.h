
#ifndef BERT_ACCEPTOR_H
#define BERT_ACCEPTOR_H

#include "Socket.h"
#include "Typedefs.h"

namespace ananas {
namespace internal {

class Acceptor : public Channel {   // 继承自Channel的Acceptor, Acceptor本身也是Channel对象
public:
    explicit Acceptor(EventLoop* loop);
    ~Acceptor();

    Acceptor(const Acceptor& ) = delete;    // 不可拷贝或赋值
    void operator= (const Acceptor& ) = delete;

    void SetNewConnCallback(NewTcpConnCallback cb); // 设置连接回调函数
    bool Bind(const SocketAddr& addr);  // 绑定地址addr

    int Identifier() const override;    // 返回acceptor建立的sockfd

    bool HandleReadEvent() override;    // 可读, 可写, 错误处理回调函数
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

private:
    int _Accept();

    SocketAddr peer_;
    int localSock_;
    uint16_t localPort_;    // 本地端口

    EventLoop* const loop_; // which loop belong to

    //register msg callback and on connect callback for conn
    NewTcpConnCallback newConnCallback_;    // 连接回调函数

    static const int kListenQueue;
};

} // namespace internal
} // namespace ananas

#endif

