#include <larvae/precomp.h>
#include <larvae/assertions.h>
#include <sstream>

namespace larvae
{
    std::string FormatAssertionMessage(
        const char* file,
        std::uint_least32_t line,
        const char* expression,
        const std::string& expected_str,
        const std::string& actual_str,
        const std::string& custom_message)
    {
        std::ostringstream ss;
        ss << file << ":" << line << ": Assertion failed\n";
        ss << "  Expression: " << expression << "\n";

        if (!expected_str.empty())
        {
            ss << "  Expected: " << expected_str << "\n";
        }

        if (!actual_str.empty())
        {
            ss << "  Actual: " << actual_str << "\n";
        }

        if (!custom_message.empty())
        {
            ss << "  Message: " << custom_message << "\n";
        }

        return ss.str();
    }
}
