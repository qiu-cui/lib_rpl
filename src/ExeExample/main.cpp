#include <rpl/event_stream.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

// 自定义的派生类，继承自 rxcpp::observable
template <typename T>
class custom_observable : public rxcpp::observable<T, rxcpp::dynamic_observable<T>> {
   public:
    // 使用基类的构造函数
    using rxcpp::observable<T, rxcpp::dynamic_observable<T>>::observable;

    // 添加一个新的成员函数
    void custom_function() const { std::cout << "This is a custom function in custom_observable." << std::endl; }
};

namespace detail {
template <typename Value = rpl::empty_value>
using producer = custom_observable<Value>;
}

int main() {
    rxcpp::observable<int> obs{};

    // obs 监听错误
    obs.subscribe([](int value) { std::cout << "obs-->" << value << std::endl; },
                  [](std::exception_ptr error) {
                      std::cout << "obs-->"
                                << "error" << std::endl;
                  });
    custom_observable<int> custom_obs{};
    rpl::event_stream<int> stream;
    rpl::event_stream<int> stream2;
    custom_obs = stream.events();
    detail::producer<int> producer = stream.events();

    rpl::lifetime lifetime;

    stream.fire(1);
    stream.fire(2);
    stream.fire(3);
    stream.fire(4);

    producer.subscribe([&](int value) { std::cout << "producer->" << (char)value << std::endl; });

    std::atomic<bool> running{ true };
    std::thread([&] {
        while (running) {
            stream.fire('1');
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }).detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    rpl::start_to_stream(stream2, lifetime)(std::move(producer));

    auto c = rpl::start_spawning(lifetime)(std::move(producer));
    std::cout << "c is create" << std::endl;
    running.store(false);

    stream2.events().subscribe([&](int value) { std::cout << "stream2-->" << value << std::endl; });
    c.subscribe([&](int value) { std::cout << "c-->" << (char)value << std::endl; });

    while (1) {
        int c = std::cin.get();
        if (c == 'q') break;
        if (c == 'c') { lifetime.unsubscribe(); }
        if (c == '\n') continue;
        stream.fire(std::move(c));
    }

    return EXIT_SUCCESS;
}
