#ifndef JS0NCPP_HPP
# define JS0NCPP_HPP
# pragma once

#include <cstring>

#include <ostream>

#include <sstream>

#include <string_view>

#include <type_traits>

#include <utility>

#include "js0n/src/js0n.h"

namespace jcpp
{

template <typename A, typename ...B>
struct front
{
  using type = A;
};

template <typename ...A>
using front_t = typename front<A...>::type;

//
template <typename U>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<U>>;

//
template <typename, typename = std::void_t<>>
constexpr bool is_complete_v = false;

template <typename T>
constexpr bool is_complete_v<T, std::void_t<decltype(sizeof(T))>> = true;

//
template <typename T, typename = std::void_t<>>
constexpr bool is_container_v = false;

template <typename T>
constexpr bool is_container_v<T, std::void_t<
    std::enable_if_t<is_complete_v<typename T::const_iterator>>,
    std::enable_if_t<is_complete_v<typename T::iterator>>,
    std::enable_if_t<is_complete_v<typename T::size_type>>,
    std::enable_if_t<is_complete_v<typename T::value_type>>,

    decltype(std::declval<T>().cbegin()),
    decltype(std::declval<T>().cend()),
    decltype(std::declval<T>().begin()),
    decltype(std::declval<T>().end()),
    decltype(std::declval<T>().size())
  >
> = true;

template <typename T, typename = std::void_t<>>
constexpr bool is_associative_container_v = false;

template <typename T>
constexpr bool is_associative_container_v<T, std::void_t<
    std::enable_if_t<is_complete_v<typename T::key_compare>>,
    std::enable_if_t<is_complete_v<typename T::key_type>>,
    std::enable_if_t<is_complete_v<typename T::mapped_type>>
  >
> = is_container_v<T>;

template <typename T, typename = std::void_t<>>
constexpr bool is_sequence_container_v = is_container_v<T>;

template <typename T>
constexpr bool is_sequence_container_v<T, std::void_t<
    std::enable_if_t<is_complete_v<typename T::key_compare>>,
    std::enable_if_t<is_complete_v<typename T::key_type>>,
    std::enable_if_t<is_complete_v<typename T::mapped_type>>
  >
> = false;

class js0n
{
  std::string_view s_;

  // careful! We need to move beyond s_.data(), hence we never call these
  // functions, but calling is_array() is ok.
  bool is_map() const noexcept
  {
    return is_valid() && !is_string() && ('{' == *s_.data());
  }

  bool is_string() const noexcept
  {
    return '"' == *(s_.data() - 1);
  }

public:
  js0n() = default;

  js0n(js0n const&) = default;
  js0n(js0n&&) = default;

  template <typename T, std::size_t N,
    typename = std::enable_if_t<std::is_same_v<char, std::decay_t<T>>>
  >
  explicit js0n(T(&s)[N]) noexcept(noexcept(decltype(s_)(s, N))) :
    s_(s, N)
  {
  }

  template <typename ...U,
    typename = std::enable_if_t<
      !std::is_same_v<js0n, std::decay_t<front_t<U...>>>
    >
  >
  explicit js0n(U&& ...u) noexcept(noexcept(
    decltype(s_)(std::forward<U>(u)...))) :
    s_(std::forward<U>(u)...)
  {
  }

  js0n& operator=(js0n const&) = default;
  js0n& operator=(js0n&&) = default;

  template <typename U>
  js0n& operator=(U&& u) noexcept(noexcept(s_ = std::forward<U>(u)))
  {
    s_ = std::forward<U>(u);

    return *this;
  }

  template <typename T, std::size_t N>
  auto operator[](T (&k)[N]) const noexcept ->
    std::enable_if_t<std::is_same<char, std::decay_t<T>>{}, js0n>
  {
    std::size_t vlen;

    auto const v(::js0n(k, N - 1, s_.data(), s_.size(), &vlen));

    return js0n(v, std::size_t(-1) == vlen ? 0 : vlen);
  }

  auto operator[](std::string_view const& k) const noexcept
  {
    std::size_t vlen;

    auto const v(::js0n(k.data(), k.size(), s_.data(), s_.size(), &vlen));

    return js0n(v, std::size_t(-1) == vlen ? 0 : vlen);
  }

  auto operator[](std::size_t const i) const noexcept
  {
    std::size_t vlen;

    auto const v(::js0n(nullptr, i, s_.data(), s_.size(), &vlen));

    return js0n(v, std::size_t(-1) == vlen ? 0 : vlen);
  }

  template <typename ...U>
  void assign(U&& ...u) noexcept(noexcept(
    s_ = std::string_view(std::forward<U>(u)...)))
  {
    s_ = std::string_view(std::forward<U>(u)...);
  }

  bool is_array() const noexcept
  {
    return is_valid() && (*this)[0].is_valid();
  }

  bool is_valid() const noexcept
  {
    return s_.data() && s_.size();
  }

/*
  template <typename A,
    std::enable_if_t<
      std::is_arithmetic<T>{} &&
      !std::is_pointer<T>{} &&
      !std::is_reference<T>{} &&
      !std::is_same<T, bool>{} &&
      !std::is_same<T, char>{} &&
      !std::is_same<T, signed char>{} &&
      !std::is_same<T, unsigned char>{},
      int
    > = 0
  >
  bool get(A&& a) const noexcept
  {
    if (is_valid())
    {
      auto const first(s_.data());
      auto const last(first + s_.size());

      if (T v; last == std::from_chars(first, last, r).ptr)
      {
        a = v
        return false;
      }
    }

    return true;
  }
*/

  // size of an array, does not support objects
  std::size_t size() const noexcept
  {
    std::size_t i{};

    if (is_array())
    {
      for (;; ++i)
      {
        if (auto const e(operator[](i)); !e.is_valid())
        {
          break;
        }
      }
    }

    return i;
  }

  auto& view() const noexcept
  {
    return s_;
  }
};

namespace dec
{

// anything can be turned into a string
inline bool decode(js0n const& j, std::string& a) noexcept
{
  return j.is_valid() ? a = j.view(), false : true;
}

inline bool decode(js0n const& j, std::string_view& a) noexcept
{
  return j.is_valid() ? a = j.view(), false : true;
}

// bool
template <typename A,
  std::enable_if_t<
    std::is_same_v<std::decay_t<A>, bool>,
    int
  > = 0
>
inline bool decode(js0n const& j, A&& a) noexcept
{
  if (j.is_valid())
  {
    if (auto const data(j.view().data()); !std::strncmp(data, "true", 4))
    {
      return !(a = true);
    }
    else if (!std::strncmp(data, "false", 5))
    {
      return a = false;
    }
  }

  return true;
}

template <typename A,
  std::enable_if_t<
    std::is_arithmetic_v<std::decay_t<A>> &&
    !std::is_same_v<std::decay_t<A>, bool> &&
    !std::is_same_v<std::decay_t<A>, char> &&
    !std::is_same_v<std::decay_t<A>, signed char> &&
    !std::is_same_v<std::decay_t<A>, unsigned char>,
    int
  > = 0
>
inline bool decode(js0n const& j, A&& a) noexcept
{
  if (j.is_valid())
  {
    std::stringstream ss(std::string{j.view()});

    if (ss >> a)
    {
      return false;
    }
  }

  return true;
}

// specials
template <typename A,
  std::enable_if_t<
    std::is_same_v<std::decay_t<A>, std::ostream>,
    int
  > = 0
>
inline bool decode(js0n const& j, A&& a)
{
  if (j.is_valid())
  {
    std::string_view v;

    return decode(j, v) ? true : (a << v, false);
  }
  else
  {
    return true;
  }
}

template <typename A,
  std::enable_if_t<
    std::is_invocable_r_v<bool, A, js0n const&>,
    int
  > = 0
>
inline auto decode(js0n const& j, A&& a)
{
  return a(j);
}

template <typename A>
inline auto decode(js0n const& j, A&& a) -> decltype(a.from_js0n(), bool())
{
  return decode(j, a.from_js0n());
}

// containers
template <typename A,
  std::enable_if_t<
    is_sequence_container_v<std::decay_t<A>>,
    int
  > = 0
>
inline auto decode(js0n const& j, A&& a) -> decltype(a.clear(),
  a.push_back(std::declval<typename std::decay_t<A>::value_type>()), bool())
{
  bool error;

  if (!(error = !j.is_array()))
  {
    a.clear();

    for (std::size_t i{}; !error; ++i)
    {
      if (auto const e(j[i]); e.is_valid())
      {
        if (typename std::decay_t<A>::value_type v; !(error = decode(e, v)))
        {
          a.push_back(std::move(v));
        }
      }
      else
      {
        break;
      }
    }
  }

  return error;
}

template <typename ...A>
auto array(A&& ...a) noexcept
{
  return [&a...](js0n const& j)
    {
      bool error;

      if (!(error = !j.is_array()))
      {
        if constexpr ((1 == sizeof...(A)) && std::is_invocable_r_v<
          bool, front_t<A...>, std::size_t, decltype(j)>)
        {
          for (std::size_t i{};; ++i)
          {
            if (auto const e(j[i]); e.is_valid())
            {
              if (error = (a(i, e), ...))
              {
                break;
              }
            }
            else
            {
              break;
            }
          }
        }
        else if constexpr ((1 == sizeof...(A)) && std::is_invocable_v<
          front_t<A...>, std::size_t, decltype(j)>)
        {
          for (std::size_t i{};; ++i)
          {
            if (auto const e(j[i]); e.is_valid())
            {
              (a(i, e), ...);
            }
            else
            {
              break;
            }
          }
        }
        else if constexpr ((1 == sizeof...(A)) &&
          std::is_invocable_r_v<bool, front_t<A...>, decltype(j)>)
        {
          for (std::size_t i{};; ++i)
          {
            if (auto const e(j[i]); e.is_valid())
            {
              if (error = (a(e), ...))
              {
                break;
              }
            }
            else
            {
              break;
            }
          }
        }
        else if constexpr ((1 == sizeof...(A)) &&
          std::is_invocable_v<front_t<A...>, decltype(j)>)
        {
          for (std::size_t i{};; ++i)
          {
            if (auto const e(j[i]); e.is_valid())
            {
              (a(e), ...);
            }
            else
            {
              break;
            }
          }
        }
        else
        {
          if (auto const sz(j.size()); !(error = (sz != sizeof...(A))))
          {
            // decode the rest of the items
            std::size_t i{};
            error = error || (decode(j[i++], std::forward<A>(a)) || ...);
          }
        }
      }

      return error;
    };
}

template <typename A, typename ...B>
auto map(std::string_view const& key, A&& a, B&& ...b) noexcept
{
  static_assert(!(sizeof...(B) % 2));
  return [key, &a, &b...](js0n const& j)
    {
      bool error;

      if (auto const v(j[key]); !(error = !v.is_valid()))
      {
        error = decode(v, std::forward<A>(a));
      }

      if constexpr (bool(sizeof...(b)))
      {
        return error || map(std::forward<B>(b)...)(j);
      }
      else
      {
        return error;
      }
    };
}

}

}

#endif // JS0NCPP_HPP
