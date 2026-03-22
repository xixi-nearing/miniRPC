#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace mrpc {

namespace detail {

template <typename T>
using UnsignedEquivalent = std::conditional_t<
    std::is_same_v<T, bool>,
    std::uint8_t,
    std::make_unsigned_t<T>>;

template <typename T>
UnsignedEquivalent<T> ToUnsigned(T value) {
  if constexpr (std::is_same_v<T, bool>) {
    return static_cast<std::uint8_t>(value ? 1 : 0);
  } else {
    return static_cast<UnsignedEquivalent<T>>(value);
  }
}

template <typename T>
T FromUnsigned(UnsignedEquivalent<T> value) {
  if constexpr (std::is_same_v<T, bool>) {
    return value != 0;
  } else {
    return static_cast<T>(value);
  }
}

}  // namespace detail

class BufferWriter {
 public:
  const std::string& data() const { return buffer_; }
  std::string Take() { return std::move(buffer_); }

  template <typename T>
  void WriteIntegral(T value) {
    static_assert(std::is_integral_v<T> || std::is_same_v<T, bool>);
    using U = detail::UnsignedEquivalent<T>;
    U raw = detail::ToUnsigned(value);
    // 协议层统一使用大端序，便于跨语言/跨机器保持稳定的线格式。
    for (int shift = static_cast<int>(sizeof(U) - 1) * 8; shift >= 0; shift -= 8) {
      buffer_.push_back(static_cast<char>((raw >> shift) & 0xFFU));
    }
  }

  void WriteDouble(double value) {
    const auto bits = std::bit_cast<std::uint64_t>(value);
    WriteIntegral(bits);
  }

  void WriteString(std::string_view value) {
    WriteIntegral<std::uint32_t>(static_cast<std::uint32_t>(value.size()));
    buffer_.append(value.data(), value.size());
  }

 private:
  std::string buffer_;
};

class BufferReader {
 public:
  explicit BufferReader(std::string_view data) : data_(data) {}

  template <typename T>
  bool ReadIntegral(T* value) {
    static_assert(std::is_integral_v<T> || std::is_same_v<T, bool>);
    using U = detail::UnsignedEquivalent<T>;
    if (remaining() < sizeof(U)) {
      return false;
    }
    U raw = 0;
    // 与 WriteIntegral 对应，按大端序逐字节恢复整数。
    for (std::size_t i = 0; i < sizeof(U); ++i) {
      raw = static_cast<U>((raw << 8) | static_cast<unsigned char>(data_[offset_ + i]));
    }
    offset_ += sizeof(U);
    *value = detail::FromUnsigned<T>(raw);
    return true;
  }

  bool ReadDouble(double* value) {
    std::uint64_t bits = 0;
    if (!ReadIntegral(&bits)) {
      return false;
    }
    *value = std::bit_cast<double>(bits);
    return true;
  }

  bool ReadString(std::string* value) {
    std::uint32_t size = 0;
    if (!ReadIntegral(&size) || remaining() < size) {
      return false;
    }
    *value = std::string(data_.substr(offset_, size));
    offset_ += size;
    return true;
  }

  bool empty() const { return remaining() == 0; }
  std::size_t remaining() const { return data_.size() - offset_; }

 private:
  std::string_view data_;
  std::size_t offset_ = 0;
};

}  // namespace mrpc
