
#ifndef BERT_BUFFER_H
#define BERT_BUFFER_H

#include <cstring>
#include <memory>
#include <list>

namespace ananas {

///@brief A simple buffer with memory management like STL's vector<char>
class Buffer {  // 缓冲, 空-readpos-可用数据-write-空, 只有readpos和writepos之间的数据是有数据的区域
public:
    Buffer() :
        readPos_(0),    // 三个指针, readpos, writepos, capacity
        writePos_(0),
        capacity_(0) {
    }

    Buffer(const void* data, size_t size) :
        readPos_(0),
        writePos_(0),
        capacity_(0) {
        PushData(data, size);
    }

    Buffer(const Buffer& ) = delete;
    void operator = (const Buffer& ) = delete;

    Buffer(Buffer&& ) ;
    Buffer& operator = (Buffer&& ) ;

    std::size_t PushDataAt(const void* data, std::size_t size, std::size_t offset = 0); // 在缓存某个位置push数据
    std::size_t PushData(const void* data, std::size_t size);

    void Produce(std::size_t bytes) {
        writePos_ += bytes;
    }   // write之后更新writepos

    std::size_t PeekDataAt(void* pBuf, std::size_t size, std::size_t offset = 0);   // 在某个位置移出数据, offset是相对于writepos的offset, 一般为0
    std::size_t PopData(void* pBuf, std::size_t size);
    void Consume(std::size_t bytes);

    char* ReadAddr()  {
        return &buffer_[readPos_];
    }   // readpos所在地址, 可读地址
    char* WriteAddr() {
        return &buffer_[writePos_];
    }

    bool IsEmpty() const {
        return ReadableSize() == 0;
    }
    std::size_t ReadableSize() const {
        return writePos_ - readPos_;
    }   // 可读数据的大小
    std::size_t WritableSize() const {
        return capacity_ - writePos_;
    }   // 可写的数据
    std::size_t Capacity() const {
        return capacity_;
    }

    void Shrink();
    void Clear();
    void Swap(Buffer& buf);

    void AssureSpace(std::size_t size);

    static const std::size_t  kMaxBufferSize;   // 常数
    static const std::size_t  kHighWaterMark;
    static const std::size_t  kDefaultSize;

private:
    Buffer& _MoveFrom(Buffer&& );

    std::size_t readPos_;
    std::size_t writePos_;
    std::size_t capacity_;
    std::unique_ptr<char []>  buffer_;  // Buffer内部维护的buffer_
};

struct BufferVector {   // 多个Buffer对象组成的list
    static constexpr int kMinSize = 1024;

    typedef std::list<Buffer> BufferContainer;   // Buffer对象链表

    // 以下类型定义借助了std::list<>
    typedef BufferContainer::const_iterator const_iterator;
    typedef BufferContainer::iterator iterator;
    typedef BufferContainer::value_type value_type;
    typedef BufferContainer::reference reference;
    typedef BufferContainer::const_reference const_reference;

    BufferVector() {}

    BufferVector(Buffer&& first) {
        Push(std::move(first));
    }

    bool Empty() const {
        return buffers.empty();
    }

    std::size_t TotalBytes() const {
        return totalBytes;
    }

    void Clear() {
        buffers.clear();
        totalBytes = 0;
    }

    void Push(Buffer&& buf) {
        totalBytes += buf.ReadableSize();
        if (_ShouldMerge()) {
            auto& last = buffers.back();
            last.PushData(buf.ReadAddr(), buf.ReadableSize());  // buf 的内容push到buffer.last()中
        } else {
            buffers.push_back(std::move(buf));
        }
    }

    void Push(const void* data, size_t size) {  // 将void* data表示的数据
        totalBytes += size;
        if (_ShouldMerge()) {
            auto& last = buffers.back();
            last.PushData(data, size);  // 在buffer.last后面push数据
        } else{
            buffers.push_back(Buffer(data, size));
        }
    }

    void pop() {
        totalBytes -= buffers.front().ReadableSize();    // 缓冲列表的front移出数据
        buffers.pop_front();
    }
    iterator begin() {  // 返回的迭代器是指向list节点
        return buffers.begin();
    }
    iterator end() {
        return buffers.end();
    }
    const_iterator begin() const {  // const迭代器可以作为只读迭代器
        return buffers.begin();
    }

    const_iterator end() const {
        return buffers.end();
    }

    const_iterator cbegin() const {
        return buffers.cbegin();
    }
    const_iterator cend() const {
        return buffers.cend();
    }

    BufferContainer buffers;    // buffer缓冲列表
    size_t totalBytes {0};

 private:
    bool _ShouldMerge() const {
        if (buffers.empty()) {
            return false;
        } else{
            const auto& last = buffers.back();
            return last.ReadableSize() < kMinSize;    // 最后一个缓冲单元可读区较小
        }
    }
};

struct Slice {  //Slice可以看成C语言的char*, 但增加了len保证安全, 类似redis的sds
    const void* data;
    size_t len;

    explicit
    Slice(const void* d = nullptr, size_t l = 0) :
        data(d),
        len(l)
    {}
};

struct SliceVector {    // 多个Slice对象组成的list
private:
    typedef std::list<Slice> Slices;

public:

    typedef Slices::const_iterator const_iterator;
    typedef Slices::iterator iterator;
    typedef Slices::value_type value_type;
    typedef Slices::reference reference;
    typedef Slices::const_reference const_reference;

    iterator begin() {
        return slices.begin();
    }
    iterator end() {
        return slices.end();
    }

    const_iterator begin() const {
        return slices.begin();
    }
    const_iterator end() const {
        return slices.end();
    }

    const_iterator cbegin() const {
        return slices.cbegin();
    }
    const_iterator cend() const {
        return slices.cend();
    }

    bool Empty() const {
        return slices.empty();
    }

    void PushBack(const void* data, size_t len) {
        slices.push_back(Slice(data, len));
    }

private:
    Slices slices;
};

} // namespace ananas

#endif

