#include <cassert>
#include <stdexcept>
#include <string>
#include "Coroutine.h"

namespace ananas {

// #include的变量除了inline, 其他为声音, 编译单元设需要重新定义初始化, 包括static。
// 这些变量先于main函数开始, 注意创建了main Coroutine对象
unsigned int Coroutine::sid_ = 0;
Coroutine Coroutine::main_; // 表示main函数执行的上下文情况
Coroutine* Coroutine::current_ = nullptr;   // 表示当前协程, 默认指向main

/*
基于#include <ucontext.h>实现协程依赖的四大函数 
int getcontext(ucontext_t *ucp);
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);
int swapcontext(ucontext_t *oucp, ucontext_t *ucp);
int setcontext(const ucontext_t *ucp);
*/
// 构造函数, 初始化ucontext, 协程要执行的函数等
Coroutine::Coroutine(std::size_t size) :
    id_(++ sid_),   // 初始化id
    state_(State::Init),    // 初始化状态
    stack_(size > kDefaultStackSize ? size : kDefaultStackSize)
{
    if (this == &main_) {   // 如果是创建main协程, main协程是静态变量, 不是外界调用
        std::vector<char>().swap(stack_);   // stack清空, 返回
        return;
    }
    
/*
    if (id_ == main_.id_) 
        id_ = ++sid_;
    int ret = ::getcontext(&handle_);   // 保存上下文到handle_
    assert(ret == 0);

    handle_.uc_stack.ss_sp = &stack_[0];    // 修改内容到初始化
    handle_.uc_stack.ss_size = stack_.size();   // 栈地址和栈大小
    handle_.uc_link = 0;    // 说明handle_ context可能被执行恢复等操作

    // 设置好handle_
    //::makecontext(&handle_, reinterpret_cast<void (*)(void)>(&Coroutine::_Run), 1, this); // 转为void (*)(Void) 函数指针类型, 作为协程要执行的任务
    ::makecontext(&handle_, reinterpret_cast<void (*)(void)>(&Coroutine::_Run), 1, this);
*/
    if (id_ == main_.id_)
        id_ = ++ sid_;  // when sid_ overflow

    int ret = ::getcontext(&handle_);
    assert (ret == 0);

    handle_.uc_stack.ss_sp   = &stack_[0];
    handle_.uc_stack.ss_size = stack_.size();
    handle_.uc_link = 0;

    ::makecontext(&handle_, reinterpret_cast<void (*)(void)>(&Coroutine::_Run), 1, this);
}

Coroutine::~Coroutine() {
}

AnyPointer Coroutine::_Send(Coroutine* crt, AnyPointer param) {
    assert (crt);
    assert (this == current_);
    assert (this != crt);

    current_ = crt;

    if (param) {
        if (crt->state_ == State::Init && crt != &Coroutine::main_)
            throw std::runtime_error("Can't send non-void value to a just-created coroutine");
        
        this->yieldValue_ = std::move(param);   // 要返回给上下文return的值, 可以传给新协程上下文
    }

    int ret = ::swapcontext(&handle_, &crt->handle_);   // 保存当前上下文到handle_(是current_, main_协程的成员), 转而执行crt->handle_
    if (ret != 0) {
        perror("FATAL ERROR: swapcontext");
        throw std::runtime_error("FATAL ERROR: swapcontext failed");
    }

    return std::move(crt->yieldValue_); // 返回当前上下文的值
}

AnyPointer Coroutine::_Yield(const AnyPointer& param) {
    return _Send(&main_, param);    // 调用_Send(), 返回main上下文, param是要return的值
}

void Coroutine::_Run(Coroutine* crt) {  // 协程初始化执行
    assert (&Coroutine::main_ != crt);
    assert (Coroutine::current_ == crt);

    crt->state_ = State::Running;   // 状态

    if (crt->func_) // 有函数执行函数
        crt->func_();
    
    crt->state_ = State::Finish;
    crt->_Yield(crt->result_);
}

// 这是静态函数
AnyPointer Coroutine::Send(const CoroutinePtr& crt, AnyPointer param) {
    if (crt->state_ == Coroutine::State::Finish) {
        throw std::runtime_error("Send to a finished coroutine.");
        return AnyPointer();
    }

    if (!Coroutine::current_) {
        Coroutine::current_ = &Coroutine::main_;    // main协程
    }

    return Coroutine::current_->_Send(crt.get(), param);    // 实际是调用handle_去执行
}

AnyPointer Coroutine::Yield(const AnyPointer& param) {
    return Coroutine::current_->_Yield(param);
}

AnyPointer Coroutine::Next(const CoroutinePtr& crt) {
    return Coroutine::Send(crt);
}

} // namespace ananas

