#include <larvae/larvae.h>

#include "hive/core/assert.h"

int main(int argc, char** argv)
{
    hive::Assert(false, "test");
    return larvae::RunAllTests(argc, argv);
}
