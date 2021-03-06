#include <unistd.h>
#include <cassert>

#include "Socket.h"
#include "PipeChannel.h"

namespace ananas {

namespace internal {

PipeChannel::PipeChannel() {    // 也就创建了管道fd, 通信管道
    int fd[2];
    int ret = ::pipe(fd);
    assert (ret == 0);
    readFd_ = fd[0];
    writeFd_ = fd[1];
    SetNonBlock(readFd_, true);
    SetNonBlock(writeFd_, true);
}

PipeChannel::~PipeChannel() {
    ::close(readFd_);
    ::close(writeFd_);
}

int PipeChannel::Identifier() const {
    return readFd_;
}

bool PipeChannel::HandleReadEvent() {
    char ch;
    auto n = ::read(readFd_, &ch, sizeof ch);
    return n == 1;
}

bool PipeChannel::HandleWriteEvent() {
    assert (false);
    return false;
}

void PipeChannel::HandleErrorEvent() {
}

bool PipeChannel::Notify() {
    char ch = 0;
    auto n = ::write(writeFd_, &ch, sizeof ch);
    return n == 1;
}

} // end namespace internal

} // end namespace ananas

