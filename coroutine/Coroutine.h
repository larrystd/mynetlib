#ifndef BERT_COROUTINE_H_
#define BERT_COROUTINE_H_

// Only linux 额协程, 封装C语言的uncontext.h库的有栈协程

#include <ucontext.h>

#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace ananas {

using AnyPointer = std::shared_ptr<void>;   // shared_ptr维护的void指针

class Coroutine;
using CoroutinePtr = std::shared_ptr<Coroutine>;    // 维护协程的shared_ptr

class Coroutine {   // 协程
    enum class State {  // 协程的状态
        Init,
        Running,
        Finish,
    };

public:

// 使用可变模板参数表示执行函数类型F的参数类型Args, 同样的还有Args&&... 用了可变形参
// 静态变量
    template <typename F, typename... Args>
    static CoroutinePtr 
    CreateCoroutine(F&& f, Args&&... args) {
        return std::make_shared<Coroutine>(std::forward<F>(f), std::forward<Args>(args)...);
    }
 
    // Below three static functions for schedule coroutine

    // 类似python 生成器的关键字
    static AnyPointer Send(const CoroutinePtr& crt, AnyPointer = AnyPointer(nullptr));
    static AnyPointer Yield(const AnyPointer& = AnyPointer(nullptr));
    static AnyPointer Next(const CoroutinePtr& crt);

public:
    // !!!
    // NEVER define coroutine object, please use CreateCoroutine.
    // Coroutine constructor should be private,
    // BUT compilers demand template constructor must be public...
    explicit
    Coroutine(std::size_t stackSize = 0);

    // 如果F(Args) 返回值是void, 调用这个函数生成func_
    template <typename F, typename... Args, 
            typename = typename std::enable_if<std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type, typename Dummy = void>
    Coroutine(F&& f, Args&&... args) : Coroutine(kDefaultStackSize) {
        func_ = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    }

    // F(Args)执行返回后不是void类型, 会设置Coroutine object的result_成员作为执行返回的结果
    template <typename F, typename... Args,
            typename =  typename std::enable_if<!std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type>
    Coroutine(F&& f, Args&&... args) : Coroutine(kDefaultStackSize) {
        using ResultType = typename std::result_of<F (Args...)>::type;

        auto me = this;
        auto temp = std::bind(std::forward<F>(f), std::forward<Args>(args)...);     // 绑定了F(Args)
        func_ = [temp, me]() mutable {  // 这个lambda表达式对象是mutable的
            me->result_ = std::make_shared<ResultType>(temp());
        };
    }

    ~Coroutine();

    // no copyable
    Coroutine(const Coroutine&) = delete;
    void operator=(const Coroutine&) = delete;
    // no movable
    Coroutine(Coroutine&&) = delete;
    void operator=(Coroutine&&) = delete;

    unsigned int GetID() const  {
        return  id_;
    }
    static unsigned int GetCurrentID()  {
        return current_->id_;
    }

private:
// 内部实现函数, 实现了静态函数Send, Yield, Run
    AnyPointer _Send(Coroutine* crt, AnyPointer = AnyPointer(nullptr));
    AnyPointer _Yield(const AnyPointer& = AnyPointer(nullptr));
    static void _Run(Coroutine* cxt);

    unsigned int id_;  // 1: main
    State state_;
    AnyPointer yieldValue_;

    typedef ucontext_t HANDLE;

    static const std::size_t kDefaultStackSize = 8 * 1024;
    std::vector<char> stack_;   // 栈, 用来存储协程上下文

    HANDLE handle_;
    std::function<void ()> func_;
    AnyPointer result_; // F(Args)执行得到的结果

    static Coroutine main_;
    static Coroutine* current_;
    static unsigned int sid_;
};

} // namespace ananas

#endif
