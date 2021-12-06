#include <unistd.h>
#include <atomic>

#include "net/Connection.h"
#include "net/Application.h"
#include "util/Logger.h"

std::shared_ptr<ananas::Logger> logger;

size_t OnMessage(ananas::Connection* conn, const char* data, size_t len) {
    std::string rsp(data, len);
    // 传入的data是用户发送来的数据(缓冲区可读索引), 原封不动的返回
    conn->SendPacket(rsp.data(), rsp.size());
    return len;
}

void OnNewConnection(ananas::Connection* conn) {
    using ananas::Connection;

    conn->SetOnMessage(OnMessage);
    conn->SetOnDisconnect([](Connection* conn) {
        WRN(logger) << "OnDisConnect " << conn->Identifier();
    });
}

int main(int argc, char* argv[]) {
    size_t workers = 1;
    if (argc > 1)
        workers = (size_t)std::stoi(argv[1]);
    ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_server_test");

    auto& app = ananas::Application::Instance();
    app.SetNumOfWorker(workers);
    // 已经创建绑定监听了sockfd, 存储在acceptor中。已设置OnNewConnection, 有链接到来自动回调设置OnMessage
    app.Listen("127.0.0.1", 9987, OnNewConnection);

    app.Run(argc, argv);

    return 0;
}