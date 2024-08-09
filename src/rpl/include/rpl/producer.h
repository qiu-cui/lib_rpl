#pragma once

#include <rx.hpp>

#if defined _DEBUG || defined COMPILER_MSVC
#    define RPL_PRODUCER_TYPE_ERASED_ALWAYS
#endif  // _DEBUG

namespace rpl {
struct empty_value {};

struct no_value {
    no_value() = delete;
};

class lifetime : public rxcpp::composite_subscription {
   public:
    lifetime() : rxcpp::composite_subscription() {}
    lifetime(const lifetime &o) : rxcpp::composite_subscription(o) {}
    lifetime(lifetime &&o) : rxcpp::composite_subscription(std::move(o)) {}
    lifetime &operator=(lifetime o) {
        rxcpp::composite_subscription::operator=(std::move(o));
        return *this;
    }
    ~lifetime() {
        auto state = rxcpp::composite_subscription::get_weak();
        if (!state.lock()) return;
        unsubscribe();
    }
};

using Error = std::exception_ptr;

template <typename Value = empty_value>
class producer : public rxcpp::observable<Value, rxcpp::dynamic_observable<Value>> {
   public:
    // 使用基类的构造函数
    using rxcpp::observable<Value, rxcpp::dynamic_observable<Value>>::observable;

    template <typename OnNext, typename OnError, typename OnDone>
    void start(OnNext &&next, OnError &&error, OnDone &&done, lifetime &alive_while) &&;

    template <typename OnNext, typename OnError, typename OnDone>
    [[nodiscard]] lifetime start(OnNext &&next, OnError &&error, OnDone &&done) &&;

    template <typename OnNext, typename OnError, typename OnDone>
    void start_copy(OnNext &&next, OnError &&error, OnDone &&done, lifetime &alive_while) const &;

    template <typename OnNext, typename OnError, typename OnDone>
    [[nodiscard]] lifetime start_copy(OnNext &&next, OnError &&error, OnDone &&done) const &;

    // 静态方法，用于创建 custom_observable
    template <typename Generator>
    static producer<Value> create(Generator &&generator) {
        auto observable = rxcpp::observable<>::create<Value>([=](rxcpp::subscriber<Value> s) { s.add(generator(s)); });
        return producer<Value>(observable);
    }

   private:
    // 私有构造函数，用于从基类 observable 创建 custom_observable
    explicit producer(const rxcpp::observable<Value, rxcpp::dynamic_observable<Value>> &obs)
        : rxcpp::observable<Value, rxcpp::dynamic_observable<Value>>(obs) {}
};

template <typename Value>
template <typename OnNext, typename OnError, typename OnDone>
inline void producer<Value>::start(OnNext &&next, OnError &&error, OnDone &&done, lifetime &alive_while) && {
    auto subscriber = rxcpp::make_subscriber<Value>(alive_while, std::forward<OnNext>(next),
                                                    std::forward<OnError>(error), std::forward<OnDone>(done));
    std::move(*this).subscribe(std::move(subscriber));
}

template <typename Value>
template <typename OnNext, typename OnError, typename OnDone>
[[nodiscard]] inline lifetime producer<Value>::start(OnNext &&next, OnError &&error, OnDone &&done) && {
    auto result = lifetime();
    auto subscriber = rxcpp::make_subscriber<Value>(result, std::forward<OnNext>(next), std::forward<OnError>(error),
                                                    std::forward<OnDone>(done));
    std::move(*this).subscribe(std::move(subscriber));
    return result;
}

template <typename Value>
template <typename OnNext, typename OnError, typename OnDone>
inline void producer<Value>::start_copy(OnNext &&next, OnError &&error, OnDone &&done, lifetime &alive_while) const & {
    auto copy = *this;
    auto subscriber = rxcpp::make_subscriber<Value>(alive_while, std::forward<OnNext>(next),
                                                    std::forward<OnError>(error), std::forward<OnDone>(done));
    std::move(copy).subscribe(std::move(subscriber));
}

template <typename Value>
template <typename OnNext, typename OnError, typename OnDone>
[[nodiscard]] inline lifetime producer<Value>::start_copy(OnNext &&next, OnError &&error, OnDone &&done) const & {
    auto result = lifetime();
    auto copy = *this;
    auto subscriber = rxcpp::make_subscriber<Value>(result, std::forward<OnNext>(next), std::forward<OnError>(error),
                                                    std::forward<OnDone>(done));
    std::move(copy).subscribe(std::move(subscriber));
    return result;
}

template <typename Value = empty_value, typename Generator>
inline auto make_producer(Generator &&generator) -> producer<Value> {
    return producer<Value>::create(std::move(generator));
}

}  // namespace rpl