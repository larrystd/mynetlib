#ifndef BERT_STRINGVIEW_H
#define BERT_STRINGVIEW_H

///@file StringView.h
///@brief Simply simulate C++17 std::string_view

#include <functional>
#include <string>
#include <ostream>

namespace ananas {

// stringView不是C++标准库string那种vector<char>, 而只是维护了const char*指针和len的高效率
// 毕竟string其实是动态数据, 因此要先将传入的string construct到内存中
class StringView {
public:
    using value_type = char;
    using pointer = char*;
    using const_pointer = const char*;
    using reference = char&;
    using const_reference = const char&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using const_iterator = const char* ;

    StringView();
    StringView(const char* );
    explicit // be care of string's lifetime
    StringView(const std::string& );
    StringView(const char*, size_t);
    StringView(const StringView& ) = default;

    const_reference operator[](size_t index) const;

    const_pointer Data() const;
    const_reference Front() const;
    const_reference Back() const;

    const_iterator begin() const;
    const_iterator end() const;

    size_type Size() const;
    bool Empty() const;

    void RemovePrefix(size_t n);
    void RemoveSuffix(size_t n);

    void Swap(StringView& other);
    StringView Substr(size_type pos = 0, size_type count = npos ) const;

    std::string ToString() const;

    static constexpr size_type npos = size_type(-1);
private:
    const char* data_;
    size_t len_;
};

bool operator==(const StringView& a, const StringView& b);  // 重载operator比较操作符
bool operator!=(const StringView& a, const StringView& b);
bool operator<(const StringView& a, const StringView& b);
bool operator>(const StringView& a, const StringView& b);
bool operator<=(const StringView& a, const StringView& b);
bool operator>=(const StringView& a, const StringView& b);

inline std::ostream& operator<< (std::ostream& os, const StringView& sv) {  // 重载operator<< 以能被ostream输出
    return os << sv.Data();
}

} // namespace ananas

namespace std {
template<>
struct hash<ananas::StringView> {   // StringView可以被hash(应该是方便unordered_map使用吧)
    typedef ananas::StringView argument_type;
    typedef std::size_t result_type;
    result_type operator()(const argument_type& sv) const noexcept {
        std::size_t result = 0;
        for (auto ch : sv) {
            result = (result * 131) + ch;
        }
        return result;
    }
};
}

#endif

