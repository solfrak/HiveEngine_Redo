#include <larvae/precomp.h>
#include <larvae/larvae.h>

namespace larvae
{
    int RunAllTests(int argc, char** argv)
    {
        TestRunnerConfig config = ParseCommandLine(argc, argv);
        TestRunner runner{config};
        return runner.Run();
    }
}
