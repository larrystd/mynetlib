#include <cassert>
#include <iostream>
#include <string>
#include "Coroutine.h"


using std::cerr;
using std::endl; // for test

using namespace ananas;

// A coroutine: simply twice the input integer and return
int Double(int input) {
    cerr << "Coroutine Double: got input "
         << input
         << endl;

    cerr << "Coroutine Double: Return to Main." << endl;
    auto rsp = Coroutine::Yield(std::make_shared<std::string>("I am calculating, please wait...")); // 暂停执行, 返回原保存的上下文, 内容会传给恢复的上下文

    cerr << "Coroutine Double is resumed from Main\n";

    cerr<< "Coroutine Double: got message \""
        << *std::static_pointer_cast<std::string>(rsp) << "\""
        << endl;

    cerr << "Exit " << __FUNCTION__ << endl;

    // twice the input
    return input * 2;   // 直接return, 这个协程函数执行完毕。
}



int main() {
    // 一个上下文crt, 可以表示一个执行的函数。因此这里main函数会有一个协程, crt用来表示Double执行的情况
    const int input = 42;

    //1. create coroutine
    auto crt(Coroutine::CreateCoroutine(Double, input));    // 利用F和Args创建一个协程对象

    //2. start crt, get result from crt
    auto ret = Coroutine::Send(crt);    // 表示暂停上下文, crt继续执行, 也就是Double函数, ret是传来的值
    
    cerr << "Main func: got reply message \"" << *std::static_pointer_cast<std::string>(ret).get() << "\""<< endl;

    //3. got the final result: 84
    auto finalResult = Coroutine::Send(crt, std::make_shared<std::string>("Please be quick, I am waiting for your result"));    // 暂停, 转换协程继续执行
    cerr << "Main func: got the twice of " << input << ", answer is " << *std::static_pointer_cast<int>(finalResult) << endl;

    cerr << "BYE BYE\n";
}

