#include "ollama_client.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ollama {
    static std::wstring widen(const std::string& text) {
        if (text.empty())
            return L"";

        int size = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0
        );

        if (size <= 0)
            throw std::runtime_error("Could not convert UTF-8 text to UTF-16.");

        std::wstring output(size, L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            output.data(),
            size
        );

        return output;
    }

    static std::string narrow(const std::wstring& text) {
        if (text.empty())
            return "";

        int size = WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0,
            nullptr,
            nullptr
        );

        if (size <= 0)
            throw std::runtime_error("Could not convert UTF-16 text to UTF-8.");

        std::string output(size, '\0');

        WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            output.data(),
            size,
            nullptr,
            nullptr
        );

        return output;
    }

    static std::string getLastWindowsError() {
        DWORD code = GetLastError();

        if (code == 0)
            return "Unknown Windows HTTP error.";

        LPWSTR buffer = nullptr;

        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr
        );

        if (size == 0 || buffer == nullptr)
            return "Windows HTTP error " + std::to_string(code) + ".";

        std::wstring message(buffer, size);
        LocalFree(buffer);

        while (
            !message.empty() &&
            (
                message.back() == L'\r' ||
                message.back() == L'\n' ||
                message.back() == L' '
            )
        ) {
            message.pop_back();
        }

        return narrow(message);
    }

    static std::string jsonEscape(const std::string& text) {
        std::ostringstream output;

        for (unsigned char character : text) {
            switch (character) {
                case '\\':
                    output << "\\\\";
                    break;
                case '"':
                    output << "\\\"";
                    break;
                case '\b':
                    output << "\\b";
                    break;
                case '\f':
                    output << "\\f";
                    break;
                case '\n':
                    output << "\\n";
                    break;
                case '\r':
                    output << "\\r";
                    break;
                case '\t':
                    output << "\\t";
                    break;
                default:
                    if (character < 0x20) {
                        output
                            << "\\u"
                            << std::hex
                            << std::setw(4)
                            << std::setfill('0')
                            << static_cast<int>(character)
                            << std::dec;
                    }
                    else {
                        output << character;
                    }
                    break;
            }
        }

        return output.str();
    }

    static void appendUtf8Codepoint(
        std::string& output,
        unsigned int codepoint
    ) {
        if (codepoint <= 0x7F) {
            output += static_cast<char>(codepoint);
        }
        else if (codepoint <= 0x7FF) {
            output += static_cast<char>(0xC0 | (codepoint >> 6));
            output += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else if (codepoint <= 0xFFFF) {
            output += static_cast<char>(0xE0 | (codepoint >> 12));
            output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            output += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else {
            output += static_cast<char>(0xF0 | (codepoint >> 18));
            output += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            output += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }

    static int hexValue(char character) {
        if (character >= '0' && character <= '9')
            return character - '0';

        if (character >= 'a' && character <= 'f')
            return character - 'a' + 10;

        if (character >= 'A' && character <= 'F')
            return character - 'A' + 10;

        return -1;
    }

    static bool readJsonHexCodepoint(
        const std::string& text,
        size_t position,
        unsigned int& codepoint
    ) {
        if (position + 4 > text.size())
            return false;

        unsigned int value = 0;

        for (size_t i = 0; i < 4; ++i) {
            int hex = hexValue(text[position + i]);

            if (hex < 0)
                return false;

            value = (value << 4) | static_cast<unsigned int>(hex);
        }

        codepoint = value;
        return true;
    }

    static std::string decodeJsonString(
        const std::string& text,
        size_t& position
    ) {
        if (position >= text.size() || text[position] != '"')
            throw std::runtime_error("Expected JSON string.");

        ++position;

        std::string output;

        while (position < text.size()) {
            char character = text[position++];

            if (character == '"')
                return output;

            if (character != '\\') {
                output += character;
                continue;
            }

            if (position >= text.size())
                throw std::runtime_error("Incomplete JSON escape.");

            char escaped = text[position++];

            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    output += escaped;
                    break;
                case 'b':
                    output += '\b';
                    break;
                case 'f':
                    output += '\f';
                    break;
                case 'n':
                    output += '\n';
                    break;
                case 'r':
                    output += '\r';
                    break;
                case 't':
                    output += '\t';
                    break;
                case 'u': {
                    unsigned int codepoint = 0;

                    if (!readJsonHexCodepoint(text, position, codepoint))
                        throw std::runtime_error("Invalid JSON unicode escape.");

                    position += 4;

                    if (
                        codepoint >= 0xD800 &&
                        codepoint <= 0xDBFF &&
                        position + 6 <= text.size() &&
                        text[position] == '\\' &&
                        text[position + 1] == 'u'
                    ) {
                        unsigned int low = 0;

                        if (
                            readJsonHexCodepoint(
                                text,
                                position + 2,
                                low
                            ) &&
                            low >= 0xDC00 &&
                            low <= 0xDFFF
                        ) {
                            position += 6;
                            codepoint =
                                0x10000 +
                                ((codepoint - 0xD800) << 10) +
                                (low - 0xDC00);
                        }
                    }

                    appendUtf8Codepoint(output, codepoint);
                    break;
                }
                default:
                    throw std::runtime_error("Unknown JSON escape.");
            }
        }

        throw std::runtime_error("Unterminated JSON string.");
    }

    static bool extractJsonStringField(
        const std::string& json,
        const std::string& field,
        std::string& value
    ) {
        std::string needle = "\"" + field + "\"";
        size_t position = 0;

        while ((position = json.find(needle, position)) != std::string::npos) {
            position += needle.size();

            while (
                position < json.size() &&
                (
                    json[position] == ' ' ||
                    json[position] == '\n' ||
                    json[position] == '\r' ||
                    json[position] == '\t'
                )
            ) {
                ++position;
            }

            if (position >= json.size() || json[position] != ':')
                continue;

            ++position;

            while (
                position < json.size() &&
                (
                    json[position] == ' ' ||
                    json[position] == '\n' ||
                    json[position] == '\r' ||
                    json[position] == '\t'
                )
            ) {
                ++position;
            }

            if (position >= json.size() || json[position] != '"')
                continue;

            try {
                value = decodeJsonString(json, position);
                return true;
            }
            catch (const std::exception&) {
                return false;
            }
        }

        return false;
    }

    static std::string buildGenerateUrl(const std::string& configuredUrl) {
        if (configuredUrl.find("/api/generate") != std::string::npos)
            return configuredUrl;

        if (configuredUrl.empty())
            return "http://127.0.0.1:11434/api/generate";

        if (configuredUrl.back() == '/')
            return configuredUrl + "api/generate";

        return configuredUrl + "/api/generate";
    }

    class WinHttpHandle {
    private:
        HINTERNET _handle = nullptr;

    public:
        explicit WinHttpHandle(HINTERNET handle) {
            _handle = handle;
        }

        ~WinHttpHandle() {
            if (_handle != nullptr)
                WinHttpCloseHandle(_handle);
        }

        HINTERNET get() const {
            return _handle;
        }

        explicit operator bool() const {
            return _handle != nullptr;
        }
    };

    Result generate(
        const Options& options,
        const std::string& prompt
    ) {
        Result result;

        try {
            std::string apiUrl = buildGenerateUrl(options.url);
            std::wstring wideUrl = widen(apiUrl);

            URL_COMPONENTS parts = {};
            parts.dwStructSize = sizeof(parts);

            std::vector<wchar_t> host(256);
            std::vector<wchar_t> path(2048);
            std::vector<wchar_t> extra(2048);

            parts.lpszHostName = host.data();
            parts.dwHostNameLength = static_cast<DWORD>(host.size());
            parts.lpszUrlPath = path.data();
            parts.dwUrlPathLength = static_cast<DWORD>(path.size());
            parts.lpszExtraInfo = extra.data();
            parts.dwExtraInfoLength = static_cast<DWORD>(extra.size());

            if (!WinHttpCrackUrl(
                wideUrl.c_str(),
                static_cast<DWORD>(wideUrl.size()),
                0,
                &parts
            )) {
                result.error = "Invalid Ollama URL: " + apiUrl;
                return result;
            }

            std::wstring hostName(
                parts.lpszHostName,
                parts.dwHostNameLength
            );

            std::wstring requestPath(
                parts.lpszUrlPath,
                parts.dwUrlPathLength
            );

            if (parts.dwExtraInfoLength > 0) {
                requestPath.append(
                    parts.lpszExtraInfo,
                    parts.dwExtraInfoLength
                );
            }

            std::ostringstream body;
            body
                << "{\"model\":\""
                << jsonEscape(options.model)
                << "\",\"prompt\":\""
                << jsonEscape(prompt)
                << "\",\"stream\":false,\"think\":false,\"options\":{\"temperature\":"
                << options.temperature
                << ",\"num_predict\":"
                << options.maxTokens
                << "}}";

            std::string bodyText = body.str();

            WinHttpHandle session(WinHttpOpen(
                L"UWUAutocorrect/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            ));

            if (!session) {
                result.error = getLastWindowsError();
                return result;
            }

            int timeout = options.timeoutMs > 0 ? options.timeoutMs : 8000;

            WinHttpSetTimeouts(
                session.get(),
                timeout,
                timeout,
                timeout,
                timeout
            );

            WinHttpHandle connection(WinHttpConnect(
                session.get(),
                hostName.c_str(),
                parts.nPort,
                0
            ));

            if (!connection) {
                result.error = getLastWindowsError();
                return result;
            }

            DWORD flags =
                parts.nScheme == INTERNET_SCHEME_HTTPS ?
                WINHTTP_FLAG_SECURE :
                0;

            WinHttpHandle request(WinHttpOpenRequest(
                connection.get(),
                L"POST",
                requestPath.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags
            ));

            if (!request) {
                result.error = getLastWindowsError();
                return result;
            }

            const wchar_t* headers =
                L"Content-Type: application/json; charset=utf-8\r\n";

            if (!WinHttpSendRequest(
                request.get(),
                headers,
                static_cast<DWORD>(-1L),
                bodyText.data(),
                static_cast<DWORD>(bodyText.size()),
                static_cast<DWORD>(bodyText.size()),
                0
            )) {
                result.error = getLastWindowsError();
                return result;
            }

            if (!WinHttpReceiveResponse(request.get(), nullptr)) {
                result.error = getLastWindowsError();
                return result;
            }

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);

            if (!WinHttpQueryHeaders(
                request.get(),
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusCodeSize,
                WINHTTP_NO_HEADER_INDEX
            )) {
                result.error = getLastWindowsError();
                return result;
            }

            std::string response;

            while (true) {
                DWORD available = 0;

                if (!WinHttpQueryDataAvailable(request.get(), &available)) {
                    result.error = getLastWindowsError();
                    return result;
                }

                if (available == 0)
                    break;

                std::string chunk(available, '\0');
                DWORD read = 0;

                if (!WinHttpReadData(
                    request.get(),
                    chunk.data(),
                    available,
                    &read
                )) {
                    result.error = getLastWindowsError();
                    return result;
                }

                chunk.resize(read);
                response += chunk;
            }

            if (statusCode < 200 || statusCode >= 300) {
                result.error =
                    "Ollama returned HTTP " +
                    std::to_string(statusCode) +
                    ": " +
                    response;
                return result;
            }

            std::string generated;

            if (!extractJsonStringField(response, "response", generated)) {
                result.error = "Could not read Ollama response JSON.";
                return result;
            }

            result.ok = true;
            result.text = generated;
            return result;
        }
        catch (const std::exception& exception) {
            result.error = exception.what();
            return result;
        }
    }
}
