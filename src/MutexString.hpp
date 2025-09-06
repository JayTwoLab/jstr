#pragma once
#include <string>
#include <mutex>
#include <utility>
#include <type_traits>
#include <cassert>

// j2 namespace
namespace j2 {

// thread-safe string wrapper
// - only members are std::string, std::mutex 두 개만 유지
// - std::string API는 가능한 한 동일/유사 시그니처로 제공
// - returning pointer/iterator/reference directly is dangerous, so 가드(guard) 또는 스냅샷(str)로만 접근
class MutexString {
public:
    // locked view(가드): 수명 동안 뮤텍스를 잡고 내부 std::string 에 직접 접근
    class Locked {
    public:
        // ⚠️ reentrancy detection를 위해 owner(protected 대상 객체) 포인터를 함께 전달받습니다.
        Locked(std::string& s, std::mutex& m, const MutexString* owner);
        Locked(const std::string& s, std::mutex& m, const MutexString* owner);
        ~Locked(); // release reentrancy mark in debug mode

        // during guard lifetime 내부 std::string 전체 API 사용 가능
        std::string* operator->();
        const std::string* operator->() const;
        std::string& operator*();
        const std::string& operator*() const;

        // early unlock and status check if needed
        [[deprecated("unlock()는 특별한 경우가 아니면 사용을 지양하십시오. 가드 수명 범위를 좁히세요.")]]
        void unlock();
        bool owns_lock() const;

    protected:
        // ⚠ protected: 외부 코드에서 직접 포인터를 꺼내 쓰는 헬퍼를 차단(오남용 방지)
        const char* guard_cstr() const;  // == (cs_ ? cs_ : s_)->c_str()

        // 내부 상태
        std::string* s_ = nullptr;
        const std::string* cs_ = nullptr;
        std::unique_lock<std::mutex> lock_;

        // 디버그 재진입 제어용
#ifndef NDEBUG
        const MutexString* owner_ = nullptr;
        bool mark_set_ = false;
#endif

        friend class MutexString; // MutexString 내부에서 접근 가능
    };

    // RAII pointer guard(internal use)
    // - get()/operator const char*() 제공
    // - keep lock during object lifetime → 함수 인자에 바로 넣어도 호출 동안 안전
    class CStrGuard {
    public:
        CStrGuard(const std::string& s, std::mutex& m);
        const char* get() const { return p_; }
        operator const char*() const { return p_; } // 직접 인자 전달 허용
        CStrGuard(const CStrGuard&) = delete;
        CStrGuard& operator=(const CStrGuard&) = delete;
    private:
        std::unique_lock<std::mutex> lock_;
        const char* p_ = nullptr;
    };

public:
    // ===== 생성/대입 =====
    MutexString() = default;                 // empty string

    // ⬇⬇⬇ explicit 제거 → "j2::MutexString ms = "start";" / "jstr ms = "start";" 가능
    MutexString(std::string s);
    MutexString(const char* s);

    MutexString(const MutexString& other);
    MutexString(MutexString&& other) noexcept;
    MutexString& operator=(const MutexString& other);
    MutexString& operator=(MutexString&& other) noexcept;

    // 표준 문자열/char* 대입 (쓰기)
    MutexString& operator=(const std::string& rhs);
    MutexString& operator=(const char* rhs);

    // 비교 (읽기)
    bool operator==(const std::string& rhs) const;
    bool operator==(const char* rhs) const;
    bool operator!=(const std::string& rhs) const { return !(*this == rhs); }
    bool operator!=(const char* rhs) const { return !(*this == rhs); }
    friend inline bool operator==(const std::string& lhs, const MutexString& rhs) { return rhs == lhs; }
    friend inline bool operator==(const char* lhs, const MutexString& rhs) { return rhs == lhs; }
    friend inline bool operator!=(const std::string& lhs, const MutexString& rhs) { return !(rhs == lhs); }
    friend inline bool operator!=(const char* lhs, const MutexString& rhs) { return !(rhs == lhs); }

    // ===== 용량/상태 =====
    std::size_t size() const;
    std::size_t length() const;
    bool empty() const;
    std::size_t capacity() const;
    std::size_t max_size() const;
    void reserve(std::size_t n);
    void shrink_to_fit();

    // ===== 요소 접근(읽기 전용 값 반환) + 짧은 setter =====
    // note: 참조 반환(operator[], at의 char&)은 안전 문제로 제공하지 않음
    char at(std::size_t pos) const;
    char operator[](std::size_t pos) const;     // 읽기 전용 값 반환
    char front() const;                          // 게터
    char back() const;                           // 게터

    void set(std::size_t pos, char ch);         // set_at 대체 (짧은 이름)
    void front(char ch);                         // setter 오버로드
    void back(char ch);                          // setter 오버로드

    // ===== 수정 =====
    void clear();
    void push_back(char ch);
    void pop_back();

    void assign(const std::string& s);
    void assign(const char* s);
    void assign(std::size_t count, char ch);

    // append (체이닝: MutexString& 반환)
    MutexString& append(const std::string& s);
    MutexString& append(const char* s);
    MutexString& append(std::size_t count, char ch);

    // operator+=
    MutexString& operator+=(const std::string& s);
    MutexString& operator+=(const char* s);
    MutexString& operator+=(char ch);

    // insert
    MutexString& insert(std::size_t pos, const std::string& s);
    MutexString& insert(std::size_t pos, const char* s);
    MutexString& insert(std::size_t pos, std::size_t count, char ch);

    // erase
    MutexString& erase(std::size_t pos = 0, std::size_t count = std::string::npos);

    // replace
    MutexString& replace(std::size_t pos, std::size_t count, const std::string& s);
    MutexString& replace(std::size_t pos, std::size_t count, const char* s);
    MutexString& replace(std::size_t pos, std::size_t count, std::size_t n, char ch);

    // resize
    void resize(std::size_t n);
    void resize(std::size_t n, char ch);

    // swap
    void swap(MutexString& other);          // MutexString 간
    void swap(std::string& other_str);      // 내부 문자열과 std::string 간

    // ===== 문자열 연산 =====
    std::string substr(std::size_t pos = 0, std::size_t count = std::string::npos) const;
    std::size_t copy(char* dest, std::size_t count, std::size_t pos = 0) const;

    int compare(const std::string& s) const;
    int compare(std::size_t pos, std::size_t count, const std::string& s) const;

    std::size_t find(const std::string& s, std::size_t pos = 0) const;
    std::size_t find(const char* s, std::size_t pos = 0) const;
    std::size_t find(char ch, std::size_t pos = 0) const;

    std::size_t rfind(const std::string& s, std::size_t pos = std::string::npos) const;
    std::size_t rfind(const char* s, std::size_t pos = std::string::npos) const;
    std::size_t rfind(char ch, std::size_t pos = std::string::npos) const;

    std::size_t find_first_of(const std::string& s, std::size_t pos = 0) const;
    std::size_t find_first_of(const char* s, std::size_t pos = 0) const;
    std::size_t find_first_of(char ch, std::size_t pos = 0) const;

    std::size_t find_last_of(const std::string& s, std::size_t pos = std::string::npos) const;
    std::size_t find_last_of(const char* s, std::size_t pos = std::string::npos) const;
    std::size_t find_last_of(char ch, std::size_t pos = std::string::npos) const;

    std::size_t find_first_not_of(const std::string& s, std::size_t pos = 0) const;
    std::size_t find_first_not_of(const char* s, std::size_t pos = 0) const;
    std::size_t find_first_not_of(char ch, std::size_t pos = 0) const;

    std::size_t find_last_not_of(const std::string& s, std::size_t pos = std::string::npos) const;
    std::size_t find_last_not_of(const char* s, std::size_t pos = std::string::npos) const;
    std::size_t find_last_not_of(char ch, std::size_t pos = std::string::npos) const;

    // ===== 안전 편의 =====
    std::string str() const;

    // 람다로 잠금 범위 실행: with_lock()/with() (짧은 별칭)
    // ⚠️ 디버그 모드에서: with() 범위 안에서 같은 객체의 다른 멤버를 호출하면 assert
    template <typename Fn>
    auto with_lock(Fn&& f) -> decltype(std::forward<Fn>(f)(std::declval<std::string&>())) {
#ifndef NDEBUG
        assert_not_reentrant_();               // 같은 스레드의 재진입 방지
        ReentrancyMark _rmk{this};             // with() 수명 동안 "이 객체 while lock is held" 표시
#endif
        std::scoped_lock lock(m_);
        return std::forward<Fn>(f)(s_);
    }
    template <typename Fn>
    auto with_lock(Fn&& f) const -> decltype(std::forward<Fn>(f)(std::declval<const std::string&>())) {
#ifndef NDEBUG
        assert_not_reentrant_();
        ReentrancyMark _rmk{this};
#endif
        std::scoped_lock lock(m_);
        return std::forward<Fn>(f)(s_);
    }
    template <typename Fn>
    auto with(Fn&& f) -> decltype(std::forward<Fn>(f)(std::declval<std::string&>())) {
        return with_lock(std::forward<Fn>(f));
    }
    template <typename Fn>
    auto with(Fn&& f) const -> decltype(std::forward<Fn>(f)(std::declval<const std::string&>())) {
        return with_lock(std::forward<Fn>(f));
    }

    // 풀 API 접근(반복자/포인터 포함)
    [[nodiscard]] Locked synchronize();
    [[nodiscard]] Locked synchronize() const;

    // 짧은 별칭
    [[nodiscard]] Locked guard() { return synchronize(); }
    [[nodiscard]] Locked guard() const { return synchronize(); }

protected:
    // ⚠ protected: RAII c_str() 헬퍼도 외부에 노출하지 않음(오남용 방지)
    CStrGuard c_str() const;

    // 디버그 재진입 검사용 헬퍼/마크
#ifndef NDEBUG
    static thread_local const MutexString* tls_owner_;
    void assert_not_reentrant_() const {
        // 동일 스레드에서 이미 이 객체의 inside lock context이라면 no reentrancy
        assert(tls_owner_ != this && "reentrancy detection: with()/guard() 범위 안에서 다시 ms.* 호출 금지. "
                                     "with() 내부에서는 넘겨준 std::string(s)만 조작하세요.");
    }
    struct ReentrancyMark {
        const MutexString* prev;
        ReentrancyMark(const MutexString* self) : prev(tls_owner_) {
            // 이미 다른 객체가 표시돼도, 동일 객체 재진입만 금지
            assert(tls_owner_ != self && "동일 객체 no reentrancy");
            tls_owner_ = self;
        }
        ~ReentrancyMark() { tls_owner_ = prev; }
    };
#endif

    // 파생 클래스에서 직접 접근 가능
    std::string        s_;
    mutable std::mutex m_;
};

// non-member swap (ADL 대상)
inline void swap(MutexString& a, MutexString& b) { a.swap(b); }

} // namespace j2

// alias: 전역에서 use shortly하고 싶을 때 (예: jstr ms = "start";)
using jstr = j2::MutexString;
