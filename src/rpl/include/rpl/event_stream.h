// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <optional>

#include "rpl/producer.h"

namespace rpl {

// Currently not thread-safe :(

template <typename Value = empty_value>
class event_stream {
   public:
    event_stream() noexcept = default;
    event_stream(event_stream &&other);
    event_stream &operator=(event_stream &&other);

    void fire_forward(Value &&value) const {
        auto weak = make_weak();
        if (const auto strong = weak.lock()) { strong->get_subscriber().on_next(std::move(value)); }
    }
    void fire(Value &&value) const { return fire_forward(std::move(value)); }
    void fire_copy(const Value &value) const { return fire_forward(value); }

    void fire_error_forward(std::exception_ptr &&error) const {
        auto weak = make_weak();
        if (const auto strong = weak.lock()) { strong->get_subscriber().on_error(std::move(error)); }
    }
    void fire_error(std::exception_ptr &&error) const { return fire_error_forward(std::move(error)); }
    void fire_error_copy(const std::exception_ptr &error) const { return fire_error_forward(error); }

    void fire_done() const;

#if defined _MSC_VER && _MSC_VER >= 1914 && _MSC_VER < 1916
    producer<Value> events() const {
#else   // _MSC_VER >= 1914 && _MSC_VER < 1916
    auto events() const {
#endif  // _MSC_VER >= 1914 && _MSC_VER < 1916
        auto weak = make_weak();
        if (const auto strong = weak.lock()) {
            return strong->get_observable();
        } else {
            std::terminate();
        }
    }
    auto events_starting_with(Value &&value) const { return events().start_with(std::move(value)); }
    auto events_starting_with_copy(const Value &value) const { return events().start_with(value); }
    bool has_consumers() const { return (_data != nullptr) && _data->has_observers(); }

    ~event_stream();

   private:
    std::weak_ptr<rxcpp::subjects::subject<Value>> make_weak() const;

    mutable std::shared_ptr<rxcpp::subjects::subject<Value>> _data;
};

template <typename Value>
inline event_stream<Value>::event_stream(event_stream &&other) : _data(std::exchange(other._data, nullptr)) {}

template <typename Value>
inline event_stream<Value> &event_stream<Value>::operator=(event_stream &&other) {
    if (this != &other) {
        std::swap(_data, other._data);
        other.fire_done();
    }
    return *this;
}

template <typename Value>
void event_stream<Value>::fire_done() const {
    if (const auto data = std::exchange(_data, nullptr)) { data->get_subscription().unsubscribe(); }
}

template <typename Value>
inline auto event_stream<Value>::make_weak() const -> std::weak_ptr<rxcpp::subjects::subject<Value>> {
    if (!_data) { _data = std::make_shared<rxcpp::subjects::subject<Value>>(); }
    return _data;
}

template <typename Value>
inline event_stream<Value>::~event_stream() {
    fire_done();
}

template <typename Value>
auto start_to_stream(event_stream<Value> &stream, lifetime &alive_while) {
    return [&](auto &&observable) {
        observable.subscribe(
            alive_while, [&](Value value) { stream.fire(std::move(value)); },
            [&](std::exception_ptr error) { stream.fire_error(std::move(error)); }, [&]() { stream.fire_done(); });
    };
}
namespace details {

class start_spawning_helper {
   public:
    explicit start_spawning_helper(lifetime &alive_while) : _lifetime(alive_while) {}

    template <typename Value>
    auto operator()(rxcpp::observable<Value> &&initial) -> rxcpp::observable<Value> {
        auto stream = new event_stream<Value>();

        std::vector<Value> values;
        std::optional<std::exception_ptr> maybeError;

        auto collecting =
            stream->events().subscribe([&](Value value) { values.push_back(std::move(value)); },
                                       [&](std::exception_ptr error) { maybeError = std::move(error); }, [] {});
        std::move(initial) | start_to_stream(*stream, _lifetime);
        collecting.unsubscribe();

        if (maybeError.has_value()) {
            return rxcpp::observable<>::create<Value>(
                [error = std::move(*maybeError)](rxcpp::subscriber<Value> s) { s.on_error(std::move(error)); });
        }

        return rxcpp::observable<>::iterate(std::move(values)).concat(stream->events());
    }

   private:
    lifetime &_lifetime;
};

}  // namespace details
// namespace details {

//	class start_spawning_helper {
//	public:
//		start_spawning_helper(lifetime& alive_while)
//			: _lifetime(alive_while) {
//		}

//		template <typename Value, typename Error, typename Generator>
//		auto operator()(producer<Value, Error, Generator>&& initial) {
//			auto stream = _lifetime.make_state<event_stream<Value, Error>>();
//			auto values = std::vector<Value>();
//			if constexpr (std::is_same_v<Error, rpl::no_error>) {
//				auto collecting = stream->events().start(
//					[&](Value&& value) { values.push_back(std::move(value)); },
//					[](const Error& error) {},
//					[] {});
//				std::move(initial) | start_to_stream(*stream, _lifetime);
//				collecting.destroy();

//				return vector(std::move(values)) | then(stream->events());
//			}
//			else {
//				auto maybeError = std::optional<Error>();
//				auto collecting = stream->events().start(
//					[&](Value&& value) { values.push_back(std::move(value)); },
//					[&](Error&& error) { maybeError = std::move(error); },
//					[] {});
//				std::move(initial) | start_to_stream(*stream, _lifetime);
//				collecting.destroy();

//				if (maybeError.has_value()) {
//					return rpl::producer<Value, Error>([
//						error = std::move(*maybeError)
//					](const auto& consumer) mutable {
//							consumer.put_error(std::move(error));
//						});
//				}
//				return rpl::producer<Value, Error>(vector<Value, Error>(
//					std::move(values)
//				) | then(stream->events()));
//			}
//		}

//	private:
//		lifetime& _lifetime;

//	};

//} // namespace details

inline auto start_spawning(lifetime &alive_while) -> details::start_spawning_helper {
    return details::start_spawning_helper(alive_while);
}

}  // namespace rpl

// 重载操作符|，使得可以和start_spawning一起使用
template <typename Value, typename Func>
auto operator|(rpl::producer<Value> &&producer, Func &&func) {
    return func(std::move(producer));
}
