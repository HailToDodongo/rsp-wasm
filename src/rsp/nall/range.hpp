#pragma once

template<typename T, typename U> constexpr auto min(const T& t, const U& u) -> T {
  return t < u ? t : (T)u;
}

template<typename T, typename U> constexpr auto max(const T& t, const U& u) -> T {
  return t > u ? t : (T)u;
}

template<u32 bits> inline auto uclip(u64 x) -> u64 {
  enum : u64 { b = 1ull << (bits - 1), m = b * 2 - 1 };
  return (x & m);
}

template<u32 bits> inline auto sclamp(s64 x) -> s64 {
  enum : s64 { b = 1ull << (bits - 1), m = b - 1 };
  return (x > m) ? m : (x < -b) ? -b : x;
}

template<u32 bits> inline auto sclip(s64 x) -> s64 {
  enum : u64 { b = 1ull << (bits - 1), m = b * 2 - 1 };
  return ((x & m) ^ b) - b;
}

template<typename T = s64>
struct range_t {
  struct iterator {
    iterator(T position, T step = 0) : position(position), step(step) {}
    auto operator*() const -> T { return position; }
    auto operator!=(const iterator& source) const -> bool { return step > 0 ? position < source.position : position > source.position; }
    auto operator++() -> iterator& { position += step; return *this; }

  private:
    T position;
    const T step;
  };

  struct reverse_iterator {
    reverse_iterator(T position, T step = 0) : position(position), step(step) {}
    auto operator*() const -> T { return position; }
    auto operator!=(const reverse_iterator& source) const -> bool { return step > 0 ? position > source.position : position < source.position; }
    auto operator++() -> reverse_iterator& { position -= step; return *this; }

  private:
    T position;
    const T step;
  };

  auto begin() const -> iterator { return {origin, stride}; }
  auto end() const -> iterator { return {target}; }

  auto rbegin() const -> reverse_iterator { return {target - stride, stride}; }
  auto rend() const -> reverse_iterator { return {origin - stride}; }

  T origin;
  T target;
  T stride;
};

template<typename T = s64>
inline auto range(s64 size) {
  return range_t<T>{0, size, 1};
}

template<typename T = s64>
inline auto range(s64 offset, s64 size) {
  return range_t<T>{offset, size, 1};
}

template<typename T = s64>
inline auto range(s64 offset, s64 size, s64 step) {
  return range_t<T>{offset, size, step};
}
