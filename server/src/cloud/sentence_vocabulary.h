#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SentenceVocabulary
{
struct Entry
{
    std::string word, level, phonetic, pos, gloss;
};
class Dictionary
{
  public:
    explicit Dictionary(const std::filesystem::path &path)
    {
        try
        {
            std::ifstream input(path);
            if (!input)
                return;
            const auto data = nlohmann::json::parse(input);
            if (data.value("schema", 0) != 1)
                return;
            for (const auto &row : data.at("entries"))
            {
                Entry entry{row.at("word"), row.at("level"), row.value("phonetic", ""), row.value("pos", ""),
                            row.at("gloss")};
                if (entry.level != "CET4" && entry.level != "CET6")
                    continue;
                entries_[entry.word] = entry;
                for (const auto &form : row.value("forms", std::vector<std::string>{}))
                    forms_.emplace(form, entry.word);
            }
        }
        catch (...)
        {
            entries_.clear();
            forms_.clear();
        }
    }

    std::vector<Entry> Select(const std::string &sentence) const
    {
        static const std::unordered_set<std::string> stop = {
            "a",   "an",    "the",  "i",     "you",   "he",    "she",   "it",    "we",     "they", "me",
            "my",  "your",  "our",  "their", "this",  "that",  "these", "those", "be",     "am",   "is",
            "are", "was",   "were", "been",  "being", "have",  "has",   "had",   "do",     "does", "did",
            "can", "could", "will", "would", "may",   "might", "must",  "shall", "should", "to",   "of",
            "in",  "on",    "at",   "by",    "for",   "with",  "as",    "and",   "or",     "but",  "if",
            "so",  "than",  "then", "not",   "no",    "yes",   "very",  "also",  "some",   "any"};
        std::vector<Entry> selected;
        std::unordered_set<std::string> seen;
        std::string word;
        auto accept = [&] {
            if (stop.count(word) || word.size() < 3)
                return;
            auto found = entries_.find(word);
            if (found == entries_.end())
            {
                const auto form = forms_.find(word);
                if (form != forms_.end())
                    found = entries_.find(form->second);
            }
            if (found != entries_.end() && seen.insert(found->first).second && !stop.count(found->first))
                selected.push_back(found->second);
        };
        for (unsigned char ch : sentence + " ")
        {
            if (ch >= 'A' && ch <= 'Z')
                word += static_cast<char>(ch + ('a' - 'A'));
            else if (ch >= 'a' && ch <= 'z')
                word += static_cast<char>(ch);
            else
            {
                accept();
                word.clear();
            }
        }
        std::stable_sort(selected.begin(), selected.end(),
                         [](const Entry &a, const Entry &b) { return a.level == "CET6" && b.level != "CET6"; });
        if (selected.size() > 5)
            selected.resize(5);
        return selected;
    }

  private:
    std::unordered_map<std::string, Entry> entries_;
    std::unordered_map<std::string, std::string> forms_;
};

inline std::string Format(const std::vector<Entry> &entries)
{
    std::string text;
    for (const auto &entry : entries)
    {
        if (!text.empty())
            text += "\n";
        text += entry.word + "  [" + entry.level + "]";
        if (!entry.phonetic.empty())
            text += "  /" + entry.phonetic + "/";
        text += "  " + entry.pos + "  " + entry.gloss;
    }
    return text.empty() ? text : "四六级重点词 · 常见义\n" + text;
}
} // namespace SentenceVocabulary
