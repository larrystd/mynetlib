
#include "Buffer.h"
#include <iostream>
#include <limits>
#include <cassert>

namespace ananas {

inline static std::size_t RoundUp2Power(std::size_t size) { // 实际大小能容纳size的2的指数
    if (size == 0)
        return 0;

    std::size_t roundUp = 1;
    while (roundUp < size)
        roundUp *= 2;

    return roundUp;
}

const std::size_t Buffer::kMaxBufferSize = std::numeric_limits<std::size_t>::max() / 2; // size_t最大的一半
const std::size_t Buffer::kHighWaterMark = 1 * 1024;
const std::size_t Buffer::kDefaultSize = 256;

std::size_t Buffer::PushData(const void* data, std::size_t size) {
    std::size_t bytes = PushDataAt(data, size);
    Produce(bytes);  // 写入了数据, 更新writepos, 移动byte

    assert(bytes == size);
    return bytes;   // 写入的字节数量
}

std::size_t Buffer::PushDataAt(const void* data, std::size_t size, std::size_t offset) {
    if (!data || size == 0)
        return 0;
    if (ReadableSize() + size + offset >= kMaxBufferSize)
        return 0; // overflow
    
    AssureSpace(size + offset); // 分配空间
    assert(size + offset <= WritableSize());    // 大小不能超过可写空间, 并没有自动增长空间的能力
    ::memcpy(&buffer_[writePos_ + offset], data, size);
    return size;
}

std::size_t Buffer::PopData(void* buf, std::size_t size) {
    std::size_t bytes = PeekDataAt(buf, size);  // 从Buffer拿数据
    Consume(bytes); // 拿数据后移动readpos

    return bytes;
}

void Buffer::Consume(std::size_t bytes) {
    assert (readPos_ + bytes <= writePos_);

    readPos_ += bytes;
    if (IsEmpty())  // 当writepos=readpos时
        Clear();
}

//@brief 取数据到buf中 
std::size_t Buffer::PeekDataAt(void* buf, std::size_t size, std::size_t offset) {
    const std::size_t dataSize = ReadableSize();
    if (!buf || size == 0 || dataSize <= offset)
        return 0;   // buf指向获得数据的Buffer对象, 不可为空
    
    if (size + offset > dataSize)
        size = dataSize - offset;   // 最多能取到dataSize的数据
    ::memcpy(buf, &buffer_[readPos_ + offset], size);
    return size;
}

void Buffer::AssureSpace(std::size_t needsize) {
    if (WritableSize() >= needsize) // 足够空间写
        return;
    const size_t dataSize = ReadableSize();
    const size_t oldCap = capacity_;

    while (WritableSize() + readPos_ < needsize) { // 尝试利用readpos前面的空间 
        if (capacity_ < kDefaultSize) { // 设置新的capacity
            capacity_ = kDefaultSize;
        } else if (capacity_ <= kMaxBufferSize) {
            const auto newCapacity = RoundUp2Power(capacity_);
            if (capacity_ < newCapacity)
                capacity_ = newCapacity;
            else
                capacity_ = 3 * newCapacity / 2;
        } else {
            assert(false);  // 需要处理, 逻辑bug问题
        }
    }

    if (oldCap < capacity_) {   // 确实需要扩大buffer大小
        std::unique_ptr<char []> tmp(new char[capacity_]);  // 使用new分配空间, 可以考虑优化成jtmalloc

        if (dataSize != 0)
            memcpy(&tmp[0], &buffer_[readPos_], dataSize);  // 拷贝到新空间
        buffer_.swap(tmp);  // buffer_也是std::unique_ptr<char[]>
    } else {
        memmove(&buffer_[0], &buffer_[readPos_], dataSize);  // 拷贝到新空间, memove可以防止内存覆盖
    }
    readPos_ = 0;
    writePos_ = dataSize;
}

void Buffer::Shrink() {
    if (IsEmpty()) {
        if (capacity_ > 8*1024) {
            Clear();
            capacity_ = 0;
            buffer_.reset();
        }
        return ;
    }
    
    std::size_t oldCap = capacity_;
    std::size_t dataSize = ReadableSize();
    if (dataSize > oldCap / 4)
        return;
    std::size_t newCap = RoundUp2Power(dataSize);

    std::unique_ptr<char []> tmp(new char[newCap]);
    memcpy(&tmp[0], &buffer_[readPos_], dataSize);
    buffer_.swap(tmp);
    capacity_ = newCap;

    readPos_  = 0;
    writePos_ = dataSize;    
}

void Buffer::Clear() {
    readPos_ = writePos_ = 0;   // 重置
}

void Buffer::Swap(Buffer& buf) {    // 移动三个核心变量
    std::swap(readPos_, buf.readPos_);
    std::swap(writePos_, buf.writePos_);
    std::swap(capacity_, buf.capacity_);
    buffer_.swap(buf.buffer_);
}

Buffer::Buffer(Buffer&& other) {
    _MoveFrom(std::move(other));
}

Buffer& Buffer::operator= (Buffer&& other) {
    return _MoveFrom(std::move(other));
}

Buffer& Buffer::_MoveFrom(Buffer&& other) { // other内容赋给this, other清空
    if (this != &other) {
        this->readPos_ = other.readPos_;
        this->writePos_ = other.writePos_;
        this->capacity_ = other.capacity_;
        this->buffer_ = std::move(other.buffer_);

        other.Clear();
        other.Shrink();
    }

    return *this;
}

} // namespace ananas

