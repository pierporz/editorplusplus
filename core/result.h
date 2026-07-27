#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace ep {

struct Error {
  std::string message;
};

// Result<T>: either a T or an Error. No exceptions anywhere in core/.
template <typename T>
class Result {
 public:
  Result(T value) : m_data(std::move(value)) {}
  Result(Error error) : m_data(std::move(error)) {}

  bool IsOk() const { return std::holds_alternative<T>(m_data); }
  bool IsError() const { return std::holds_alternative<Error>(m_data); }
  explicit operator bool() const { return IsOk(); }

  const T& Value() const { return std::get<T>(m_data); }
  T& Value() { return std::get<T>(m_data); }
  const Error& Err() const { return std::get<Error>(m_data); }

 private:
  std::variant<T, Error> m_data;
};

// Result<void>: either Ok or an Error.
template <>
class Result<void> {
 public:
  Result() : m_error() {}
  Result(Error error) : m_error(std::move(error)) {}

  bool IsOk() const { return !m_error.has_value(); }
  bool IsError() const { return m_error.has_value(); }
  explicit operator bool() const { return IsOk(); }

  const Error& Err() const { return *m_error; }

 private:
  std::optional<Error> m_error;
};

inline Result<void> Ok() { return Result<void>(); }

template <typename T>
Result<T> Ok(T value) {
  return Result<T>(std::move(value));
}

template <typename T = void>
Result<T> Fail(std::string message) {
  return Result<T>(Error{std::move(message)});
}

}  // namespace ep
