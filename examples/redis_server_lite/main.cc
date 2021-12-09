#include <iostream>
#include <unistd.h>
#include "RedisContext.h"
#include "net/Application.h"

#include "RedisLog.h"

std::shared_ptr<ananas::Logger> logger;

void OnConnect(std::shared_ptr<RedisContext> ctx, ananas::Connection* conn) {
    std::cout << "RedisContext.OnConnect " << conn->Identifier() << std::endl;
}

void OnNewConnection(ananas::Connection* conn) {    // 该函数有设置信息回调, 用于对客户端提供服务
    std::cout << "OnNewConnection " << conn->Identifier() << std::endl;

    auto ctx = std::make_shared<RedisContext>(conn);

    conn->SetBatchSend(true);
    conn->SetOnConnect(std::bind(&OnConnect, ctx, std::placeholders::_1));
    conn->SetOnMessage(std::bind(&RedisContext::OnRecv, ctx, std::placeholders::_1,
                                 std::placeholders::_2,
                                 std::placeholders::_3));
}

bool Init(int ac, char* av[]) {
    uint16_t port = 6379;

    int ch = 0;
    while ((ch = getopt(ac, av, "p:")) != -1) {
        switch (ch) {
        case 'p':
            port = std::stoi(optarg);
            break;

        default:
            return false;
        }
    }

    auto& app = ananas::Application::Instance();
    app.Listen("loopback", port, OnNewConnection);  // 监听端口
    return true;
}

int main(int argc, char* argv[]) {
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_server_test");

    auto& app = ananas::Application::Instance();
    app.SetOnInit(Init);    // init函数, 执行包括socket创建listen在内, 等待客户端连接之前的一系列工作

    app.Run(argc, argv);    // 注册sockfd, 得以accept连接

    return 0;
}