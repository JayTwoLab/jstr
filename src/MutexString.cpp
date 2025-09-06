#include "MutexString.hpp"

namespace j2 {

#ifndef NDEBUG
thread_local const MutexString* MutexString::tls_owner_ = nullptr;
#endif

// ================= Locked 구현 =================
MutexString::Locked::Locked(std::string& s, std::mutex& m, const MutexString* owner)
    : s_(&s)
    , lock_(m)                 // ✅ lock_을 owner_보다 먼저 초기화
#ifndef NDEBUG
    , owner_(owner)
#endif
{
#ifndef NDEBUG
    // guard 수명 전체 동안 동일 객체의 다른 멤버 호출을 막기 위해 마크 설정
    assert(MutexString::tls_owner_ != owner_ && "동일 객체 no reentrancy(guard 보유 중 ms.* 호출)");
    MutexString::tls_owner_ = owner_;
    mark_set_ = true;
#endif
}

MutexString::Locked::Locked(const std::string& s, std::mutex& m, const MutexString* owner)
    : cs_(&s)
    , lock_(m)                 // ✅ lock_을 owner_보다 먼저 초기화
#ifndef NDEBUG
    , owner_(owner)
#endif
{
#ifndef NDEBUG
    assert(MutexString::tls_owner_ != owner_ && "동일 객체 no reentrancy(guard 보유 중 ms.* 호출)");
    MutexString::tls_owner_ = owner_;
    mark_set_ = true;
#endif
}

MutexString::Locked::~Locked() {
#ifndef NDEBUG
    if (mark_set_ && MutexString::tls_owner_ == owner_) {
        MutexString::tls_owner_ = nullptr;
    }
#endif
}

std::string* MutexString::Locked::operator->() { return s_; }
const std::string* MutexString::Locked::operator->() const { return cs_ ? cs_ : s_; }
std::string& MutexString::Locked::operator*() { return *s_; }
const std::string& MutexString::Locked::operator*() const { return cs_ ? *cs_ : *s_; }

void MutexString::Locked::unlock() {
    lock_.unlock();
#ifndef NDEBUG
    // 가드를 조기 해제하면 더 이상 owner 마크는 유지하지 않음
    if (mark_set_ && MutexString::tls_owner_ == owner_) {
        MutexString::tls_owner_ = nullptr;
        mark_set_ = false;
    }
#endif
}
bool MutexString::Locked::owns_lock() const { return lock_.owns_lock(); }

// protected 메서드: during guard lifetime만 안전
const char* MutexString::Locked::guard_cstr() const {
    return (cs_ ? cs_ : s_)->c_str();
}

// ================= CStrGuard 구현 =================
MutexString::CStrGuard::CStrGuard(const std::string& s, std::mutex& m)
    : lock_(m), p_(s.c_str()) {
    // NOTE: p_는 std::string 내부 버퍼 포인터이며,
    // CStrGuard 수명(=락 유지) 동안만 안전하게 사용할 수 있습니다.
}

// ================= MutexString 본체 =================
MutexString::MutexString(std::string s) : s_(std::move(s)) {}
MutexString::MutexString(const char* s) : s_(s ? s : "") {}

MutexString::MutexString(const MutexString& other) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(other.m_);
    s_ = other.s_;
}
MutexString::MutexString(MutexString&& other) noexcept {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(other.m_);
    s_ = std::move(other.s_);
}

MutexString& MutexString::operator=(const MutexString& other) {
    if (this != &other) {
#ifndef NDEBUG
        assert_not_reentrant_();
#endif
        std::scoped_lock lock(m_, other.m_);
        s_ = other.s_;
    }
    return *this;
}

MutexString& MutexString::operator=(MutexString&& other) noexcept {
    if (this != &other) {
#ifndef NDEBUG
        assert_not_reentrant_();
#endif
        std::scoped_lock lock(m_, other.m_);
        s_ = std::move(other.s_);
    }
    return *this;
}

// ===== std::string/char* 대입 =====
MutexString& MutexString::operator=(const std::string& rhs) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_);
    s_ = rhs;
    return *this;
}
MutexString& MutexString::operator=(const char* rhs) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_);
    s_ = (rhs ? rhs : "");
    return *this;
}

// ===== 비교 =====
bool MutexString::operator==(const std::string& rhs) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_);
    return s_ == rhs;
}
bool MutexString::operator==(const char* rhs) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_);
    return s_ == (rhs ? rhs : "");
}

// ===== 용량/상태 =====
std::size_t MutexString::size() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.size();
}
std::size_t MutexString::length() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.length();
}
bool MutexString::empty() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.empty();
}
std::size_t MutexString::capacity() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.capacity();
}
std::size_t MutexString::max_size() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.max_size();
}
void MutexString::reserve(std::size_t n) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.reserve(n);
}
void MutexString::shrink_to_fit() {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.shrink_to_fit();
}

// ===== 요소 접근(값 반환) + setter =====
char MutexString::at(std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.at(pos);
}
char MutexString::operator[](std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_[pos];
}
char MutexString::front() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.front();
}
char MutexString::back() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.back();
}
void MutexString::set(std::size_t pos, char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.at(pos) = ch;
}
void MutexString::front(char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.front() = ch;
}
void MutexString::back(char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.back() = ch;
}

// ===== 수정 =====
void MutexString::clear() {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.clear();
}
void MutexString::push_back(char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.push_back(ch);
}
void MutexString::pop_back() {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.pop_back();
}

void MutexString::assign(const std::string& s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_ = s;
}
void MutexString::assign(const char* s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_ = (s ? s : "");
}
void MutexString::assign(std::size_t count, char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.assign(count, ch);
}

MutexString& MutexString::append(const std::string& s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.append(s); return *this;
}
MutexString& MutexString::append(const char* s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.append(s ? s : ""); return *this;
}
MutexString& MutexString::append(std::size_t count, char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.append(count, ch); return *this;
}

MutexString& MutexString::operator+=(const std::string& s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_ += s; return *this;
}
MutexString& MutexString::operator+=(const char* s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_ += (s ? s : ""); return *this;
}
MutexString& MutexString::operator+=(char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_ += ch; return *this;
}

MutexString& MutexString::insert(std::size_t pos, const std::string& s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.insert(pos, s); return *this;
}
MutexString& MutexString::insert(std::size_t pos, const char* s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.insert(pos, s ? s : ""); return *this;
}
MutexString& MutexString::insert(std::size_t pos, std::size_t count, char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.insert(pos, count, ch); return *this;
}

MutexString& MutexString::erase(std::size_t pos, std::size_t count) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.erase(pos, count); return *this;
}

MutexString& MutexString::replace(std::size_t pos, std::size_t count, const std::string& s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.replace(pos, count, s); return *this;
}
MutexString& MutexString::replace(std::size_t pos, std::size_t count, const char* s) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.replace(pos, count, s ? s : ""); return *this;
}
MutexString& MutexString::replace(std::size_t pos, std::size_t count, std::size_t n, char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.replace(pos, count, n, ch); return *this;
}

void MutexString::resize(std::size_t n) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.resize(n);
}
void MutexString::resize(std::size_t n, char ch) {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); s_.resize(n, ch);
}

void MutexString::swap(MutexString& other) {
    if (this == &other) return;
#ifndef NDEBUG
    assert_not_reentrant_();
    other.assert_not_reentrant_();
#endif
    MutexString* first  = this < &other ? this : &other;
    MutexString* second = this < &other ? &other : this;
    std::scoped_lock lock(first->m_, second->m_);
    s_.swap(other.s_);
}
void MutexString::swap(std::string& other_str) {
#ifndef NDEBUG
    assert_not_reentrant_(); // 상대 std::string 이 공유 객체라면 별도 동기화 필요
#endif
    std::scoped_lock lock(m_); s_.swap(other_str);
}

// ===== 문자열 연산 =====
std::string MutexString::substr(std::size_t pos, std::size_t count) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.substr(pos, count);
}
std::size_t MutexString::copy(char* dest, std::size_t count, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.copy(dest, count, pos);
}
int MutexString::compare(const std::string& s) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.compare(s);
}
int MutexString::compare(std::size_t pos, std::size_t count, const std::string& s) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.compare(pos, count, s);
}

std::size_t MutexString::find(const std::string& s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find(s, pos);
}
std::size_t MutexString::find(const char* s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find(s ? s : "", pos);
}
std::size_t MutexString::find(char ch, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find(ch, pos);
}

std::size_t MutexString::rfind(const std::string& s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.rfind(s, pos);
}
std::size_t MutexString::rfind(const char* s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.rfind(s ? s : "", pos);
}
std::size_t MutexString::rfind(char ch, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.rfind(ch, pos);
}

std::size_t MutexString::find_first_of(const std::string& s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_first_of(s, pos);
}
std::size_t MutexString::find_first_of(const char* s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_first_of(s ? s : "", pos);
}
std::size_t MutexString::find_first_of(char ch, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_first_of(std::string(1, ch), pos);
}

std::size_t MutexString::find_last_of(const std::string& s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_last_of(s, pos);
}
std::size_t MutexString::find_last_of(const char* s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_last_of(s ? s : "", pos);
}
std::size_t MutexString::find_last_of(char ch, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_last_of(std::string(1, ch), pos);
}

std::size_t MutexString::find_first_not_of(const std::string& s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_first_not_of(s, pos);
}
std::size_t MutexString::find_first_not_of(const char* s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_first_not_of(s ? s : "", pos);
}
std::size_t MutexString::find_first_not_of(char ch, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_first_not_of(std::string(1, ch), pos);
}

std::size_t MutexString::find_last_not_of(const std::string& s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_last_not_of(s, pos);
}
std::size_t MutexString::find_last_not_of(const char* s, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_last_not_of(s ? s : "", pos);
}
std::size_t MutexString::find_last_not_of(char ch, std::size_t pos) const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_.find_last_not_of(std::string(1, ch), pos);
}

// ===== 안전 편의 =====
std::string MutexString::str() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    std::scoped_lock lock(m_); return s_;
}

// ===== 풀 API 접근 =====
MutexString::Locked MutexString::synchronize() {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    return Locked{s_, m_, this};
}
MutexString::Locked MutexString::synchronize() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    return Locked{s_, m_, this};
}

// ===== protected: RAII c_str() (외부에 노출 안 함) =====
MutexString::CStrGuard MutexString::c_str() const {
#ifndef NDEBUG
    assert_not_reentrant_();
#endif
    return CStrGuard{s_, m_};
}

} // namespace j2
