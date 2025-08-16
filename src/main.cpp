#include "MutexString.hpp"
#include <iostream>
#include <thread>
#include <random>
#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

// C API 흉내(예시)
static void c_api_sink(const char* p) {
    (void)p; // 데모에서는 출력 생략
}

// 데모: 두 스레드가 랜덤하게 읽기/쓰기 수행
// - 외부에서는 c_str()/guard_cstr()가 protected 이므로 직접 호출 불가
// - 안전한 사용법: (1) 스냅샷 복사 ms.str()
//                (2) 락 범위 가드로 std::string API 사용: auto g=ms.guard(); g->c_str();
int main() {
    j2::MutexString ms("start");
    std::atomic<bool> stop{false};

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

                // 2-a) 스냅샷 복사로 C API 호출 (항상 안전)
                std::string snap = ms.str();
                c_api_sink(snap.c_str());

                // 2-b) 또는 가드를 잡고 std::string::c_str() 사용 (가드 수명 동안만 안전)
                {
                    auto g = ms.guard();
                    c_api_sink(g->c_str()); // 주의: 가드가 파괴되면 포인터는 더 이상 안전하지 않음
                }

                // ⚠ 아래는 디버그에서 assert로 잡힙니다(교착 예방):
                // ms.append("X"); // with()/guard() 범위 안에서 다시 ms.* 호출 금지
            }
            std::this_thread::sleep_for(1ms);
        }
        std::cout << "[reader] 'hello' 감지: " << hello_hits << "\n";
    };

    std::thread t1(writer), t2(reader);
    t1.join(); t2.join();

    std::cout << "최종(ms): " << ms.str() << "\n";

    std::cout << "\n[주의]\n"
              << " - ms.c_str() / guard().guard_cstr() 는 protected 입니다.\n"
              << " - guard 수명 동안 동일 객체의 다른 멤버 호출은 디버그에서 assert로 차단됩니다.\n"
              << " - 포인터/반복자/참조를 가드 범위 밖으로 넘기지 마세요.\n";
    return 0;
}

