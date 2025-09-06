# Safe Usage Guide for `jstr` <sub>(=`j2::MutexString`)</sub> in Multi-threaded Environments

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

## 6. Continuous Integration
Add a GitHub Actions workflow or your preferred CI to lint, test, and build.


<br />

---

## 7. License
This project is released under the MIT license. See `LICENSE` if present.

