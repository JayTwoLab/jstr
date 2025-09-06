#include "MutexString.hpp"
#include <iostream>
#include <thread>
#include <random>
// #include <chrono>

using namespace std::chrono_literals;

// C API mimic(example)
static void c_api_sink(const char* p) {
    (void)p; // demo에서는 출력 생략
}

void testAtomicity();
void testJstr();

int main() {

    // j2::MutexString = jstr 클래스의 기본 멤버 테스트
    testJstr();

    // demo: two threads randomly read/write strings
    testAtomicity();

    return 0;
}

//---------------------------------------------------------------------------
// demo: two threads randomly read/write strings
// - externally, c_str()/guard_cstr()가 protected 이므로 cannot be called directly
// - safe usage: (1) snapshot copy ms.str()
//                  (2) lock guard scope로 std::string API 사용: auto g=ms.guard(); g->c_str();
void testAtomicity() {

    j2::MutexString ms("start"); // jstr ms("start"); 와 동일함.

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

            // 1) 비교 자체는 안전(락 내부)
            if (ms == "hello") {
                ++hello_hits;

                // 2-a) snapshot copy로 C API 호출 (항상 안전)
                std::string snap = ms.str();
                c_api_sink(snap.c_str());

                // 2-b) 또는 가드를 잡고 std::string::c_str() 사용 (during guard lifetime만 안전)
                {
                    auto g = ms.guard(); // 가드 생성
                    c_api_sink(g->c_str()); // note: 가드가 파괴되면 포인터는 더 이상 안전하지 않음
                }

                // ⚠ 아래는 caught by assert in debug mode(deadlock prevention):
                // ms.append("X"); // with()/guard() 범위 안에서 다시 ms.* 호출 금지
            }

            std::this_thread::sleep_for(1ms);
        }
        std::cout << "[reader] 'hello' 감지: " << hello_hits << "\n";
    };

    std::thread t1(writer), t2(reader);
    t1.join(); t2.join();

    std::cout << "최종(ms): " << ms.str() << "\n";

    std::cout << "\n[note]\n"
              << " - ms.c_str() / guard().guard_cstr() 는 protected 입니다.\n"
              << " - guard 수명 동안 동일 객체의 다른 멤버 호출은 디버그에서 assert로 차단됩니다.\n"
              << " - 포인터/반복자/참조를 가드 범위 밖으로 넘기지 마세요.\n";
}

//---------------------------------------------------------------------------
// std::string 의 기본 멤버와 유사한 기능들을 j2::MutexString 로 폭넓게 시연
// testJstr() 내부에서는 no multi-thread related tests.
void testJstr() {

    std::cout << "\n===== testJstr: j2::MutexString basic functionality test =====\n";

    // construction/copy initialization(암시적 변환 허용)
    // jstr 는 j2::MutexString 와 동일함.
    jstr ms = "start"; // 5글자
    std::cout << "초기값: \"" << ms.str() << "\" size=" << ms.size()
              << " empty=" << std::boolalpha << ms.empty() << "\n";
    // 초기값: "start" size=5 empty=false

    // reserve/capacity/shrink_to_fit
    auto cap0 = ms.capacity();
    ms.reserve(64);
    std::cout << "reserve(64): capacity " << cap0 << " -> " << ms.capacity() << "\n";
    // reserve(64): capacity 15 -> 64

    // append / operator+=
    ms.append(" plus");
    ms += ' ';
    ms += std::string("more");
    std::cout << "append/+= 후: \"" << ms.str() << "\"\n";
    // append/+= 후: "start plus more"

    // insert (문자열/문자 반복/char*)
    ms.insert(0, "["); // insert at the beginning [ 삽입
    ms.push_back(']'); // append at the end ] 추가
    ms.insert(1, 3, '*'); // 인덱스 1 위치에 '*' 3개
    ms.insert(ms.size(), " tail"); // append at the end C-문자열인 " tail" 삽입
    std::cout << "insert/push_back 후: \"" << ms.str() << "\"\n";
    // insert/push_back 후: "[***start plus more] tail"

    // set/at/operator[]/front/back (게터/세터)
    char old0 = ms[0]; // read-only indexer
    ms.set(0, '{'); // 0번 문자 수정
    char f_before = ms.front();
    ms.front('(');
    char b_before = ms.back();
    ms.back(')');
    std::cout << "인덱싱/front/back: old[0]='" << old0
              << "' front:" << f_before << "->" << ms.front()
              << " back:" << b_before  << "->" << ms.back()
              << " result=\"" << ms.str() << "\"\n";
    // 인덱싱/front/back: old[0]='[' front:{->( back:l->) result="(***start plus more] tai)"

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

    // replace (문자열/char*/n, ch)
    if (ms.size() >= 5) {
        ms.replace(1, 5, "BEGIN");     // 1~5 구간을 "BEGIN"으로
    }
    ms.replace(0, 0, 2, '#');          // insert at the beginning '#' 2개 삽입
    std::cout << "replace 후: \"" << ms.str() << "\"\n";
    // replace 후: "##(BEGINart plus more] tai)"

    // erase (delete one space if exists)
    auto sp = ms.find(' ');
    if (sp != std::string::npos) ms.erase(sp, 1);
    std::cout << "erase(space 1개) 후: \"" << ms.str() << "\"\n";
    // erase(space 1개) 후: "##(BEGINartplus more] tai)"

    // copy (널 종료는 직접 추가 필요)
    char buf[128];
    std::size_t n = ms.copy(buf, ms.size(), 0);
    buf[n] = '\0';
    std::cout << "copy -> buf: \"" << buf << "\" (n=" << n << ")\n";
    // copy -> buf: "##(BEGINartplus more] tai)" (n=26)

    // resize
    ms.resize(ms.size() + 3, '!');
    std::cout << "resize(+3,'!') 후: \"" << ms.str() << "\"\n";
    // resize(+3,'!') 후: "##(BEGINartplus more] tai)!!!"
    ms.shrink_to_fit();
    std::cout << "shrink_to_fit() 후 capacity=" << ms.capacity() << "\n";
    // shrink_to_fit() 후 capacity=29

    // swap(std::string&) — 단일 스레드 예제에서는 안전
    std::string ext = "EXTERNAL";
    ms.swap(ext);
    std::cout << "swap(std::string) 후: ms=\"" << ms.str() << "\", ext=\"" << ext << "\"\n";
    // swap(std::string) 후: ms="EXTERNAL", ext="##(BEGINartplus more] tai)!!!"
    ms.swap(ext); // 원복

    // swap(MutexString&)
    jstr ms2 = "other";
    ms2.swap(ms);
    std::cout << "swap(jstr) 후: ms=\"" << ms.str() << "\", ms2=\"" << ms2.str() << "\"\n";
    // swap(jstr) 후: ms="other", ms2="##(BEGINartplus more] tai)!!!"
    ms2.swap(ms); // 원복

    // clear / empty
    jstr tmp = "temp";
    tmp.clear();
    std::cout << "clear() 후: tmp.empty()=" << std::boolalpha << tmp.empty() << "\n";
    // clear() 후: tmp.empty()=true

    // guard() 사용: std::string 의 포인터/전체 API를 가드 범위에서만 사용 (안전)
    {
        auto g = ms.guard();
        std::cout << "guard()->length()=" << g->length()
                  << ", guard()->c_str()=\"" << g->c_str() << "\"\n";
        // guard()->length()=29, guard()->c_str()="##(BEGINartplus more] tai)!!!"

        // ⚠ 포인터를 범위 밖으로 보관/반환하지 마세요.
    }

    // with() 사용: 한 덩어리의 변경을 락 범위에서 처리
    ms.with([](std::string& s){
        s.append(" [WITH]");
        // ⚠ 여기서 다시 ms.* 멤버를 호출하면 디버그에서 assert(재진입) 발생
    });
    std::cout << "with() 후: \"" << ms.str() << "\"\n";
    // with() 후: "##(BEGINartplus more] tai)!!! [WITH]"

    // insert(반복 문자) / replace(n, ch) / erase(0부터 끝까지)
    ms.insert(0, 3, '*');          // insert at the beginning '*' 3개
    ms.replace(0, 3, 2, '#');      // 앞 3개를 '#' 2개로 대체
    ms.erase(0);                   // 0부터 끝까지 삭제
    std::cout << "insert/replace/erase(0) 후: \"" << ms.str() << "\"\n";
    // insert/replace/erase(0) 후: ""

    // find_first_not_of(char)
    std::cout << "find_first_not_of(' ') = " << ms.find_first_not_of(' ') << "\n";
    // find_first_not_of(' ') = 18446744073709551615

    // 비교 연산자
    std::cout << "ms == \"" << ms.str() << "\" ? " << (ms == ms.str() ? "true" : "false") << "\n";
    // ms == "" ? true
    std::cout << "ms != \"zzz\" ? " << (ms != "zzz" ? "true" : "false") << "\n";
    // ms != "zzz" ? true

    std::cout << "===== testJstr 끝 =====\n";
}
