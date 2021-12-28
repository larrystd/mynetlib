#include "SSLManager.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace ananas {

namespace ssl {

SSLManager::SSLManager() {
}

SSLManager& SSLManager::Instance() {
    static SSLManager mgr;
    return mgr;
}

void SSLManager::GlobalInit() { // SSL 的全局初始化
    (void)SSL_library_init();
    OpenSSL_add_all_algorithms();

    ERR_load_ERR_strings();
    SSL_load_error_strings();
}

SSLManager::~SSLManager() { // 析构函数
    for (const auto& e : ctxSet_)
        SSL_CTX_free(e.second);

    ERR_free_strings();
    EVP_cleanup();
}

bool SSLManager::AddCtx(const std::string& name,
                        const std::string& cafile,
                        const std::string& certfile,
                        const std::string& keyfile) // 传入的参数是证书文件
{
    if (ctxSet_.count(name)) 
        return false;
    
    // 创建ctx 结构体
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_method());
    if (!ctx)   
        return false;
    
    // 关闭SSLV2 & SSLV3
    SSL_CTX_clear_options(ctx, SSL_OP_NO_SSLv2);
    SSL_CTX_clear_options(ctx, SSL_OP_NO_SSLv3);

// 如果失败返回错误宏定义
#define RETURN_IF_FAIL(call) \
    if ((call) <= 0) {      \
        ERR_print_errors_fp(stderr);    \
        return false;   \
    }

    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

    // 设置ctx的内容, 证书, 私钥等并检查
    RETURN_IF_FAIL (SSL_CTX_set_session_id_context(ctx, (const unsigned char*)ctx, sizeof ctx));
    RETURN_IF_FAIL (SSL_CTX_load_verify_locations(ctx, cafile.data(), nullptr));
    RETURN_IF_FAIL (SSL_CTX_use_certificate_file(ctx, certfile.data(), SSL_FILETYPE_PEM));
    RETURN_IF_FAIL (SSL_CTX_use_PrivateKey_file(ctx, keyfile.data(), SSL_FILETYPE_PEM));
    RETURN_IF_FAIL (SSL_CTX_check_private_key(ctx));

// 之后的代码不能使用RETURN_IF_FAIL这个宏定义了
#undef RETURN_IF_FAIL 
    // ctx_加入到维护
    return ctxSet_.insert({name, ctx}).second;
}

// 根据string name返回ctxSet_ map的SSL_CTX*
SSL_CTX* SSLManager::GetCtx(const std::string& name) const {
    auto it  = ctxSet_.find(name);
    return it == ctxSet_.end() ? nullptr : it->second;
}

} // end namespace ssl

} // end namespace ananas

