#include <larvae/larvae.h>
#include <iostream>

int main(int argc, char** argv)
{
    std::cout << "Larvae Benchmark Runner\n\n";
    return larvae::RunAllBenchmarks(argc, argv);
}
