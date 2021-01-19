#ifndef JCPP_HPP
# define JCPP_HPP
# pragma once

#include <cstdlib>

#include <cstring>

#include <charconv>

#include <ostream>

#include <string>

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
template <typename, typename = std::void_t<>>
constexpr auto is_complete_v{false};

template <typename T>
constexpr auto is_complete_v<T, std::void_t<decltype(sizeof(T))>>{true};

//
template <typename T, typename = std::void_t<>>
constexpr auto is_container_v{false};

template <typename T>
constexpr auto is_container_v<T, std::void_t<
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
>{true};

//
template <typename T, typename = std::void_t<>>
constexpr auto is_associative_container_v{false};

template <typename T>
constexpr auto is_associative_container_v<T, std::void_t<
    std::enable_if_t<is_complete_v<typename T::key_compare>>,
    std::enable_if_t<is_complete_v<typename T::key_type>>,
    std::enable_if_t<is_complete_v<typename T::mapped_type>>
  >
> = is_container_v<T>;

//
template <typename T>
constexpr auto is_sequence_container_v = is_container_v<T> &&
  !is_associative_container_v<T>;

//
template <typename, typename = std::void_t<>>
constexpr auto has_array_subscript_v{false};

template <typename T>
constexpr auto has_array_subscript_v<T, std::void_t<
    decltype(std::declval<T>().operator[](0))
  >
>{true};

//
template <typename, typename = std::void_t<>>
constexpr auto has_emplace_back_v{false};

template <typename T>
constexpr auto has_emplace_back_v<T, std::void_t<
    decltype(std::declval<T>().emplace_back(
      std::declval<typename T::value_type>())
    )
  >
>{true};

//
template <typename>
constexpr auto is_managed_array_v{false};

template <typename T>
constexpr auto is_managed_array_v<std::shared_ptr<T[]>>{true};

template <typename T>
constexpr auto is_managed_array_v<std::unique_ptr<T[]>>{true};

template <typename T, std::size_t N>
constexpr auto is_managed_array_v<std::shared_ptr<T[N]>>{true};

template <typename T, std::size_t N>
constexpr auto is_managed_array_v<std::unique_ptr<T[N]>>{true};

//
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

  template <std::size_t N>
  explicit js0n(char const (&s)[N]) noexcept :
    s_(s, N)
  {
  }

  template <typename ...U,
    std::enable_if_t<
      !std::is_same_v<js0n, std::decay_t<front_t<U...>>>,
      int
    > = 0,
    std::enable_if_t<
      std::is_constructible_v<std::string_view, U...>,
      int
    > = 0
  >
  js0n(U&& ...u) noexcept(noexcept(decltype(s_)(std::forward<U>(u)...))) :
    s_(std::forward<U>(u)...)
  {
  }

  js0n& operator=(js0n const&) = default;
  js0n& operator=(js0n&&) = default;

  template <typename U>
  js0n& operator=(U&& u) noexcept(noexcept(s_ = std::forward<U>(u)))
  {
    return s_ = std::forward<U>(u), *this;
  }

  template <std::size_t N>
  js0n operator[](char const (&k)[N]) const noexcept
  {
    std::size_t vlen;
    return {::js0n(k, N - 1, s_.data(), s_.size(), &vlen), vlen};
  }

  js0n operator[](std::string_view const& k) const noexcept
  {
    std::size_t vlen;
    return {::js0n(k.data(), k.size(), s_.data(), s_.size(), &vlen), vlen};
  }

  js0n operator[](std::size_t const i) const noexcept
  {
    std::size_t vlen;
    return {::js0n(nullptr, i, s_.data(), s_.size(), &vlen), vlen};
  }

  //
  bool is_array() const noexcept
  {
    return is_valid() && (*this)[0].is_valid();
  }

  bool is_valid() const noexcept
  {
    return s_.data();
  }

  auto& view() const noexcept
  {
    return s_;
  }

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
};

namespace dec
{

struct tag {};

template <typename A>
inline auto decode(js0n const& j, A&& a) -> decltype(
  bool(decode(j, std::forward<A>(a), tag{})))
{
  return decode(j, std::forward<A>(a), tag{});
}

inline bool decode(js0n const& j, std::ostream& a) noexcept
{
  return (j.is_valid() ? a << j.view() : a << nullptr), false;
}

// anything can be converted into a string
inline bool decode(js0n const& j, std::string& a)
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
    if (auto& v(j.view()); (4 == v.size()) &&
      !std::strncmp(v.data(), "true", 4))
    {
      return !(a = true);
    }
    else if ((5 == v.size()) && !std::strncmp(v.data(), "false", 5))
    {
      return a = false;
    }
  }

  return true;
}

template <typename A,
  std::enable_if_t<
    std::is_floating_point_v<std::decay_t<A>>,
    int
  > = 0
>
inline bool decode(js0n const& j, A&& a) noexcept
{
  if (j.is_valid())
  {
    char* ptr;
    auto const d(j.view().data());

    if constexpr (std::is_same_v<std::decay_t<A>, float>)
    {
      a = std::strtof(d, &ptr);
    }
    else if constexpr (std::is_same_v<std::decay_t<A>, double>)
    {
      a = std::strtod(d, &ptr);
    }
    else if constexpr (std::is_same_v<std::decay_t<A>, long double>)
    {
      a = std::strtold(d, &ptr);
    }

    return ptr == d;
  }

  return true;
}

template <typename A,
  std::enable_if_t<
    std::is_arithmetic_v<std::decay_t<A>> &&
    !std::is_floating_point_v<std::decay_t<A>> &&
    !std::is_same_v<std::decay_t<A>, bool> &&
    !std::is_same_v<std::decay_t<A>, char> &&
    !std::is_same_v<std::decay_t<A>, signed char> &&
    !std::is_same_v<std::decay_t<A>, unsigned char>,
    int
  > = 0
>
bool decode(js0n const& j, A&& a) noexcept
{
  if (j.is_valid())
  {
    auto& view(j.view());

    auto const first(view.data());
    auto const last(first + view.size());

    return last != std::from_chars(first, last, a).ptr;
  }

  return true;
}

// specials
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
inline auto decode(js0n const& j, A&& a) -> decltype(a(tag{}), bool())
{
  return decode(j, a(tag{}));
}

// containers
template <typename A,
  std::enable_if_t<is_managed_array_v<A>, int> = 0
>
inline auto decode(js0n const& j, A& a)
{
  bool error;

  // an empty array [], will not be recognized, a js0n quirk
  if (!(error = !j.is_array()))
  {
    for (std::size_t i{}; !error; ++i)
    {
      if (auto const e(j[i]); e.is_valid())
      {
        error = decode(e, a[i]);
      }
      else
      {
        break;
      }
    }
  }

  return error;
}

template <typename A,
  std::enable_if_t<
    is_sequence_container_v<std::decay_t<A>> &&
    has_array_subscript_v<std::decay_t<A>> &&
    !has_emplace_back_v<std::decay_t<A>>,
    int
  > = 0
>
inline auto decode(js0n const& j, A&& a)
{
  bool error;

  // an empty array [], will not be recognized, a js0n quirk
  if (!(error = !j.is_array()))
  {
    auto const sz(a.size());

    for (decltype(a.size()) i{}; !error && (i != sz); ++i)
    {
      if (auto const e(j[i]); e.is_valid())
      {
        error = decode(e, a[i]);
      }
      else
      {
        break;
      }
    }
  }

  return error;
}

template <typename A,
  std::enable_if_t<
    is_sequence_container_v<std::decay_t<A>> &&
    has_emplace_back_v<std::decay_t<A>>,
    int
  > = 0
>
inline auto decode(js0n const& j, A&& a)
{
  bool error;

  if (!(error = !j.is_array()))
  {
    for (decltype(a.size()) i{}; !error; ++i)
    {
      typename std::decay_t<A>::value_type v;

      if (auto const e(j[i]); e.is_valid() && !(error = decode(e, v)))
      {
        a.emplace_back(std::move(v));
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
              if ((error = (a(i, e), ...)))
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
              if ((error = (a(e), ...)))
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
        else if constexpr (bool(sizeof...(a)))
        {
          // decode the rest of the items
          std::size_t i{};
          return error || (decode(j[i++], std::forward<A>(a)) || ...);
        }
      }

      return error;
    };
}

template <typename A, typename ...B>
auto map(std::string_view key, A&& a, B&& ...b) noexcept
{
  static_assert(!(sizeof...(B) % 2));
  return [key(std::move(key)), &a, &b...](js0n const& j)
    {
      bool error;

      if (auto const v(j[key]); !(error = !v.is_valid()))
      {
        error = decode(v, std::forward<A>(a));

        if constexpr (bool(sizeof...(b)))
        {
          return error || map(std::forward<B>(b)...)(j);
        }
      }

      return error;
    };
}

}

}

#endif // JCPP_HPP
