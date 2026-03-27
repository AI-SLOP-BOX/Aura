#pragma once

#include <string>
#include <variant>
#include <stdexcept>

namespace Aura::Core::Utils {

/**
 * @struct Error
 * @brief Formal Error descriptor for the 2026 Engine.
 */
struct Error {
    std::string message;
    int code;
};

/**
 * @template Result<T>
 * @brief Modern Expected-style Result Type (C++23 inspired).
 * HONEST FIX: Replaces the 'Silent Failure (void)' with explicit Error propagation.
 * Force the caller to handle failures (IO, Allocation, Invalid State).
 */
template <typename T>
class Result {
public:
    Result(T val) : m_data(std::move(val)) {}
    Result(Error err) : m_data(std::move(err)) {}

    bool isOk() const { return std::holds_alternative<T>(m_data); }
    bool isError() const { return std::holds_alternative<Error>(m_data); }

    T& get() { 
        if (!isOk()) throw std::runtime_error("Attempted to get() an Error Result");
        return std::get<T>(m_data); 
    }

    const Error& getError() const { return std::get<Error>(m_data); }

private:
    std::variant<T, Error> m_data;
};

} // namespace Aura::Core::Utils
