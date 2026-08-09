#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "uwuifier.h"

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: uwuifier.exe <input file> <output file>\n";
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    std::ifstream input(inputPath, std::ios::binary);

    if (!input)
    {
        std::cerr << "Could not open input file.\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    std::string text = buffer.str();

    input.close();

    std::string result = uwuifier::uwuify(text);

    std::ofstream output(
        outputPath,
        std::ios::binary | std::ios::trunc
    );

    if (!output)
    {
        std::cerr << "Could not open output file.\n";
        return 1;
    }

    output << result;
    output.close();

    return 0;
}