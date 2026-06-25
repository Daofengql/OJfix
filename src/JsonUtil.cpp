#include "JsonUtil.h"

#include "TextUtil.h"

#include <cctype>
#include <cstdio>

namespace ojfix {
namespace {

void AppendUtf8CodePoint(std::string& output, unsigned int codePoint)
{
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0x10FFFF) {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

int HexValue(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

bool ReadJsonHex4(const std::string& json, size_t offset, unsigned int& value)
{
    if (offset + 4 > json.size()) {
        return false;
    }

    value = 0;
    for (size_t index = 0; index < 4; ++index) {
        const int hex = HexValue(json[offset + index]);
        if (hex < 0) {
            return false;
        }

        value = (value << 4) | static_cast<unsigned int>(hex);
    }

    return true;
}

bool TryReadJsonStringAt(const std::string& json, size_t quoteIndex, std::string& value, std::string& error)
{
    if (quoteIndex >= json.size() || json[quoteIndex] != '"') {
        error = "JSON string did not start with a quote.";
        return false;
    }

    value.clear();
    for (size_t index = quoteIndex + 1; index < json.size(); ++index) {
        const char ch = json[index];
        if (ch == '"') {
            return true;
        }

        if (ch != '\\') {
            value.push_back(ch);
            continue;
        }

        if (++index >= json.size()) {
            error = "JSON string ended inside an escape sequence.";
            return false;
        }

        switch (json[index]) {
        case '"':
        case '\\':
        case '/':
            value.push_back(json[index]);
            break;
        case 'b':
            value.push_back('\b');
            break;
        case 'f':
            value.push_back('\f');
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        case 'u': {
            unsigned int codePoint = 0;
            if (!ReadJsonHex4(json, index + 1, codePoint)) {
                error = "Invalid JSON unicode escape.";
                return false;
            }
            index += 4;

            if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                if (index + 6 >= json.size() || json[index + 1] != '\\' || json[index + 2] != 'u') {
                    error = "JSON high surrogate was not followed by a low surrogate.";
                    return false;
                }

                unsigned int lowSurrogate = 0;
                if (!ReadJsonHex4(json, index + 3, lowSurrogate) ||
                    lowSurrogate < 0xDC00 ||
                    lowSurrogate > 0xDFFF) {
                    error = "Invalid JSON low surrogate.";
                    return false;
                }

                index += 6;
                codePoint = 0x10000 +
                    ((codePoint - 0xD800) << 10) +
                    (lowSurrogate - 0xDC00);
            }

            AppendUtf8CodePoint(value, codePoint);
            break;
        }
        default:
            error = "Unsupported JSON escape sequence.";
            return false;
        }
    }

    error = "JSON string was not closed.";
    return false;
}

} // namespace

std::string JsonEscapeUtf8(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size() + 16);

    for (unsigned char ch : text) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (ch < 0x20) {
                char buffer[7] = {};
                sprintf_s(buffer, "\\u%04x", ch);
                escaped += buffer;
            } else {
                escaped.push_back(static_cast<char>(ch));
            }
            break;
        }
    }

    return escaped;
}

std::string JsonEscapeWide(const std::wstring& text)
{
    return JsonEscapeUtf8(ToUtf8(text));
}

bool TryExtractJsonStringFieldAfter(
    const std::string& json,
    size_t startIndex,
    const char* fieldName,
    std::string& value,
    std::string& error)
{
    const std::string token = std::string("\"") + fieldName + "\"";
    const size_t fieldIndex = json.find(token, startIndex);
    if (fieldIndex == std::string::npos) {
        return false;
    }

    size_t colonIndex = json.find(':', fieldIndex + token.size());
    if (colonIndex == std::string::npos) {
        error = "Response JSON field is malformed: " + token;
        return false;
    }

    ++colonIndex;
    while (colonIndex < json.size() && isspace(static_cast<unsigned char>(json[colonIndex]))) {
        ++colonIndex;
    }

    if (colonIndex >= json.size() || json[colonIndex] != '"') {
        error = "Response JSON field is not a string: " + token;
        return false;
    }

    return TryReadJsonStringAt(json, colonIndex, value, error);
}

} // namespace ojfix
