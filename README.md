# Safe Usage Guide for `jstr` <sub>(=`j2::MutexString`)</sub> in Multi-threaded Environments

---

[Korean](README.ko.md)



---

## 1. Overview

- Even for read-only calls, using **pointers/iterators/references to the internal buffer** is dangerous.
- In such cases, you must always use them **within a lock guard scope**, e.g., `auto g = ms.guard();`.
- On the other hand, most **single member function calls** lock internally and are safe without `guard()`.

<br />

---

## 2. Cases Where `guard()` is **Required** (or Strongly Recommended)

- **Never** expose `pointers`, `iterators`, or `references` obtained from a guard **outside** the guard’s scope.

- Using pointers/iterators/references of the guarded `std::string`:
  - Pointer
    ```cpp
      g->c_str()
      g->data()
    ```
  - Iterators
    ```cpp
      g->begin()
      g->end()
      g->rbegin()
      g->rend()
    ```
    All iterators fall in this category.
  - References: accessors that return `char&` from a non-const guard
    ```cpp
      g->operator[](i)
      g->at(i)
      g->front()
      g->back()
    ```

- When multiple steps must be performed **atomically within the same lock scope**:
   - Example:
    ```cpp
      {
       auto g = ms.guard();
       if (g->find("x") != npos)
         g->replace(...);
      }
    ```

> Note: These pointer/iterator-based APIs are **not exposed directly** by `MutexString` and are only accessible through a guard by design.

<br />

---

## 3. Members That Are Safe **Without** `guard()` (Each Call Locks Internally)

- If you need to group multiple calls **atomically**, use `guard()`/`with()` as in Section 2.

### 3.1 Read-only Members

- Comparisons
   ```cpp
     operator==(const std::string&) const
     operator==(const char*) const  // (and `!=`)
   ```
- Size/Status
   ```cpp
     size()
     length()
     empty()
     capacity()
     max_size()
   ```
- Element access (value-returning): `at(size_t) const`, `operator[](size_t) const`, `front() const`, `back() const`
- Substring/copy/compare: `substr(pos,count)`, `copy(char* dest,count,pos)`, `compare(...)` overloads
- Search: `find(...)`, `rfind(...)`, `find_first_of(...)`, `find_last_of(...)`,
   - `find_first_not_of(...)`, `find_last_not_of(...)`
- Snapshot: `str()` *(returns a full copy, independent of later modifications)*

### 3.2 Write Members

- Assignment: `operator=(const std::string&)`, `operator=(const char*)`
- Capacity: `reserve(size_t)`, `shrink_to_fit()`
- Character modification: `set(pos,char)`, `front(char)`, `back(char)`
- Content modification:
   - `clear()`, `push_back(char)`, `pop_back()`
   - `assign(...)` (3 variants), `append(...)` (3 variants), `operator+=` (3 variants)
   - `insert(...)` (3 variants), `erase(pos,count)`
   - `replace(...)` (3 variants), `resize(n)`, `resize(n,char)`
- Swap: `swap(MutexString&)` *(both sides are locked internally)*
- Lock helpers: `guard()/synchronize()`, `with()/with_lock()` *(these functions themselves provide locking and are safe)*

<br />

---

## 4. Edge Cases and Warnings

- **`copy(char* dest, ...)`**: The call itself is safe, but concurrent safety of the `dest` buffer is the caller’s responsibility.
- **`swap(std::string& other_str)`**: `ms` is locked internally, but `other_str` is an external object. If another thread modifies it concurrently, it is unsafe. Use only with non-shared external strings.
- **TOCTOU (Time-Of-Check to Time-Of-Use)**:  
  For example, after `if (ms == "hello") { … }`, the value may change immediately.  
  If you need subsequent operations based on the comparison result, handle it within the **same lock scope** using `guard()`/`with()`, or work with a snapshot (`auto snap = ms.str()`).

<br />

---

## 5. One-line Summary

- For **any** use of pointers/iterators/references → restrict them **within a `guard()` scope**.

* Otherwise, single member calls are internally locked and therefore safe.  
* For multi-step atomic operations, use `guard()`/`with()`.

<br />

---

## 6. Example

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

## 7. License
This project is released under the MIT license. See `LICENSE` if present.

