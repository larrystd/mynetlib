#ifndef BERT_CONNECTION_H
#define BERT_CONNECTION_H

#include <sys/types.h>
#include <string>

#include "Socket.h"
#include "Poller.h"
#include "Typedefs.h"
#include "ananas/util/Buffer.h"

namespace ananas {

namespace internal {    // 前向声明两个类
class Acceptor;
class Connector;
}

enum class ShutdownMode {   // 枚举类
    eSM_Both,
    eSM_Read,
    eSM_Write,
};

class Connection : public internal::Channel { // 连接对象, 继承自Channel。这里继承的类是前向声明的
 public:
    explicit Connection(EventLoop* loop);
    ~Connection();

    Connection(const Connection& ) = delete;    // 不可拷贝和赋值
    void operator= (const Connection& ) = delete;

    bool Init(int sock, const SocketAddr& peer);    // 用sockfd和peer addr初始化Connection
    const SocketAddr& Peer() const {
        return peer_;
    }

    void ActiveClose(); // 关闭Connection

    EventLoop* GetLoop() const {
        return loop_;   // 处理Connection的loop*
    }
    void Shutdown(ShutdownMode mode);
    void SetNodelay(bool enable);

    int Identifier() const override;    // Connection 维护的fd
    bool HandleReadEvent() override;    // Connection 可读, 可写, 错误事件回调函数
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    bool SendPacket(const void* data, std::size_t len); // 发送数据, 可以是void*, string对象, Buffer对象
    bool SendPacket(const std::string& data);
    bool SendPacket(const Buffer& buf);

    bool SendPacket(const BufferVector& datum);
    bool SendPacket(const SliceVector& slice);

    // 线程安全的发送数据
    bool SafeSend(const void* data, std::size_t len);
    bool SafeSend(const std::string& data);

    ///@brief Something internal.
    ///
    ///    When processing read event, pipeline requests made us handle many
    ///    requests at one time. If each response is ::send to network directyly,
    ///    there will be many small packets, lead to poor performance.
    ///    So if you call SetBatchSend(true), ananas will collect the small packets
    ///    together, after processing read events, they'll be send all at once.
    ///    The default value is true. But if your server process only one request at
    ///    one time, you should call SetBatchSend(false)
    void SetBatchSend(bool batch);

    void SetOnConnect(std::function<void (Connection* )> cb);   // 连接建立回调
    void SetOnDisconnect(std::function<void (Connection* )> cb);    // 连接断回调
    
    void SetOnMessage(TcpMessageCallback cb);   // 接受到数据流的回调
    void SetOnWriteComplete(TcpWriteCompleteCallback wccb); // 发送完数据流的回调

    ///@brief Set user's context pointer
    void SetUserData(std::shared_ptr<void> user);

    ///@brief Get user's context pointer
    template <typename T>
    std::shared_ptr<T> GetUserData() const;

    void SetMinPacketSize(size_t s);    // 包的最小大小, 小于这个等于没收到
    size_t GetMinPacketSize() const;

private:
    enum State {    // Connection状态State枚举
        eS_None,    // 空状态
        eS_Connected,
        eS_CloseWaitWrite,  // passive or active close, but I still have data to send
        eS_PassiveClose,    // should close
        eS_ActiveClose,     // should close
        eS_Error,           // waiting to handle error
        eS_Closed,          // final state being to destroy
    };

    friend class internal::Acceptor;
    friend class internal::Connector;

    void _OnConnect();
    int _Send(const void* data, size_t len);    // 发送数据

    EventLoop* const loop_;
    State state_ = State::eS_None;
    int localSock_;
    size_t minPacketSize_;

    Buffer recvBuf_;
    BufferVector sendBuf_;

    bool processingRead_{false};
    bool batchSend_{true};
    Buffer batchSendBuf_;

    SocketAddr peer_;

    std::function<void (Connection* )> onConnect_;  // 连接回调函数
    std::function<void (Connection* )> onDisconnect_;

    TcpMessageCallback onMessage_;  // function<size_t (Connection*, const char* data, size_t len)>
    TcpWriteCompleteCallback onWriteComplete_;

    std::shared_ptr<void> userData_;
};

template <typename T>
inline std::shared_ptr<T> Connection::GetUserData() const {
    return std::static_pointer_cast<T>(userData_);
}

} // namespace ananas

#endif

