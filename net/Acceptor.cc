#include <errno.h>
#include <cassert>
#include "EventLoop.h"
#include "Application.h"
#include "Connection.h"
#include "Acceptor.h"

#include "AnanasDebug.h"


namespace ananas {
namespace internal {

const int Acceptor::kListenQueue = 1024;

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
    newConnCallback_ = std::move(cb);
}

// 创建socketfd. 绑定地址并监听
bool Acceptor::Bind(const SocketAddr& addr) {   // 事实是创建一个socket并绑定addr
    if (!addr.IsValid())
        return false;

    if (localSock_ != kInvalid) {
        ANANAS_ERR << "Already listen " << localPort_;
        return false;
    }

    localSock_ = CreateTCPSocket();
    if (localSock_ == kInvalid)
        return false;

    localPort_ = addr.GetPort();

    SetNonBlock(localSock_);
    SetNodelay(localSock_);
    SetReuseAddr(localSock_);
    SetRcvBuf(localSock_);
    SetSndBuf(localSock_);

    auto serv = addr.GetAddr();

    int ret = ::bind(localSock_, (struct sockaddr*)&serv, sizeof serv);
    if (kError == ret) {
        ANANAS_ERR << "Cannot bind to " << addr.ToString();
        return false;
    }

    ret = ::listen(localSock_, kListenQueue);
    if (kError == ret) {
        ANANAS_ERR << "Cannot listen on " << addr.ToString();
        return false;
    }

    if (!loop_->Register(eET_Read, this->shared_from_this()))   // Acception是一种Channel, 将该Channel注册为可读eET_Read
        return false;

    ANANAS_INF << "Create listen socket " << localSock_
               << " on port " << localPort_;
    return  true;
}

int Acceptor::Identifier() const {
    return localSock_;  // 返回localSock_维护的fd
}

bool Acceptor::HandleReadEvent() {  // 处理可读回调函数， 即有新连接到到来。先用connfd封装新连接connection, 再将connection注册到poller, 最后执行onConnect回调
    while (true) {
        int connfd = _Accept(); // 执行accept获得connfd
        if (connfd != kInvalid) {   // connfd有效
            auto loop = Application::Instance().Next();
            // 将执行conn->_OnConnect()
            auto func = [loop, newCb = newConnCallback_, connfd, peer = peer_]() {
                auto conn(std::make_shared<Connection>(loop));  // 基于loop创建connection对象
                conn->Init(connfd, peer);   // 用connfd初始化conn
                if (loop->Register(eET_Read, conn)) {   // 注册新连接到Poll
                    newCb(conn.get());  // 新连接回调函数执行(这里面会设置信息回调)
                    conn->_OnConnect(); // conn执行_OnConnect()回调函数
                } else {
                    ANANAS_ERR << "Failed to register socket " << conn->Identifier();
                }
            };
            // 执行func
            loop->Execute(std::move(func));
        } else {
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

int Acceptor::_Accept() {   // 
    socklen_t addrLength = sizeof peer_;
    return ::accept(localSock_, (struct sockaddr *)&peer_, &addrLength);
}

} // end namespace internal
} // end namespace ananas

