#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <cstring>
#include <exception>

namespace ewr_test {

struct TestInfo {
    std::string suite_name;
    std::string test_name;
    std::function<void()> func;
};

class TestRegistry {
public:
    static TestRegistry& Get() {
        static TestRegistry instance;
        return instance;
    }

    void Register(const std::string& suite, const std::string& name, std::function<void()> func) {
        tests_.push_back({suite, name, func});
    }

    const std::vector<TestInfo>& GetTests() const {
        return tests_;
    }

private:
    std::vector<TestInfo> tests_;
};

class TestRegistrar {
public:
    TestRegistrar(const std::string& suite, const std::string& name, std::function<void()> func) {
        TestRegistry::Get().Register(suite, name, func);
    }
};

struct TestContext {
    static TestContext& Get() {
        static TestContext instance;
        return instance;
    }
    
    bool current_test_failed = false;
    int total_assertions = 0;
    int failed_assertions = 0;
    int passed_tests = 0;
    int failed_tests = 0;
};

inline int RunAllTests() {
    auto& registry = TestRegistry::Get();
    auto& context = TestContext::Get();
    std::cout << "[===============================================================]" << std::endl;
    std::cout << "[==========] Running " << registry.GetTests().size() << " tests." << std::endl;
    for (const auto& test : registry.GetTests()) {
        std::cout << "[ RUN      ] " << test.suite_name << "." << test.test_name << std::endl;
        context.current_test_failed = false;
        try {
            test.func();
        } catch (const std::exception& e) {
            std::cerr << "Caught exception: " << e.what() << std::endl;
            context.current_test_failed = true;
        } catch (...) {
            std::cerr << "Caught unknown exception" << std::endl;
            context.current_test_failed = true;
        }
        if (context.current_test_failed) {
            std::cout << "[  FAILED  ] " << test.suite_name << "." << test.test_name << std::endl;
            context.failed_tests++;
        } else {
            std::cout << "[       OK ] " << test.suite_name << "." << test.test_name << std::endl;
            context.passed_tests++;
        }
    }
    std::cout << "[==========] Done running tests." << std::endl;
    std::cout << "[  PASSED  ] " << context.passed_tests << " tests." << std::endl;
    if (context.failed_tests > 0) {
        std::cout << "[  FAILED  ] " << context.failed_tests << " tests." << std::endl;
        return 1;
    }
    return 0;
}

} // namespace ewr_test

#define TEST_CONCAT_IMPL(x, y) x##y
#define TEST_CONCAT(x, y) TEST_CONCAT_IMPL(x, y)

#define TEST(Suite, Name) \
    void TEST_CONCAT(Suite, _##Name##_Func)(); \
    static ewr_test::TestRegistrar TEST_CONCAT(Suite, _##Name##_Registrar)(#Suite, #Name, TEST_CONCAT(Suite, _##Name##_Func)); \
    void TEST_CONCAT(Suite, _##Name##_Func)()

#define EXPECT_TRUE(cond) \
    do { \
        ewr_test::TestContext::Get().total_assertions++; \
        if (!(cond)) { \
            std::cerr << "Assertion Failed: EXPECT_TRUE(" << #cond << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            ewr_test::TestContext::Get().current_test_failed = true; \
            ewr_test::TestContext::Get().failed_assertions++; \
        } \
    } while(0)

#define EXPECT_FALSE(cond) \
    do { \
        ewr_test::TestContext::Get().total_assertions++; \
        if ((cond)) { \
            std::cerr << "Assertion Failed: EXPECT_FALSE(" << #cond << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            ewr_test::TestContext::Get().current_test_failed = true; \
            ewr_test::TestContext::Get().failed_assertions++; \
        } \
    } while(0)

#define EXPECT_EQ(val1, val2) \
    do { \
        ewr_test::TestContext::Get().total_assertions++; \
        auto v1 = (val1); \
        auto v2 = (val2); \
        if (!(v1 == v2)) { \
            std::cerr << "Assertion Failed: EXPECT_EQ(" << #val1 << ", " << #val2 << ") at " << __FILE__ << ":" << __LINE__ \
                      << " | Expected: " << v1 << ", Actual: " << v2 << std::endl; \
            ewr_test::TestContext::Get().current_test_failed = true; \
            ewr_test::TestContext::Get().failed_assertions++; \
        } \
    } while(0)

#define EXPECT_NE(val1, val2) \
    do { \
        ewr_test::TestContext::Get().total_assertions++; \
        auto v1 = (val1); \
        auto v2 = (val2); \
        if (v1 == v2) { \
            std::cerr << "Assertion Failed: EXPECT_NE(" << #val1 << ", " << #val2 << ") at " << __FILE__ << ":" << __LINE__ \
                      << " | Both are: " << v1 << std::endl; \
            ewr_test::TestContext::Get().current_test_failed = true; \
            ewr_test::TestContext::Get().failed_assertions++; \
        } \
    } while(0)

#define EXPECT_STREQ(str1, str2) \
    do { \
        ewr_test::TestContext::Get().total_assertions++; \
        const char* s1 = (str1); \
        const char* s2 = (str2); \
        if (s1 == nullptr || s2 == nullptr) { \
            if (s1 != s2) { \
                std::cerr << "Assertion Failed: EXPECT_STREQ(" << #str1 << ", " << #str2 << ") at " << __FILE__ << ":" << __LINE__ \
                          << " | One of the strings is null" << std::endl; \
                ewr_test::TestContext::Get().current_test_failed = true; \
                ewr_test::TestContext::Get().failed_assertions++; \
            } \
        } else if (std::strcmp(s1, s2) != 0) { \
            std::cerr << "Assertion Failed: EXPECT_STREQ(" << #str1 << ", " << #str2 << ") at " << __FILE__ << ":" << __LINE__ \
                      << " | Expected: \"" << s1 << "\", Actual: \"" << s2 << "\"" << std::endl; \
            ewr_test::TestContext::Get().current_test_failed = true; \
            ewr_test::TestContext::Get().failed_assertions++; \
        } \
    } while(0)
