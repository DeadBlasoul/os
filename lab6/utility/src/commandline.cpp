#include <utility/commandline.hpp>

#include <cctype>
#include <array>

using namespace std::literals;

namespace {
    auto split(std::string_view const string) -> std::vector<std::string_view> {
        using iterator = std::string_view::iterator;

        auto       argv = std::vector<std::string_view> {};
        auto       it   = string.begin();
        auto const end  = string.end();

        auto skip_whitespace = [](iterator& it, iterator const end) {
            while (it < end && isspace(*it))
                ++it;
        };
        auto skip_word = [](iterator& it, iterator const end) {
            while (it < end && !isspace(*it))
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
            auto const last   = it;
            auto const offset = static_cast<std::size_t>(first - string.begin());
            auto const size   = static_cast<std::size_t>(last - first);
            argv.push_back(string.substr(offset, size));
        }

        return argv;
    }
}

auto utility::commandline::command_runner::exec(std::string_view const command) noexcept(false) -> void {
    auto exec_line = [this](std::string_view const line) -> void {
        auto const argv = split(line);
        if (std::size(argv) == 0) {
            return;
        }

        auto const it = callbacks_.find(argv[0]);
        if (it == callbacks_.end()) {
            throw std::invalid_argument {"no such command '" + std::string {argv[0]} + "'"};
        }

        auto& callback = it->second;
        std::invoke(callback, argv);
    };

    auto it = command.begin();
    while (it != command.end()) {
        auto const start = it;

        while (it != command.end() && *it != '\n') {
            ++it;
        }
        auto const end    = it;
        auto const offset = static_cast<std::size_t>(start - command.begin());
        auto const size   = static_cast<std::size_t>(end - start);
        auto const line   = command.substr(offset, size);

        exec_line(line);

        // We need to skip '\n' to start looking for next line 
        if (it != command.end()) {
            ++it;
        }
    }
}
