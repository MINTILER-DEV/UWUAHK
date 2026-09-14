#pragma once

#include <string>

namespace ollama {
    class Options {
    public:
        std::string url = "http://127.0.0.1:11434";
        std::string model = "llama3.2";
        int timeoutMs = 8000;
        double temperature = 0.35;
        int maxTokens = 512;
    };

    class Result {
    public:
        bool ok = false;
        std::string text;
        std::string error;
    };

    Result generate(
        const Options& options,
        const std::string& prompt
    );
}
