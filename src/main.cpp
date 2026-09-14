#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ollama_client.h"
#include "uwuifier.h"

namespace {
    enum class Mode {
        Classic,
        Ai,
        Hybrid
    };

    class AppSettings {
    public:
        Mode mode = Mode::Classic;
        bool fallbackToClassic = true;
        ollama::Options ollama;
    };

    static std::string trim(const std::string& text) {
        size_t start = 0;

        while (start < text.size() && std::isspace(
            static_cast<unsigned char>(text[start])
        )) {
            ++start;
        }

        size_t end = text.size();

        while (end > start && std::isspace(
            static_cast<unsigned char>(text[end - 1])
        )) {
            --end;
        }

        return text.substr(start, end - start);
    }

    static std::string toLower(std::string text) {
        for (char& character : text) {
            character = static_cast<char>(std::tolower(
                static_cast<unsigned char>(character)
            ));
        }

        return text;
    }

    static bool parseBool(
        const std::string& value,
        bool fallback
    ) {
        std::string lowered = toLower(trim(value));

        if (
            lowered == "1" ||
            lowered == "true" ||
            lowered == "yes" ||
            lowered == "on"
        ) {
            return true;
        }

        if (
            lowered == "0" ||
            lowered == "false" ||
            lowered == "no" ||
            lowered == "off"
        ) {
            return false;
        }

        return fallback;
    }

    static Mode parseMode(
        const std::string& value,
        Mode fallback
    ) {
        std::string lowered = toLower(trim(value));

        if (lowered == "classic")
            return Mode::Classic;

        if (lowered == "ai" || lowered == "ollama")
            return Mode::Ai;

        if (lowered == "hybrid")
            return Mode::Hybrid;

        return fallback;
    }

    static void applySetting(
        AppSettings& settings,
        const std::string& key,
        const std::string& value
    ) {
        std::string normalizedKey = toLower(trim(key));
        std::string trimmedValue = trim(value);

        if (normalizedKey == "mode") {
            settings.mode = parseMode(trimmedValue, settings.mode);
        }
        else if (normalizedKey == "ollama_url") {
            settings.ollama.url = trimmedValue;
        }
        else if (normalizedKey == "ollama_model") {
            settings.ollama.model = trimmedValue;
        }
        else if (normalizedKey == "ollama_timeout_ms") {
            try {
                settings.ollama.timeoutMs = std::stoi(trimmedValue);
            }
            catch (const std::exception&) {
            }
        }
        else if (normalizedKey == "ollama_temperature") {
            try {
                settings.ollama.temperature = std::stod(trimmedValue);
            }
            catch (const std::exception&) {
            }
        }
        else if (normalizedKey == "ollama_max_tokens") {
            try {
                settings.ollama.maxTokens = std::stoi(trimmedValue);
            }
            catch (const std::exception&) {
            }
        }
        else if (normalizedKey == "fallback_to_classic") {
            settings.fallbackToClassic = parseBool(
                trimmedValue,
                settings.fallbackToClassic
            );
        }
    }

    static void loadConfigFile(
        AppSettings& settings,
        const std::filesystem::path& path
    ) {
        std::ifstream config(path);

        if (!config)
            return;

        std::string line;

        while (std::getline(config, line)) {
            std::string trimmedLine = trim(line);

            if (
                trimmedLine.empty() ||
                trimmedLine[0] == '#' ||
                trimmedLine[0] == ';'
            ) {
                continue;
            }

            size_t equals = trimmedLine.find('=');

            if (equals == std::string::npos)
                continue;

            applySetting(
                settings,
                trimmedLine.substr(0, equals),
                trimmedLine.substr(equals + 1)
            );
        }
    }

    static void applyEnvironment(
        AppSettings& settings
    ) {
        struct EnvSetting {
            const char* name;
            const char* key;
        };

        const EnvSetting envSettings[] = {
            { "UWUIFY_MODE", "mode" },
            { "UWUIFY_OLLAMA_URL", "ollama_url" },
            { "UWUIFY_OLLAMA_MODEL", "ollama_model" },
            { "UWUIFY_OLLAMA_TIMEOUT_MS", "ollama_timeout_ms" },
            { "UWUIFY_OLLAMA_TEMPERATURE", "ollama_temperature" },
            { "UWUIFY_OLLAMA_MAX_TOKENS", "ollama_max_tokens" },
            { "UWUIFY_FALLBACK_TO_CLASSIC", "fallback_to_classic" }
        };

        for (const EnvSetting& envSetting : envSettings) {
            const char* value = std::getenv(envSetting.name);

            if (value != nullptr)
                applySetting(settings, envSetting.key, value);
        }
    }

    static AppSettings loadSettings(
        const char* executablePath
    ) {
        AppSettings settings;

        std::filesystem::path exePath(executablePath);
        std::filesystem::path exeConfig =
            exePath.parent_path() / "uwuifier.conf";

        loadConfigFile(settings, exeConfig);
        loadConfigFile(settings, "uwuifier.conf");
        applyEnvironment(settings);

        return settings;
    }

    static std::string makeAiPrompt(
        const std::string& text
    ) {
        return
            "/no_think\n\n"
            "Rewrite the following text into cute uwu style.\n"
            "Keep the original meaning, language, paragraph breaks, and rough length.\n"
            "Preserve URLs, code, commands, and file paths exactly when possible.\n"
            "Return only the transformed text, with no quotes or explanation.\n\n"
            "Text:\n"
            + text;
    }

    static std::string makeHybridPrompt(
        const std::string& original,
        const std::string& classic
    ) {
        return
            "/no_think\n\n"
            "Polish this uwuified text so it sounds natural and cute.\n"
            "Keep the original meaning, paragraph breaks, and rough length.\n"
            "Do not add commentary, explanations, labels, or quotes.\n"
            "Preserve URLs, code, commands, and file paths exactly when possible.\n\n"
            "Original text:\n"
            + original +
            "\n\nClassic uwuified text:\n"
            + classic;
    }

    static std::string transformText(
        const std::string& text,
        const AppSettings& settings,
        bool& usedFallback
    ) {
        usedFallback = false;

        if (settings.mode == Mode::Classic)
            return uwuifier::uwuify(text);

        std::string classic = uwuifier::uwuify(text);

        std::string prompt =
            settings.mode == Mode::Hybrid ?
            makeHybridPrompt(text, classic) :
            makeAiPrompt(text);

        ollama::Result aiResult =
            ollama::generate(settings.ollama, prompt);

        if (aiResult.ok && !trim(aiResult.text).empty())
            return aiResult.text;

        if (settings.fallbackToClassic) {
            usedFallback = true;
            std::cerr
                << "Ollama failed; using classic uwuifier fallback";

            if (!aiResult.error.empty())
                std::cerr << ": " << aiResult.error;

            std::cerr << "\n";
            return classic;
        }

        throw std::runtime_error(
            aiResult.error.empty() ?
            "Ollama failed." :
            aiResult.error
        );
    }
}

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

    AppSettings settings = loadSettings(argv[0]);

    std::string result;
    bool usedFallback = false;

    try {
        result = transformText(text, settings, usedFallback);
    }
    catch (const std::exception& exception) {
        std::cerr << "Could not uwuify input: " << exception.what() << "\n";
        return 1;
    }

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
