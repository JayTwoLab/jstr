# 멀티스레드에서  `jstr` <sub> (=`j2::MutexString`) </sub> 안전 사용 가이드

- [English](README.md)

---

### 0. `jstr` 요약

`jstr` 는 **`j2::MutexString`** 의 별칭으로, `std::string` 을 감싼 **스레드 안전 문자열 클래스**입니다.
멀티스레드 환경에서 문자열을 **안전하게 접근**할 수 있도록 설계되었습니다.

* **뮤텍스 보호**: 내부적으로 `std::mutex` 를 사용하여 모든 연산을 보호합니다.
* **API 호환성**: `std::string` 과 유사한 함수들(`append`, `insert`, `replace`, `find` 등)을 제공합니다.
* **안전한 접근 방식**:

  * **스냅샷** (`str()`): 전체 복사본 반환 → 외부 전달에 안전.
  * **가드 기반 접근** (`guard()`): 락 범위 내에서 포인터, 반복자, 참조를 안전하게 사용.
  * **원자적 연산** (`with()`): 여러 단계를 하나의 락 범위에서 처리.
* **재진입 방지**: 디버그 모드에서는 잘못된 중첩 호출을 assert 로 차단합니다.

👉 요약하면: `jstr` 은 `std::string` 처럼 사용할 수 있지만, **멀티스레드 환경에서도 안전**하게 동작하도록 만들어진 문자열 클래스입니다.

---

## 1. 개요

- 읽기 전용 호출이라도 **내부 버퍼의 포인터/반복자/참조** 를 쓰면 위험합니다.
- 이때는 반드시 `auto g = ms.guard();` 같은 **락 가드 범위 내**에서만 사용해야 합니다.
- 반면, 대부분의 **단일 멤버 함수 호출**은 내부에서 알아서 잠그므로 별도의 `guard()` 없이도 안전합니다.

<br />

---

## 2. `guard()`가 **필요한** 경우 <sub> (또는 강력 권장) </sub>

- 락 가드 범위 밖으로 `포인터(pointer)`/`반복자(iterator)`/`참조(reference)`를 **절대** 내보내지 마십시오.

- 가드로 얻은 `std::string`의 포인터/반복자/참조 사용
  - 포인터
    ```cpp
      g->c_str()
      g->data()
    ```
  - 반복자
    ```cpp
      g->begin()
      g->end()
      g->rbegin()
      g->rend() 
    ```
    등 모든 반복자
  - 참조 : `non-const` 가드에서 `char&`를 돌려주는 접근
    ```cpp
      g->operator[](i)
      g->at(i)
      g->front()
      g->back()
    ```
  
- 여러 단계를 **한 락 범위에서 원자적으로** 처리할 때
   - 예:
    ```cpp
      {
       auto g = ms.guard();
       if (g->find("x") != npos)
         g->replace(...);
      }
    ```

> 주: 이러한 포인터·반복자 계열은 `MutexString`의 공개 API로 직접 노출되지 않으며, **가드를 통해서만** 접근하도록 설계되어 있습니다.

<br />

---

## 3. `guard()` 없이도 **안전한** 멤버 <sub> (각 호출이 내부에서 잠금) </sub>

- 여러 호출을 **원자적으로 묶어야** 한다면 2절처럼 `guard()`/`with()`를 사용하세요.

<br />

---

### 3.1 읽기 계열

- 비교
   ```cpp
     operator==(const std::string&) const
     operator==(const char*) const // (및 `!=`)
   ```
- 크기/상태
   ```cpp
     size()
     length()
     empty()
     capacity()
     max_size()
   ```
- 요소 값 조회(값 반환): `at(size_t) const`, `operator[](size_t) const`, `front() const`, `back() const`
- 부분/복사/비교: `substr(pos,count)`, `copy(char* dest,count,pos)`, `compare(...)` 오버로드
- 검색: `find(...)`, `rfind(...)`, `find_first_of(...)`, `find_last_of(...)`,
   - `find_first_not_of(...)`, `find_last_not_of(...)`
- 스냅샷: `str()` *(전체 복사본을 반환하므로 이후 변경과 독립)*


<br />

---

### 3.2 쓰기 계열

- 대입: `operator=(const std::string&)`, `operator=(const char*)`
- 용량: `reserve(size_t)`, `shrink_to_fit()`
- 개별 문자 수정: `set(pos,char)`, `front(char)`, `back(char)`
- 내용 수정:
   - `clear()`, `push_back(char)`, `pop_back()`,
   - `assign(...)` 3종, `append(...)` 3종, `operator+=` 3종,
   - `insert(...)` 3종, `erase(pos,count)`,
   - `replace(...)` 3종, `resize(n)`, `resize(n,char)`
- 스왑: `swap(MutexString&)` *(양쪽 모두 내부에서 잠금)*
- 락 헬퍼: `guard()/synchronize()`, `with()/with_lock()` *(락을 제공하는 함수 자체는 안전)*


<br />

---

## 4. 경계 사례 및 주의

- **`copy(char* dest, ...)`**: 호출 중엔 안전하나, `dest` 버퍼의 동시 접근 안전성은 **호출자 책임**입니다.
- **`swap(std::string& other_str)`**: `ms`는 잠그지만 `other_str`은 외부 객체이므로, 다른 스레드가 동시에 쓰면 위험합니다. 공유되지 않는 외부 문자열에서만 사용하십시오.
- **TOCTOU(체크-후-행동)**:
  `if (ms == "hello") { … }` 직후 값이 바뀔 수 있습니다. 비교 결과를 **가정**한 연속 동작이 필요하면 `guard()`/`with()`로 **같은 락 범위**에서 처리하거나, 스냅샷(`auto snap = ms.str()`)을 기준으로 후속 로직을 수행하세요.


<br />

---

## 5. 한 줄 요약

- 포인터/반복자/참조를 쓰는 **모든** 경우 → **`guard()` 범위 내에서만** 사용.

  
* 그 외 **단일 멤버 호출**은 내부 잠금으로 **그냥 안전**.
* 여러 단계를 **원자적으로** 처리할 땐 `guard()`/`with()`로 묶어 주세요.

<br />

---

## 6. 예제

```cpp
#include "MutexString.hpp"
#include <iostream>
#include <thread>
#include <random>
// #include <chrono>

using namespace std::chrono_literals;

// C API mimic(example)
static void c_api_sink(const char* p) {
    (void)p; // demo: output omitted
}

void testAtomicity();
void testJstr();

int main() {

    // j2::MutexString = jstr class basic member test
    testJstr();

    // demo: two threads randomly read/write strings
    testAtomicity();

    return 0;
}

//---------------------------------------------------------------------------
// demo: two threads randomly read/write strings
// - externally, since c_str()/guard_cstr() are protected, they cannot be called directly
// - safe usage: (1) snapshot copy ms.str()
//               (2) use std::string API within lock guard scope: auto g=ms.guard(); g->c_str();
void testAtomicity() {

    j2::MutexString ms("start"); // same as jstr ms("start");

    auto writer = [&]{
        std::mt19937 rng(std::random_device{}());
        for (int i=0; i<300; ++i) {
            if (rng() & 1) ms = "hello";
            else           ms = "world";
            std::this_thread::sleep_for(1ms);
        }
    };

    auto reader = [&]{
        std::mt19937 rng(std::random_device{}());
        int hello_hits = 0;
        for (int i=0; i<300; ++i) {

            // 1) Comparison itself is safe (inside lock)
            if (ms == "hello") {
                ++hello_hits;

                // 2-a) Call C API with snapshot copy (always safe)
                std::string snap = ms.str();
                c_api_sink(snap.c_str());

                // 2-b) Or acquire guard and use std::string::c_str() (safe only during guard lifetime)
                {
                    auto g = ms.guard(); // create guard
                    c_api_sink(g->c_str()); // note: pointer becomes unsafe after guard is destroyed
                }

                // ⚠ The following is caught by assert in debug mode (deadlock prevention):
                // ms.append("X"); // do not call ms.* again inside with()/guard() scope
            }

            std::this_thread::sleep_for(1ms);
        }
        std::cout << "[reader] 'hello' detected: " << hello_hits << "\n";
    };

    std::thread t1(writer), t2(reader);
    t1.join(); t2.join();

    std::cout << "final(ms): " << ms.str() << "\n";

    std::cout << "\n[note]\n"
              << " - ms.c_str() / guard().guard_cstr() are protected.\n"
              << " - Calling other members of the same object during guard lifetime is blocked by assert in debug mode.\n"
              << " - Do not pass pointer/iterator/reference outside of guard scope.\n";
}

//---------------------------------------------------------------------------
// Demonstration of various j2::MutexString features similar to std::string
// testJstr() does not include multi-thread related tests.
void testJstr() {

    std::cout << "\n===== testJstr: j2::MutexString basic functionality test =====\n";

    // construction/copy initialization (implicit conversion allowed)
    // jstr is same as j2::MutexString
    jstr ms = "start"; // 5 characters
    std::cout << "initial: \"" << ms.str() << "\" size=" << ms.size()
              << " empty=" << std::boolalpha << ms.empty() << "\n";
    // initial: "start" size=5 empty=false

    // reserve/capacity/shrink_to_fit
    auto cap0 = ms.capacity();
    ms.reserve(64);
    std::cout << "reserve(64): capacity " << cap0 << " -> " << ms.capacity() << "\n";
    // reserve(64): capacity 15 -> 64

    // append / operator+=
    ms.append(" plus");
    ms += ' ';
    ms += std::string("more");
    std::cout << "after append/+=: \"" << ms.str() << "\"\n";
    // after append/+=: "start plus more"

    // insert (string/repeated char/char*)
    ms.insert(0, "["); // insert at beginning
    ms.push_back(']'); // append at end
    ms.insert(1, 3, '*'); // insert 3 '*' at index 1
    ms.insert(ms.size(), " tail"); // append C-string " tail" at end
    std::cout << "after insert/push_back: \"" << ms.str() << "\"\n";
    // after insert/push_back: "[***start plus more] tail"

    // set/at/operator[]/front/back (getter/setter)
    char old0 = ms[0]; // read-only indexer
    ms.set(0, '{'); // modify character at index 0
    char f_before = ms.front();
    ms.front('(');
    char b_before = ms.back();
    ms.back(')');
    std::cout << "indexing/front/back: old[0]='" << old0
              << "' front:" << f_before << "->" << ms.front()
              << " back:" << b_before  << "->" << ms.back()
              << " result=\"" << ms.str() << "\"\n";
    // indexing/front/back: old[0]='[' front:{->( back:l->) result="(***start plus more] tai)"

    // find / rfind / find_first_of / find_last_of / find_first_not_of / find_last_not_of
    std::size_t pos_plus = ms.find("plus");
    std::cout << "find(\"plus\") = " << pos_plus << "\n"; // find("plus") = 10
    std::cout << "rfind('l') = " << ms.rfind('l') << "\n"; // rfind('l') = 11
    std::cout << "find_first_of(\"aeiou\") = " << ms.find_first_of("aeiou") << "\n"; // find_first_of("aeiou") = 6
    std::cout << "find_last_of('o') = " << ms.find_last_of('o') << "\n"; // find_last_of('o') = 16
    std::cout << "find_first_not_of(\"()*\") = " << ms.find_first_not_of("()*") << "\n"; // find_first_not_of("()*") = 4
    std::cout << "find_last_not_of(')') = " << ms.find_last_not_of(')') << "\n"; // find_last_not_of(')') = 23

    // substr / compare
    if (pos_plus != std::string::npos) {
        auto sub = ms.substr(pos_plus, 4);
        std::cout << "substr(pos_plus,4) = \"" << sub << "\"\n";
        // substr(pos_plus,4) = "plus"
    }
    std::cout << "compare(\"(***{start plus more] tail)\") = "
              << ms.compare("(***{start plus more] tail)") << "\n";
    // compare("(***{start plus more] tail)") = -1
    std::cout << "compare(1,5,std::string(\"***{\")) = "
              << ms.compare(1, 5, std::string("***{")) << "\n";
    // compare("(***{start plus more] tail)") = -1

    // replace (string/char*/n, ch)
    if (ms.size() >= 5) {
        ms.replace(1, 5, "BEGIN");     // replace range 1~5 with "BEGIN"
    }
    ms.replace(0, 0, 2, '#');          // insert two '#' at beginning
    std::cout << "after replace: \"" << ms.str() << "\"\n";
    // after replace: "##(BEGINart plus more] tai)"

    // erase (delete one space if exists)
    auto sp = ms.find(' ');
    if (sp != std::string::npos) ms.erase(sp, 1);
    std::cout << "after erase(one space): \"" << ms.str() << "\"\n";
    // after erase(one space): "##(BEGINartplus more] tai)"

    // copy (must manually add null terminator)
    char buf[128];
    std::size_t n = ms.copy(buf, ms.size(), 0);
    buf[n] = '\0';
    std::cout << "copy -> buf: \"" << buf << "\" (n=" << n << ")\n";
    // copy -> buf: "##(BEGINartplus more] tai)" (n=26)

    // resize
    ms.resize(ms.size() + 3, '!');
    std::cout << "after resize(+3,'!'): \"" << ms.str() << "\"\n";
    // after resize(+3,'!'): "##(BEGINartplus more] tai)!!!"
    ms.shrink_to_fit();
    std::cout << "after shrink_to_fit() capacity=" << ms.capacity() << "\n";
    // after shrink_to_fit() capacity=29

    // swap(std::string&) — safe in single-thread example
    std::string ext = "EXTERNAL";
    ms.swap(ext);
    std::cout << "after swap(std::string): ms=\"" << ms.str() << "\", ext=\"" << ext << "\"\n";
    // after swap(std::string): ms="EXTERNAL", ext="##(BEGINartplus more] tai)!!!"
    ms.swap(ext); // restore

    // swap(MutexString&)
    jstr ms2 = "other";
    ms2.swap(ms);
    std::cout << "after swap(jstr): ms=\"" << ms.str() << "\", ms2=\"" << ms2.str() << "\"\n";
    // after swap(jstr): ms="other", ms2="##(BEGINartplus more] tai)!!!"
    ms2.swap(ms); // restore

    // clear / empty
    jstr tmp = "temp";
    tmp.clear();
    std::cout << "after clear(): tmp.empty()=" << std::boolalpha << tmp.empty() << "\n";
    // after clear(): tmp.empty()=true

    // use guard(): safe usage of std::string pointer/full API within guard scope
    {
        auto g = ms.guard();
        std::cout << "guard()->length()=" << g->length()
                  << ", guard()->c_str()=\"" << g->c_str() << "\"\n";
        // guard()->length()=29, guard()->c_str()="##(BEGINartplus more] tai)!!!"

        // ⚠ Do not store/return pointer outside guard scope.
    }

    // use with(): perform a batch modification within lock scope
    ms.with([](std::string& s){
        s.append(" [WITH]");
        // ⚠ Calling ms.* again here triggers assert (reentrancy) in debug mode
    });
    std::cout << "after with(): \"" << ms.str() << "\"\n";
    // after with(): "##(BEGINartplus more] tai)!!! [WITH]"

    // insert(repeated char) / replace(n, ch) / erase(all)
    ms.insert(0, 3, '*');          // insert 3 '*' at beginning
    ms.replace(0, 3, 2, '#');      // replace first 3 chars with 2 '#'
    ms.erase(0);                   // erase from beginning to end
    std::cout << "after insert/replace/erase(0): \"" << ms.str() << "\"\n";
    // after insert/replace/erase(0): ""

    // find_first_not_of(char)
    std::cout << "find_first_not_of(' ') = " << ms.find_first_not_of(' ') << "\n";
    // find_first_not_of(' ') = 18446744073709551615

    // comparison operators
    std::cout << "ms == \"" << ms.str() << "\" ? " << (ms == ms.str() ? "true" : "false") << "\n";
    // ms == "" ? true
    std::cout << "ms != \"zzz\" ? " << (ms != "zzz" ? "true" : "false") << "\n";
    // ms != "zzz" ? true

    std::cout << "===== end of testJstr =====\n";
}
```

<br />

---

## 7. 라이선스
- 본 프로젝트는 MIT 라이선스로 배포됩니다. `LICENSE` 파일이 있다면 해당 내용을 우선합니다.

