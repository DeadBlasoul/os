#pragma once

#include <cctype>
#include <vector>
#include <string_view>


namespace utility::string
{
    /// Splits string to words by default separator (spaces)
    /**
     * @param string: string that will be split
     * @return: vector of words
    */
    auto inline split_to_words(std::string_view string) noexcept(false) -> std::vector<std::string_view> {
        using iterator = std::string_view::iterator;

        auto       argv = std::vector<std::string_view>{};
        auto       it = string.begin();
        auto const end = string.end();

        auto skip_whitespace = [](iterator& it, iterator const end) {
            while (it < end && std::isspace(*it))
                ++it;
        };
        auto skip_word = [](iterator& it, iterator const end) {
            while (it < end && !std::isspace(*it))
                ++it;
        };

        while (it != end) {
            skip_whitespace(it, end);
            if (it == end) {
                break;
            }

            /*
             *  Okay, we found a word. Now we need to determine it's boundaries in next terms:
             *      [0][1][2]...[N] <-- word with N letters
             *       ^           ^
             *       |            \
             *       |             ----- last
             *     first
             *
             *  Now we know first and last points so we can compute offset in the string and size of word.
             */
            auto const first = it;
            skip_word(it, end);
            auto const last = it;
            auto const offset = static_cast<std::size_t>(first - string.begin());
            auto const size = static_cast<std::size_t>(last - first);
            argv.push_back(string.substr(offset, size));
        }

        return argv;
    }
}
