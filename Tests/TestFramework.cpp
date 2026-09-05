#include "TestFramework.h"

#include <cstdio>
#include <vector>

namespace Gilgamesh::Testing
{
namespace
{
struct TestCase
{
    std::string_view suite;
    std::string_view name;
    TestFunction function;
};

std::vector<TestCase>& RegisteredTests()
{
    static std::vector<TestCase> tests;
    return tests;
}

bool MatchesFilter(const TestCase& test, std::string_view filter) noexcept
{
    return filter.empty()
        || test.suite.find(filter) != std::string_view::npos
        || test.name.find(filter) != std::string_view::npos;
}

void PrintName(const TestCase& test)
{
    std::printf(
        "%.*s.%.*s",
        static_cast<int>(test.suite.size()),
        test.suite.data(),
        static_cast<int>(test.name.size()),
        test.name.data());
}
}

void TestContext::Check(
    bool condition,
    std::string_view expression,
    std::string_view message,
    std::string_view file,
    int line) noexcept
{
    if (condition)
        return;

    std::fprintf(
        stderr,
        "%.*s(%d): FAILED: %.*s [%.*s]\n",
        static_cast<int>(file.size()),
        file.data(),
        line,
        static_cast<int>(message.size()),
        message.data(),
        static_cast<int>(expression.size()),
        expression.data());
    ++failureCount_;
}

TestRegistrar::TestRegistrar(
    std::string_view suite,
    std::string_view name,
    TestFunction function)
{
    RegisteredTests().push_back({ suite, name, function });
}

int RunTests(int argc, char* argv[])
{
    bool listOnly = false;
    std::string_view filter;

    if (argc > 1)
    {
        const std::string_view argument = argv[1];
        if (argument == "--list")
            listOnly = true;
        else
            filter = argument;
    }

    if (argc > 2)
    {
        std::fprintf(stderr, "Usage: GilgameshTests [--list|filter]\n");
        return 2;
    }

    int selected = 0;
    int failed = 0;

    for (const TestCase& test : RegisteredTests())
    {
        if (!MatchesFilter(test, filter))
            continue;

        ++selected;
        if (listOnly)
        {
            PrintName(test);
            std::printf("\n");
            continue;
        }

        std::printf("[ RUN      ] ");
        PrintName(test);
        std::printf("\n");

        TestContext context;
        test.function(context);
        if (context.FailureCount() == 0)
        {
            std::printf("[       OK ] ");
        }
        else
        {
            std::printf("[  FAILED  ] ");
            ++failed;
        }
        PrintName(test);
        std::printf("\n");
    }

    if (listOnly)
        return 0;

    if (selected == 0)
    {
        std::fprintf(stderr, "No tests matched the requested filter.\n");
        return 2;
    }

    std::printf(
        "[==========] %d test(s) ran; %d passed; %d failed.\n",
        selected,
        selected - failed,
        failed);
    return failed == 0 ? 0 : 1;
}
}
