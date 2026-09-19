#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace avaui_tests {

struct TestFailure {
    std::string message;
    int line;
};

class TestContext {
public:
    void Check(bool condition, const std::string& expr, int line) {
        ++checks_;
        if (!condition) {
            failures_.push_back({expr, line});
        }
    }

    template <typename T>
    static std::string ToDisplay(const T& value) {
        std::ostringstream oss;
        if constexpr (std::is_enum_v<T>) {
            oss << static_cast<std::underlying_type_t<T>>(value);
        } else {
            oss << value;
        }
        return oss.str();
    }

    template <typename A, typename B>
    void CheckEqual(const A& actual, const B& expected, const std::string& expr, int line) {
        ++checks_;
        if (!(actual == expected)) {
            std::ostringstream oss;
            oss << expr << " (got " << ToDisplay(actual) << ", expected " << ToDisplay(expected) << ")";
            failures_.push_back({oss.str(), line});
        }
    }

    bool Ok() const { return failures_.empty(); }
    const std::vector<TestFailure>& Failures() const { return failures_; }
    int ChecksRun() const { return checks_; }

private:
    std::vector<TestFailure> failures_;
    int checks_ = 0;
};

using TestFn = std::function<void(TestContext&)>;

struct TestCase {
    std::string suite;
    std::string name;
    TestFn fn;
};

class TestRegistry {
public:
    static TestRegistry& Instance() {
        static TestRegistry registry;
        return registry;
    }

    void Add(TestCase test) { tests_.push_back(std::move(test)); }

    int RunAll() {
        int passed = 0;
        int failed = 0;
        for (const auto& test : tests_) {
            TestContext ctx;
            bool threw = false;
            std::string exceptionMessage;
            try {
                test.fn(ctx);
            } catch (const std::exception& e) {
                threw = true;
                exceptionMessage = e.what();
            } catch (...) {
                threw = true;
                exceptionMessage = "unknown exception";
            }

            if (threw) {
                std::cout << "[FAIL] " << test.suite << "." << test.name
                          << " threw: " << exceptionMessage << "\n";
                ++failed;
                continue;
            }

            if (ctx.Ok()) {
                std::cout << "[ OK ] " << test.suite << "." << test.name
                          << " (" << ctx.ChecksRun() << " checks)\n";
                ++passed;
            } else {
                std::cout << "[FAIL] " << test.suite << "." << test.name << "\n";
                for (const auto& failure : ctx.Failures()) {
                    std::cout << "        line " << failure.line << ": " << failure.message << "\n";
                }
                ++failed;
            }
        }
        std::cout << "\n" << passed << " passed, " << failed << " failed, "
                  << tests_.size() << " total\n";
        return failed == 0 ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

struct TestRegistrar {
    TestRegistrar(const std::string& suite, const std::string& name, TestFn fn) {
        TestRegistry::Instance().Add({suite, name, std::move(fn)});
    }
};

}

#define AVAUI_TEST(suite, name) \
    static void suite##_##name(avaui_tests::TestContext& ctx); \
    static avaui_tests::TestRegistrar suite##_##name##_registrar(#suite, #name, suite##_##name); \
    static void suite##_##name(avaui_tests::TestContext& ctx)

#define EXPECT_TRUE(cond) ctx.Check((cond), #cond, __LINE__)
#define EXPECT_FALSE(cond) ctx.Check(!(cond), "!(" #cond ")", __LINE__)
#define EXPECT_EQ(actual, expected) ctx.CheckEqual((actual), (expected), #actual " == " #expected, __LINE__)
