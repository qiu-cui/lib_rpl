#include <rpl/producer.h>

#include <iostream>

#define REQUIRE(X)                                                 \
    if (!(X)) {                                                    \
        std::cerr << "[ERR] " << #X << "--->" << (X) << std::endl; \
        return EXIT_FAILURE;                                       \
    }

using namespace rpl;

using no_error = rpl::Error;

class OnDestructor {
   public:
    OnDestructor(std::function<void()> callback) : _callback(std::move(callback)) {}
    ~OnDestructor() {
        if (_callback) { _callback(); }
    }

   private:
    std::function<void()> _callback;
};

int main() {
    {
        // producer next, done and lifetime end test

        auto lifetimeEnded = std::make_shared<bool>(false);
        auto sum = std::make_shared<int>(0);
        auto doneGenerated = std::make_shared<bool>(false);
        auto destroyed = std::make_shared<bool>(false);
        {
            auto destroyCaller = std::make_shared<OnDestructor>([=] { *destroyed = true; });
            {
                auto alive = rpl::make_producer<int>([=](auto &&consumer) {
                                 (void)destroyCaller;
                                 consumer.on_next(1);
                                 consumer.on_next(2);
                                 consumer.on_next(3);
                                 consumer.on_completed();
                                 return [=] {
                                     (void)destroyCaller;
                                     *lifetimeEnded = true;
                                 };
                             })
                                 .start(
                                     [=](int value) {
                                         (void)destroyCaller;
                                         *sum += value;
                                     },
                                     [=](no_error) { (void)destroyCaller; },
                                     [=]() {
                                         (void)destroyCaller;
                                         *doneGenerated = true;
                                     });
            }
        }
        REQUIRE(*sum == 1 + 2 + 3);
        REQUIRE(*doneGenerated);
        REQUIRE(*lifetimeEnded);
        REQUIRE(*destroyed);
    }
    {
        // producer error test
        auto errorGenerated = std::make_shared<bool>(false);
        {
            auto alive = make_producer<no_value>([=](auto &&consumer) {
                             consumer.on_error(rxcpp::util::error_ptr());
                             return lifetime();
                         }).start([=](no_value) {}, [=](no_error) { *errorGenerated = true; }, [=]() {});
        }
        REQUIRE(*errorGenerated);
    }
    {
        // nested lifetimes test
        auto lifetimeEndCount = std::make_shared<int>(0);
        {
            auto lifetimes = rpl::lifetime{};
            {
                auto testProducer =
                    rpl::make_producer<rpl::no_value>([=](auto &&consumer) { return [=] { ++*lifetimeEndCount; }; });
                testProducer.start_copy([=](rpl::no_value) {}, [=](rpl::Error) {}, [=] {}, lifetimes);
                std::move(testProducer).start([=](rpl::no_value) {}, [=](rpl::Error) {}, [=] {}, lifetimes);
            }
            REQUIRE(*lifetimeEndCount == 0);
        }
        REQUIRE(*lifetimeEndCount == 2);
    }

    {
        // nested producers test
        auto sum = std::make_shared<int>(0);
        auto lifetimeEndCount = std::make_shared<int>(0);
        auto saved = rpl::lifetime();
        {
            rpl::make_producer<int>([=](auto &&consumer) {
                auto inner = rpl::make_producer<int>([=](auto &&consumer) {
                    consumer.on_next(1);
                    consumer.on_next(2);
                    consumer.on_next(3);
                    return [=] { ++*lifetimeEndCount; };
                });
                auto result = rpl::lifetime();
                result.add([=] { ++*lifetimeEndCount; });
                inner.start_copy([=](int value) { consumer.on_next(value); }, [=](rpl::Error) {}, [=] {}, result);
                std::move(inner).start([=](int value) { consumer.on_next(value); }, [=](rpl::Error) {}, [=] {}, result);
                return result;
            }).start([=](int value) { *sum += value; }, [=](rpl::Error) {}, [=] {}, saved);
        }
        REQUIRE(*sum == 1 + 2 + 3 + 1 + 2 + 3);
        REQUIRE(*lifetimeEndCount == 0);
        std::exchange(saved, rpl::lifetime());  // saved.destroy();
        REQUIRE(*lifetimeEndCount == 3);
    }

    {
        // tuple producer test
        auto result = std::make_shared<int>(0);
        {
            rpl::make_producer<std::tuple<int, double>>([=](auto &&consumer) {
                consumer.on_next(std::make_tuple(1, 2.));
                return lifetime();
            })
                .start([=](std::tuple<int, double> t) { *result = std::get<0>(t) + int(std::get<1>(t)); },
                       [=](no_error error) {}, [=]() {});
        }
        REQUIRE(*result == 3);
    }
}