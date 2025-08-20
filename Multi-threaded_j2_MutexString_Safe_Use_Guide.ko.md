# 멀티스레드에서 `j2::MutexString` 안전 사용 가이드

## 1. 개요

- 읽기 전용 호출이라도 **내부 버퍼의 포인터/반복자/참조** 를 쓰면 위험합니다.
- 이때는 반드시 `auto g = ms.guard();` 같은 **락 가드 범위 내**에서만 사용해야 합니다.
- 반면, 대부분의 **단일 멤버 함수 호출**은 내부에서 알아서 잠그므로 별도의 `guard()` 없이도 안전합니다.

<br />

---

## 2. `guard()`가 **필요한** 경우 <sub> (또는 강력 권장) </sub>

- 락 가드 범위 밖으로 `포인터(pointer)`/`반복자(iterator)`/`참조(reference)`를 **절대** 내보내지 마십시오.

- 가드로 얻은 `std::string`의 포인터/반복자/참조 사용
  - `g->c_str()`, `g->data()`
  - `g->begin()/end()/rbegin()/rend()` 등 모든 반복자
  - 비-const 가드에서 `char&`를 돌려주는 접근(`g->operator[](i)`, `g->at(i)`, `g->front()`, `g->back()`)
- 여러 단계를 **한 락 범위에서 원자적으로** 처리할 때
   - (예: `auto g = ms.guard(); if (g->find("x") != npos) g->replace(...);`)

> 주: 이러한 포인터·반복자 계열은 `MutexString`의 공개 API로 직접 노출되지 않으며, **가드를 통해서만** 접근하도록 설계되어 있습니다.

<br />

---

## 3. `guard()` 없이도 **안전한** 멤버 <sub> (각 호출이 내부에서 잠금) </sub>

- 여러 호출을 **원자적으로 묶어야** 한다면 2절처럼 `guard()`/`with()`를 사용하세요.

<br />

---

### 3.1 읽기 계열

- 비교: `operator==(const std::string&) const`, `operator==(const char*) const` *(및 `!=`)*
- 크기/상태: `size()`, `length()`, `empty()`, `capacity()`, `max_size()`
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
