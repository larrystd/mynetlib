#ifndef BERT_SSLCONTEXT_H_
#define BERT_SSLCONTEXT_H_

#include <memory>
#include <string>
#include <functional>
#include "util/Buffer.h"


struct ssl_st;  // 头文件前向声明, 源文件再include, 可以降低因多次#include头文件造成的编译依赖(即一个头文件改了连带所有的都要重新编译)
typedef struct ssl_st SSL;

namespace ananas {

class Connection;

namespace ssl {

// SSL connection回调函数
void OnNewSSLConnection(const std::string& ctx, int verifyMode, bool incoming, Connection* conn);

class OpenSSLContext {
    friend void OnNewSSLConnection(const std::string&, int, bool, Connection* );

 public:
    explicit 
    OpenSSLContext(SSL* ssl, bool incoming_, void* exData);
    ~OpenSSLContext();

    bool SendData(const char* data, size_t len);
    void SetLogicProcess(std::function<size_t (Connection*, const char*, size_t)>);

    bool DoHandleShake();   // 握手
    void Shutdown();

private:
    static Buffer GetMemData(BIO* bio);
    static size_t ProcessHandshake(std::shared_ptr<OpenSSLContext> open, Connection* c, const char* data, size_t len);
    static size_t ProcessData(std::shared_ptr<OpenSSLContext> open, Connection* c, const char* data, size_t len);

    std::function<size_t (Connection* c, const char* data, size_t)> logicProcess_;

    SSL* ssl_; // ssl_st
    const bool incoming_;

    Buffer recvPlainBuf_;   // 接收缓存

    bool readWaitReadable_ {false};
    bool writeWaitReadable_ {false};

    Buffer sendBuffer_; // 发送Buffer

    bool shutdownWaitReadable_ {false};
};


} // end namespace ssl

} // end namespace ananas

#endif

