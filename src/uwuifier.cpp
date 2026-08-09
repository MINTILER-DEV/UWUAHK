#include <random>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <vector>
#include <cctype>
#include <cstdint>

#include "uwuifier.h"

namespace uwuifier {
    static Settings _settings = Settings();

    Settings& getSettings() {
        return _settings;
    }

    void resetSettings() {
        _settings = Settings();
    }

    std::random_device::result_type rd = std::random_device()();
    std::mt19937 gen = std::mt19937(rd);

    inline bool getChance(double chance) {
        return std::generate_canonical<double, 10>(gen) < chance;
    }

    bool isCaps(const std::string& text) {
        if (text.size() <= 1)
            return false;

        for (unsigned char character : text) {
            if (std::islower(character))
                return false;
        }

        return true;
    }

    std::string toLower(const std::string& text) {
        std::string newText;

        newText.reserve(text.size());

        for (unsigned char character : text)
            newText += static_cast<char>(std::tolower(character));

        return newText;
    }

    std::string toUpper(const std::string& text) {
        std::string newText;

        newText.reserve(text.size());

        for (unsigned char character : text)
            newText += static_cast<char>(std::toupper(character));

        return newText;
    }

    bool isNullOrWhiteSpace(const std::string& str) {
        return std::find_if(
            str.begin(),
            str.end(),
            [](unsigned char ch) {
                return !std::isspace(ch);
            }
        ) == str.end();
    }

    bool isLetter(char c) {
        return
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z');
    }

    bool isLowerVowel(char c) {
        return
            c == 'a' ||
            c == 'e' ||
            c == 'i' ||
            c == 'o' ||
            c == 'u';
    }

    static const std::unordered_map<std::string, std::string>
        wordReplacements = {
            { "you", "uwu" },
            { "no", "nu" },
            { "oh", "ow" },
            { "too", "two" },
            { "attempt", "attwempt" },
            { "config", "cwonfig" }
        };

    class SuffixChoice {
    private:
        const char* _text = nullptr;
        std::vector<SuffixChoice> _choices = {};

    public:
        SuffixChoice(const char* text) {
            _text = text;
        }

        SuffixChoice(const std::vector<SuffixChoice> choices) {
            _choices = choices;
        }

        const char* choose() const {
            if (!_choices.empty()) {
                std::uniform_int_distribution<int> dist(
                    0,
                    static_cast<int>(_choices.size()) - 1
                );

                return _choices.at(dist(gen)).choose();
            }

            if (_text != nullptr)
                return _text;

            return "";
        }
    };

    static const SuffixChoice& presuffixes =
        SuffixChoice(std::vector<SuffixChoice>{
            SuffixChoice("~"),
            SuffixChoice("~~"),
            SuffixChoice(",")
        });

    static const SuffixChoice& suffixes =
        SuffixChoice(std::vector<SuffixChoice>{
            SuffixChoice(":D"),

            SuffixChoice(std::vector<SuffixChoice>{
                SuffixChoice("xD"),
                SuffixChoice("XD")
            }),

            SuffixChoice(":P"),
            SuffixChoice(";3"),
            SuffixChoice("<{^v^}>"),
            SuffixChoice("^-^"),
            SuffixChoice("x3"),

            SuffixChoice(std::vector<SuffixChoice>{
                SuffixChoice("rawr"),
                SuffixChoice("rawr~"),
                SuffixChoice("rawr~~"),
                SuffixChoice("rawr x3"),
                SuffixChoice("rawr~ x3"),
                SuffixChoice("rawr~~ x3")
            }),

            SuffixChoice(std::vector<SuffixChoice>{
                SuffixChoice("owo"),
                SuffixChoice("owo~"),
                SuffixChoice("owo~~")
            }),

            SuffixChoice(std::vector<SuffixChoice>{
                SuffixChoice("uwu"),
                SuffixChoice("uwu~"),
                SuffixChoice("uwu~~")
            }),

            SuffixChoice("-.-"),
            SuffixChoice(">w<"),
            SuffixChoice(":3"),

            SuffixChoice(std::vector<SuffixChoice>{
                SuffixChoice("nya"),
                SuffixChoice("nya~"),
                SuffixChoice("nya~~"),
                SuffixChoice("nyaa"),
                SuffixChoice("nyaa~"),
                SuffixChoice("nyaa~~")
            }),

            SuffixChoice(std::vector<SuffixChoice>{
                SuffixChoice(">_<"),
                SuffixChoice(">-<")
            }),

            SuffixChoice(":flushed:"),

            SuffixChoice(std::vector<SuffixChoice>{
                SuffixChoice("^^"),
                SuffixChoice("^^;;")
            }),

            SuffixChoice(std::vector<SuffixChoice>{
                SuffixChoice("w"),
                SuffixChoice("ww")
            }),

            SuffixChoice(",")
        });

    /*
        Replace every occurrence of `from` with `to`.

        If the ENTIRE string equals `from`, don't replace it.

        This copies the weird behavior from the original Boost code,
        where simple replacements weren't applied if the regex matched
        the entire word.
    */
    static void replaceSubstring(
        std::string& text,
        const std::string& from,
        const std::string& to,
        bool skipWholeWord = true
    ) {
        if (from.empty())
            return;

        if (skipWholeWord && text == from)
            return;

        size_t position = 0;

        while (
            (position = text.find(from, position))
            != std::string::npos
        ) {
            text.replace(
                position,
                from.size(),
                to
            );

            position += to.size();
        }
    }

    /*
        Equivalent to the original:

            [lr] -> w

        But if the entire word is just "l" or "r",
        the old implementation skipped it.
    */
    static void replaceLR(std::string& text) {
        if (text == "l" || text == "r")
            return;

        for (char& c : text) {
            if (c == 'l' || c == 'r')
                c = 'w';
        }
    }

    /*
        Equivalent to:

            n(?=[aeiou]) -> ny
    */
    static void replaceNVowel(std::string& text) {
        std::string result;

        result.reserve(text.size() + 4);

        for (size_t i = 0; i < text.size(); ++i) {
            if (
                text[i] == 'n' &&
                i + 1 < text.size() &&
                isLowerVowel(text[i + 1])
            ) {
                result += "ny";
            }
            else {
                result += text[i];
            }
        }

        text = result;
    }

    /*
        Equivalent to:

            (?<!w)u(?=[ie]) -> w

        Meaning:

            u followed by i/e
            BUT not if the previous character is w
    */
    static void replaceSpecialU(std::string& text) {
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] != 'u')
                continue;

            if (i + 1 >= text.size())
                continue;

            char next = text[i + 1];

            if (next != 'i' && next != 'e')
                continue;

            bool previousIsW =
                i > 0 &&
                text[i - 1] == 'w';

            if (!previousIsW)
                text[i] = 'w';
        }
    }

    /*
        This is basically the word-replacement lambda
        from the original uwuifier.
    */
    static std::string transformWord(
        const std::string& original
    ) {
        bool caps = isCaps(original);

        std::string currentMatch =
            toLower(original);

        auto wordReplacement =
            wordReplacements.find(currentMatch);

        if (wordReplacement != wordReplacements.end()) {
            currentMatch =
                wordReplacement->second;
        }

        /*
            Same order as simpleReplacements:

                [lr]
                n(?=[aeiou])
                pow
                (?<!w)u(?=[ie])
                attempt
                config
        */

        replaceLR(currentMatch);

        replaceNVowel(currentMatch);

        replaceSubstring(
            currentMatch,
            "pow",
            "paw"
        );

        replaceSpecialU(currentMatch);

        replaceSubstring(
            currentMatch,
            "attempt",
            "attwempt"
        );

        replaceSubstring(
            currentMatch,
            "config",
            "cwonfig"
        );

        if (caps)
            return toUpper(currentMatch);

        return currentMatch;
    }

    /*
        Finds words like the original:

            (?<=\b)[a-zA-Z\']+(?=\b)

        This allows internal apostrophes:

            don't
            can't
            you're

        but doesn't consider a leading/trailing apostrophe
        part of the word.
    */
    static std::string replaceWords(
        const std::string& text
    ) {
        std::string output;

        size_t i = 0;

        while (i < text.size()) {
            if (!isLetter(text[i])) {
                output += text[i];
                ++i;
                continue;
            }

            size_t start = i;
            size_t lastLetter = i;

            ++i;

            while (
                i < text.size() &&
                (
                    isLetter(text[i]) ||
                    text[i] == '\''
                )
            ) {
                if (isLetter(text[i]))
                    lastLetter = i;

                ++i;
            }

            size_t wordLength =
                lastLetter - start + 1;

            std::string word =
                text.substr(
                    start,
                    wordLength
                );

            output += transformWord(word);

            /*
                If we scanned trailing apostrophes,
                preserve them outside the transformed word.
            */

            size_t remainderStart =
                start + wordLength;

            while (remainderStart < i) {
                output += text[remainderStart];
                ++remainderStart;
            }
        }

        return output;
    }

    /*
        Original first replacement:

            \.(?= |$)

        Change "." into "!" with a chance,
        but only if followed by a space or the end.
    */
    static std::string replacePeriods(
        const std::string& text
    ) {
        std::string output = text;

        for (size_t i = 0; i < output.size(); ++i) {
            if (output[i] != '.')
                continue;

            bool valid =
                i + 1 == output.size() ||
                output[i + 1] == ' ';

            if (
                valid &&
                getChance(
                    _settings.periodToExclamationChance
                )
            ) {
                output[i] = '!';
            }
        }

        return output;
    }

    /*
        Original duplicate-character behavior.

        IMPORTANT:

        The original code matched "," or "!"
        but ALWAYS appended commas.

        So:

            !

        could become:

            !,,,

        rather than:

            !!!!

        We're intentionally preserving that.
    */
    static std::string duplicateCharacters(
        const std::string& text
    ) {
        std::string output;

        for (char c : text) {
            output += c;

            if (c != ',' && c != '!')
                continue;

            if (
                !getChance(
                    _settings.duplicateCharactersChance
                )
            ) {
                continue;
            }

            int base =
                _settings.duplicateCharactersAmount - 1;

            std::uniform_int_distribution<int> dist(
                base,
                2 * base
            );

            int amount = dist(gen);

            for (int i = 0; i < amount; ++i)
                output += ',';
        }

        return output;
    }

    /*
        Original:

            (?:(?<= )|(?<=^))[a-zA-Z]

        Stutters the first letter if it's:

            - at the beginning of the text
            - directly after a SPACE

        Not after tabs/newlines/etc.
    */
    static std::string addStutters(
        const std::string& text
    ) {
        std::string output;

        for (size_t i = 0; i < text.size(); ++i) {
            char c = text[i];

            bool beginningOfWord =
                isLetter(c) &&
                (
                    i == 0 ||
                    text[i - 1] == ' '
                );

            if (
                beginningOfWord &&
                getChance(_settings.stutterChance)
            ) {
                output += c;
                output += '-';
            }

            output += c;
        }

        return output;
    }

    static bool isSuffixPunctuation(char c) {
        return
            c == '.' ||
            c == '!' ||
            c == '?' ||
            c == ',' ||
            c == ';' ||
            c == '-';
    }

    static std::string createSuffix() {
        std::string suffix;

        if (getChance(_settings.presuffixChance))
            suffix = presuffixes.choose();

        if (getChance(_settings.suffixChance)) {
            suffix += ' ';
            suffix += suffixes.choose();
        }

        return suffix;
    }

    /*
        Original regex:

            (?<=[.!?,;\-])(?= )|(?=$)

        Inserts suffixes:

            - after punctuation when followed by a space
            - at the end of the entire string
    */
    static std::string addSuffixes(
        const std::string& text
    ) {
        std::string output;

        for (size_t i = 0; i < text.size(); ++i) {
            output += text[i];

            bool punctuationBeforeSpace =
                isSuffixPunctuation(text[i]) &&
                i + 1 < text.size() &&
                text[i + 1] == ' ';

            if (punctuationBeforeSpace)
                output += createSuffix();
        }

        /*
            (?=$) always matches the end of the string.
        */
        output += createSuffix();

        return output;
    }

    void seedFrom(const void* ptr) {
        gen.seed(
            rd +
            static_cast<std::random_device::result_type>(
                reinterpret_cast<std::uintptr_t>(ptr)
            )
        );
    }

    /*
        Used by the autocorrect.

        Only transforms ONE word.
        No random suffixes/stutters/etc.
    */
    std::string uwuifyWord(std::string text) {
        if (isNullOrWhiteSpace(text))
            return text;

        return transformWord(text);
    }

    /*
        Full original uwuifier behavior.

        Replacement order is preserved:

            1. periods -> !
            2. duplicate punctuation
            3. word replacements
            4. stuttering
            5. suffixes
    */
    std::string uwuify(std::string text) {
        if (isNullOrWhiteSpace(text))
            return text;

        text = replacePeriods(text);

        text = duplicateCharacters(text);

        text = replaceWords(text);

        text = addStutters(text);

        text = addSuffixes(text);

        return text;
    }
}