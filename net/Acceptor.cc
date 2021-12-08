#include <errno.h>
#include <cassert>
#include "EventLoop.h"
#include "Application.h"
#include "Connection.h"
#include "Acceptor.h"

#include "AnanasDebug.h"


namespace ananas {
namespace internal {

const int Acceptor::kListenQueue = 1024;    // 监听队列长度

Acceptor::Acceptor(EventLoop* loop) :
    localSock_(kInvalid),
    localPort_(SocketAddr::kInvalidPort),
    loop_(loop) {
}

Acceptor::~Acceptor() {
    CloseSocket(localSock_);
    ANANAS_INF << "Close Acceptor " << localPort_ ;
}


void Acceptor::SetNewConnCallback(NewTcpConnCallback cb) {
    newConnCallback_ = std::move(cb);   // 设置连接回调函数
}

// 创建socketfd. 绑定地址并监听
bool Acceptor::Bind(const SocketAddr& addr) {   // 事实是创建一个socket并绑定addr
    if (!addr.IsValid())
        return false;

    if (localSock_ != kInvalid) {
        ANANAS_ERR << "Already listen " << localPort_;
        return false;
    }

    localSock_ = CreateTCPSocket(); // 创建serer的sockfd
    if (localSock_ == kInvalid)
        return false;

    localPort_ = addr.GetPort();

    SetNonBlock(localSock_);    // 设置fd一些特性
    SetNodelay(localSock_);
    SetReuseAddr(localSock_);
    SetRcvBuf(localSock_);
    SetSndBuf(localSock_);

    auto serv = addr.GetAddr();

    int ret = ::bind(localSock_, (struct sockaddr*)&serv, sizeof serv); // 绑定地址
    if (kError == ret) {
        ANANAS_ERR << "Cannot bind to " << addr.ToString();
        return false;
    }

    ret = ::listen(localSock_, kListenQueue);   // 设置sockfd监听事件
    if (kError == ret) {
        ANANAS_ERR << "Cannot listen on " << addr.ToString();
        return false;
    }

    if (!loop_->Register(eET_Read, this->shared_from_this()))   // Acception是一种Channel, 将该Channel注册为可读eET_Read(注册到epoll中, 有相应eventloop自动调用连接回调)
        return false;

    ANANAS_INF << "Create listen socket " << localSock_
               << " on port " << localPort_;
    return  true;
}

int Acceptor::Identifier() const {
    return localSock_;  // 返回localSock_维护的serverfd
}

bool Acceptor::HandleReadEvent() {  // 处理可读回调函数,新连接到来eventloop响应自动调用该函数。先用传入connfd封装新连接connection, 再将connection注册到poller, 最后执行onConnect回调函数
    while (true) {
        int connfd = _Accept(); // 执行accept获得connfd
        if (connfd != kInvalid) {   // connfd有效
            auto loop = Application::Instance().Next(); // 获取Application的下一个有效的eventloop, 操作loop间接操作线程

            auto func = [loop, newCb = newConnCallback_, connfd, peer = peer_]() {
                auto conn(std::make_shared<Connection>(loop));  // 基于loop创建connection对象 conn
                conn->Init(connfd, peer);   // 用connfd初始化conn

                if (loop->Register(eET_Read, conn)) {   // 注册新连接到Poll
                    newCb(conn.get());  // conn.get()返回内部裸指针, 构造NewTcpConnCallback回调
                    conn->_OnConnect(); // conn执行_OnConnect()回调函数
                } else {
                    ANANAS_ERR << "Failed to register socket " << conn->Identifier();
                }
            };
            // 执行func
            loop->Execute(std::move(func));
        } else {    // 接受连接失败, 错误处理
            bool goAhead = false;
            const int error = errno;
            switch (error) {
            //case EWOULDBLOCK:
            case EAGAIN:
                return true; // it's fine

            case EINTR:
            case ECONNABORTED:
            case EPROTO:
                goAhead = true; // should retry
                break;

            case EMFILE:
            case ENFILE:
                ANANAS_ERR << "Not enough file descriptor available, error is "
                           << error
                           << ", CPU may 100%";
                return true;

            case ENOBUFS:
            case ENOMEM:
                ANANAS_ERR << "Not enough memory, limited by the socket buffer limits"
                           << ", CPU may 100%";
                return true;

            case ENOTSOCK:
            case EOPNOTSUPP:
            case EINVAL:
            case EFAULT:
            case EBADF:
            default:
                ANANAS_ERR << "BUG: error = " << error;
                assert (false);
                break;
            }

            if (!goAhead)
                return false;
        }
    }

    return true;
}

bool Acceptor::HandleWriteEvent() {
    assert (false);
    return false;
}

void Acceptor::HandleErrorEvent() {
    ANANAS_ERR << "Acceptor::HandleErrorEvent";
    loop_->Unregister(eET_Read, shared_from_this());
}

int Acceptor::_Accept() {   // server的accept, 返回connfd
    socklen_t addrLength = sizeof peer_;
    return ::accept(localSock_, (struct sockaddr *)&peer_, &addrLength);
}

} // namespace internal
} // namespace ananas

