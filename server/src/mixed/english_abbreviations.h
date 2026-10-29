#pragma once
#include <string>
#include <utility>

namespace EnglishAbbreviations
{
inline std::string Canonical(const std::string &input)
{
    static const std::pair<const char *, const char *> entries[] = {
        {"ai", "AI"},     {"api", "API"},       {"cpu", "CPU"},   {"gpu", "GPU"},
        {"ram", "RAM"},   {"rom", "ROM"},       {"usb", "USB"},   {"url", "URL"},
        {"uri", "URI"},   {"ui", "UI"},         {"ux", "UX"},     {"os", "OS"},
        {"pdf", "PDF"},   {"png", "PNG"},       {"jpg", "JPG"},   {"html", "HTML"},
        {"css", "CSS"},   {"js", "JS"},         {"json", "JSON"}, {"xml", "XML"},
        {"sql", "SQL"},   {"tcp", "TCP"},       {"udp", "UDP"},   {"ip", "IP"},
        {"http", "HTTP"}, {"https", "HTTPS"},   {"ftp", "FTP"},   {"ssh", "SSH"},
        {"tls", "TLS"},   {"dns", "DNS"},       {"qr", "QR"},     {"id", "ID"},
        {"uid", "UID"},   {"uuid", "UUID"},     {"sdk", "SDK"},   {"ide", "IDE"},
        {"cli", "CLI"},   {"gui", "GUI"},       {"ml", "ML"},     {"nlp", "NLP"},
        {"ocr", "OCR"},   {"tts", "TTS"},       {"asr", "ASR"},   {"vpn", "VPN"},
        {"lan", "LAN"},   {"wan", "WAN"},       {"wifi", "WiFi"}, {"bluetooth", "Bluetooth"},
        {"git", "Git"},   {"github", "GitHub"}, {"gpt", "GPT"},   {"llm", "LLM"}};
    for (const auto &entry : entries)
        if (input == entry.first)
            return entry.second;
    return {};
}
} // namespace EnglishAbbreviations
