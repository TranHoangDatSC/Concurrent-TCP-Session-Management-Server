#pragma once

#include <iostream>
#include <mutex>
#include <string_view>

namespace TerminalUI {
    // DEFINE COLOR (CTSMS Palette)
    namespace Color {
        inline constexpr std::string_view RESET = "\033[0m";
        inline constexpr std::string_view BOLD = "\033[1m";

        inline constexpr std::string_view CRITICAL = "\033[38;2;255;59;59m";   // Red · #FF3B3B
        inline constexpr std::string_view SUCCESS = "\033[38;2;0;230;118m";   // Green · #00E676
        inline constexpr std::string_view SYSTEM = "\033[38;2;0;217;255m";   // Cyan · #00D9FF
        inline constexpr std::string_view WARNING = "\033[38;2;255;179;0m";    // Amber · #FFB300
        inline constexpr std::string_view SESSION = "\033[38;2;179;136;255m";  // Purple · #B388FF
        inline constexpr std::string_view TIMESTAMP = "\033[38;2;160;160;160m";  // Gray · #A0A0A0
    } // namespace Color

    namespace detail {
        inline std::mutex& console_mutex() {
            static std::mutex mtx;
            return mtx;
        }

        inline void print_tagged(std::string_view color,
            std::string_view tag,
            std::string_view message) {
            std::lock_guard<std::mutex> lock(console_mutex());
            std::cout << Color::BOLD << color
                << "[" << tag << "]"
                << Color::RESET << " "
                << message << "\n";
        }
    } // namespace detail

    inline void print_success(std::string_view message) {
        detail::print_tagged(Color::SUCCESS, "SUCCESS", message);
    }

    inline void print_error(std::string_view message) {
        detail::print_tagged(Color::CRITICAL, "CRITICAL", message);
    }

    inline void print_warning(std::string_view message) {
        detail::print_tagged(Color::WARNING, "WARNING", message);
    }

    inline void print_system(std::string_view message) {
        detail::print_tagged(Color::SYSTEM, "SYSTEM", message);
    }

    inline void print_session(std::string_view message) {
        detail::print_tagged(Color::SESSION, "SESSION", message);
    }

} // namespace TerminalUI
