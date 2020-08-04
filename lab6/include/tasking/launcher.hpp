#pragma once

#include <string_view>
#include <filesystem>
#include <utility>

namespace tasking {
    struct task_info;
    struct task;
    class launcher;

    struct task_info {
        std::filesystem::path path;
        std::string_view      args;
    };

    struct task {
        using size_type = std::size_t;

    private:
        static constexpr size_type size_of_handler_word = sizeof(void*);
        static constexpr size_type handler_words_count  = 2;

        std::aligned_storage_t<size_of_handler_word * handler_words_count> storage_;

        task() = default;

    public:
        task(const task&) = delete;

        task(task&& other) noexcept
            : storage_ {} {
            *this = std::move(other);
        }

        ~task() noexcept;

        task& operator=(const task&) = delete;

        task& operator=(task&& other) noexcept {
            if (&other != this) {
                other.copy_to(*this);
                other.clear();
            }
            return *this;
        }

        /// Wait until task is done
        auto wait() -> void;

        /// Stop task immediately
        auto kill() -> void;

    private:
        /// Copy storage from current task to the other's
        auto copy_to(task& other) const noexcept -> void {
            std::memcpy(&other.storage_, &this->storage_, sizeof storage_);
        }

        /// Clear task storage
        auto clear() noexcept -> void {
            std::memset(&this->storage_, 0x00, sizeof storage_);
        }

        /// Casts internal storage to the given type
        template <typename Type>
        auto as() noexcept -> decltype(auto) {
            using target_type = std::remove_reference_t<Type>;
            static_assert(sizeof(target_type) <= sizeof(storage_));
            return reinterpret_cast<target_type&>(storage_);
        }

        friend class launcher;
    };

    [[nodiscard]]
    inline auto get_launcher_for(task_info const&) noexcept -> launcher;

    class launcher {
        explicit launcher(task_info ti) noexcept
            : task_info_ {
                .path = std::move(ti.path),
                .args = std::string {ti.args}
            } { }

    public:
        [[nodiscard]]
        auto start() const noexcept(false) -> task;

    private:
        struct info {
            std::filesystem::path path;
            std::string           args;
        };

        info task_info_;

        friend auto get_launcher_for(task_info const&) noexcept -> launcher;
    };
}

[[nodiscard]]
inline auto tasking::get_launcher_for(task_info const& ti) noexcept -> launcher {
    return launcher {ti};
}
