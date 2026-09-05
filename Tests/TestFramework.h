#pragma once

#include <string_view>

namespace Gilgamesh::Testing
{
class TestContext
{
public:
    void Check(
        bool condition,
        std::string_view expression,
        std::string_view message,
        std::string_view file,
        int line) noexcept;

    [[nodiscard]] int FailureCount() const noexcept { return failureCount_; }

private:
    int failureCount_ = 0;
};

using TestFunction = void (*)(TestContext&);

class TestRegistrar
{
public:
    TestRegistrar(
        std::string_view suite,
        std::string_view name,
        TestFunction function);
};

int RunTests(int argc, char* argv[]);
}

#define GILGAMESH_TEST_DETAIL_JOIN_IMPL(left, right) left##right
#define GILGAMESH_TEST_DETAIL_JOIN(left, right) \
    GILGAMESH_TEST_DETAIL_JOIN_IMPL(left, right)

#define GILGAMESH_TEST_DETAIL_DEFINE(suite, name, line)                         \
    static void GILGAMESH_TEST_DETAIL_JOIN(GilgameshTest_, line)(              \
        ::Gilgamesh::Testing::TestContext& testContext);                       \
    namespace                                                                   \
    {                                                                           \
    const ::Gilgamesh::Testing::TestRegistrar                                   \
        GILGAMESH_TEST_DETAIL_JOIN(GilgameshTestRegistrar_, line){             \
            suite,                                                              \
            name,                                                               \
            &GILGAMESH_TEST_DETAIL_JOIN(GilgameshTest_, line)                  \
        };                                                                      \
    }                                                                           \
    static void GILGAMESH_TEST_DETAIL_JOIN(GilgameshTest_, line)(              \
        ::Gilgamesh::Testing::TestContext& testContext)

#define GILGAMESH_TEST(suite, name) \
    GILGAMESH_TEST_DETAIL_DEFINE(suite, name, __LINE__)

#define GILGAMESH_CHECK_MESSAGE(condition, message)                             \
    do                                                                          \
    {                                                                           \
        testContext.Check(                                                      \
            static_cast<bool>(condition),                                       \
            #condition,                                                         \
            message,                                                            \
            __FILE__,                                                           \
            __LINE__);                                                          \
    } while (false)
