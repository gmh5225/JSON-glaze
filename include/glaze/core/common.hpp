// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string>
#include <type_traits>
#include <tuple>
#include <utility>

#include "frozen/string.h"
#include "frozen/unordered_map.h"

#include "glaze/core/meta.hpp"
#include "glaze/util/string_view.hpp"
#include "glaze/util/variant.hpp"
#include "glaze/util/tuple.hpp"
#include "glaze/util/type_traits.hpp"

#include "NanoRange/nanorange.hpp"

namespace glz
{
   namespace detail
   {
      template <class T>
      struct Array {
         T value;
      };
      
      template <class T>
      Array(T) -> Array<T>;
      
      template <class T>
      struct Object {
         T value;
      };
      
      template <class T>
      Object(T) -> Object<T>;

      template <class T>
      struct Enum
      {
         T value;
      };

      template <class T>
      Enum(T) -> Enum<T>;
      
      template <int... I>
      using is = std::integer_sequence<int, I...>;
      template <int N>
      using make_is = std::make_integer_sequence<int, N>;

      constexpr auto size(const char *s) noexcept
      {
         int i = 0;
         while (*s != 0) {
            ++i;
            ++s;
         }
         return i;
      }

      template <const char *, typename, const char *, typename>
      struct concat_impl;

      template <const char *S1, int... I1, const char *S2, int... I2>
      struct concat_impl<S1, is<I1...>, S2, is<I2...>>
      {
         static constexpr const char value[]{S1[I1]..., S2[I2]..., 0};
      };

      template <const char *S1, const char *S2>
      constexpr auto concat_char()
      {
         return concat_impl<S1, make_is<size(S1)>, S2,
                            make_is<size(S2)>>::value;
      };

      template <size_t... Is>
      struct seq
      {};
      template <size_t N, size_t... Is>
      struct gen_seq : gen_seq<N - 1, N - 1, Is...>
      {};
      template <size_t... Is>
      struct gen_seq<0, Is...> : seq<Is...>
      {};

      template <size_t N1, size_t... I1, size_t N2, size_t... I2>
      constexpr std::array<char const, N1 + N2 - 1> concat(char const (&a1)[N1],
                                                           char const (&a2)[N2],
                                                           seq<I1...>,
                                                           seq<I2...>)
      {
         return {{a1[I1]..., a2[I2]...}};
      }

      template <size_t N1, size_t N2>
      constexpr std::array<char const, N1 + N2 - 1> concat_arrays(
         char const (&a1)[N1], char const (&a2)[N2])
      {
         return concat(a1, a2, gen_seq<N1 - 1>{}, gen_seq<N2>{});
      }

      template <size_t N>
      struct string_literal
      {
         static constexpr size_t size = (N > 0) ? (N - 1) : 0;

         constexpr string_literal() = default;


         constexpr string_literal(const char (&str)[N])
         {
            std::copy_n(str, N, value);
         }

         char value[N];
         constexpr const char *end() const noexcept { return value + size; }

         constexpr const std::string_view sv() const noexcept
         {
            return {value, size};
         }
      };

      template<size_t N>
      constexpr auto string_literal_from_view(sv str)
      {
         string_literal<N + 1> sl{};
         std::copy_n(str.data(), str.size(), sl.value);
         *(sl.value + N) = '\0';
         return sl;
      }


      template <size_t N>
      constexpr size_t length(char const (&)[N]) noexcept
      {
         return N;
      }

      template <string_literal Str>
      struct chars_impl
      {
         static constexpr std::string_view value{Str.value,
                                                 length(Str.value) - 1};
      };

      template <string_literal Str>
      inline constexpr std::string_view chars = chars_impl<Str>::value;
      
      template <uint32_t Format>
      struct read {};
      
      template <uint32_t Format>
      struct write {};
   }  // namespace detail

   struct raw_json
   {
      std::string str;
      
      raw_json() = default;
      
      template <class T>
      requires (!std::same_as<std::decay_t<T>, raw_json>)
      raw_json(T&& s) : str(std::forward<T>(s)) {}
      
      raw_json(const raw_json&) = default;
      raw_json(raw_json&&) = default;
      raw_json& operator=(const raw_json&) = default;
      raw_json& operator=(raw_json&&) = default;
   };

   template <>
   struct meta<raw_json>
   {
      static constexpr std::string_view name = "raw_json";
   };

   using basic =
      std::variant<bool, char, char8_t, unsigned char, signed char, char16_t,
                   short, unsigned short, wchar_t, char32_t, float, int,
                   unsigned int, long, unsigned long, double,
                   long long, unsigned long long, std::string>;
   
   using basic_ptr = decltype(to_variant_pointer(std::declval<basic>()));

   namespace detail
   {
      template <class T>
      concept char_t = std::same_as<std::decay_t<T>, char> || std::same_as<std::decay_t<T>, char16_t> ||
         std::same_as<std::decay_t<T>, char32_t> || std::same_as<std::decay_t<T>, wchar_t>;

      template <class T>
      concept bool_t =
         std::same_as<std::decay_t<T>, bool> || std::same_as<std::decay_t<T>, std::vector<bool>::reference>;

      template <class T>
      concept int_t = std::integral<std::decay_t<T>> && !char_t<std::decay_t<T>> && !bool_t<T>;

      template <class T>
      concept num_t = std::floating_point<std::decay_t<T>> || int_t<T>;

      template <class T>
      concept glaze_t = requires
      {
         meta<std::decay_t<T>>::value;
      }
      || local_meta_t<std::decay_t<T>>;

      template <class T>
      concept complex_t = glaze_t<std::decay_t<T>>;

      template <class T>
      concept str_t = !complex_t<T> && std::convertible_to<std::decay_t<T>, std::string_view>;

      template <class T>
      concept pair_t = requires(T pair)
      {
         {
            pair.first
            } -> std::same_as<typename T::first_type &>;
         {
            pair.second
            } -> std::same_as<typename T::second_type &>;
      };

      template <class T>
      concept map_subscriptable = requires(T container)
      {
         {
            container[std::declval<typename T::key_type>()]
            } -> std::same_as<typename T::mapped_type &>;
      };

      template <class T>
      concept map_t =
         !complex_t<T> && !str_t<T> && nano::ranges::range<T> &&
         pair_t<nano::ranges::range_value_t<T>> && map_subscriptable<T>;

      template <class T>
      concept array_t = (!complex_t<T> && !str_t<T> && !map_t<T> && nano::ranges::range<T>);

      template <class T>
      concept emplace_backable = requires(T container)
      {
         {
            container.emplace_back()
            } -> std::same_as<typename T::reference>;
      };

      template <class T>
      concept resizeable = requires(T container)
      {
         container.resize(0);
      };
      
      template <class T>
      concept is_span = requires(T t)
      {
         T::extent;
         typename T::element_type;
      };
      
      template <class T>
      concept is_dynamic_span = T::extent == static_cast<size_t>(-1);

      template <class T>
      concept has_static_size = (is_span<T> && !is_dynamic_span<T>) || (requires(T container) {
         {
            std::bool_constant<(std::decay_t<T>{}.size(), true)>()
         } -> std::same_as<std::true_type>;
      } && std::decay_t<T>{}.size() > 0);
      
      template <class T>
      constexpr size_t get_size() noexcept
      {
         if constexpr (is_span<T>) {
            return T::extent;
         }
         else {
            return std::decay_t<T>{}.size();
         }
      };

      template <class T>
      concept tuple_t = requires(T t)
      {
         std::tuple_size<T>::value;
         std::get<0>(t);
      }
      &&!complex_t<T> && !nano::ranges::range<T>;

      template <class T>
      concept nullable_t = !complex_t<T> && !str_t<T> && requires(T t)
      {
         bool(t);
         {*t};
      };

      template <class T>
      concept func_t = requires(T t)
      {
         typename T::result_type;
         std::function(t);
      } && !glaze_t<T>;

      template <class... T>
      constexpr bool all_member_ptr(std::tuple<T...>)
      {
         return std::conjunction_v<std::is_member_pointer<std::decay_t<T>>...>;
      }

      template <class T>
      concept glaze_array_t = glaze_t<T> && is_specialization_v<meta_wrapper_t<T>, Array>;

      template <class T>
      concept glaze_object_t = glaze_t<T> && is_specialization_v<meta_wrapper_t<T>, Object>;

      template <class T>
      concept glaze_enum_t = glaze_t<T> && is_specialization_v<meta_wrapper_t<T>, Enum>;

      template <class From, class To>
      concept non_narrowing_convertable = requires(From from, To to)
      {
         To{from};
      };
      
      template <class T>
      concept stream_t = requires(T t) {
         typename T::char_type;
         typename T::traits_type;
         typename T::int_type;
         t.get();
         t.peek();
         t.unget();
         t.gcount();
      };

      // from
      // https://stackoverflow.com/questions/55941964/how-to-filter-duplicate-types-from-tuple-c
      template <class T, class... Ts>
      struct unique
      {
         using type = T;
      };

      template <template <class...> class T, class... Ts, class U, class... Us>
      struct unique<T<Ts...>, U, Us...>
         : std::conditional_t<(std::is_same_v<U, Ts> || ...),
                              unique<T<Ts...>, Us...>,
                              unique<T<Ts..., U>, Us...>>
      {};

      template <class T>
      struct tuple_variant;

      template <class... Ts>
      struct tuple_variant<std::tuple<Ts...>> : unique<std::variant<>, Ts...>
      {};

      template <class T>
      struct tuple_ptr_variant;

      template <class... Ts>
      struct tuple_ptr_variant<std::tuple<Ts...>>
         : unique<std::variant<>, std::add_pointer_t<Ts>...>
      {};

      template <class Tuple,
                class = std::make_index_sequence<std::tuple_size<Tuple>::value>>
      struct value_tuple_variant;

      template <class Tuple, size_t... I>
      struct value_tuple_variant<Tuple, std::index_sequence<I...>>
      {
         using type = typename tuple_variant<decltype(std::tuple_cat(
            std::declval<std::tuple<std::tuple_element_t<
               1, std::tuple_element_t<I, Tuple>>>>()...))>::type;
      };

      template <class Tuple>
      using value_tuple_variant_t = typename value_tuple_variant<Tuple>::type;

      template <class T, size_t... I>
      inline constexpr auto make_array_impl(std::index_sequence<I...>)
      {
         using value_t = typename tuple_variant<meta_t<T>>::type;
         return std::array<value_t, std::tuple_size_v<meta_t<T>>>{
            std::get<I>(meta_v<T>)...};
      }

      template <class T>
      inline constexpr auto make_array()
      {
         constexpr auto indices =
            std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_array_impl<T>(indices);
      }

      template <class Tuple, std::size_t... Is>
      inline constexpr auto tuple_runtime_getter(std::index_sequence<Is...>)
      {
         using value_t = typename tuple_ptr_variant<Tuple>::type;
         using tuple_ref = std::add_lvalue_reference_t<Tuple>;
         using getter_t = value_t (*)(tuple_ref);
         return std::array<getter_t, std::tuple_size_v<Tuple>>{
            +[](tuple_ref tuple) -> value_t {
               return &std::get<Is>(tuple);
            }...};
      }

      template <class Tuple>
      inline auto get_runtime(Tuple &&tuple, const size_t index)
      {
         using T = std::decay_t<Tuple>;
         static constexpr auto indices = std::make_index_sequence<std::tuple_size_v<T>>{};
         static constexpr auto runtime_getter = tuple_runtime_getter<T>(indices);
         return runtime_getter[index](tuple);
      }

      template <class M>
      inline constexpr void check_member()
      {
         static_assert(std::tuple_size_v<M> == 0 || std::tuple_size_v<M> > 1,
                       "members need at least a name and a member pointer");
         static_assert(
            std::tuple_size_v<M> < 4,
            "only member_ptr, name, and comment are supported at the momment");
         if constexpr (std::tuple_size_v < M >> 0)
            static_assert(str_t<std::tuple_element_t<0, M>>,
                          "first element should be the name");
         if constexpr (std::tuple_size_v < M >> 1)
            static_assert(std::is_member_pointer_v<std::tuple_element_t<1, M>>,
                          "second element should be the member pointer");
         if constexpr (std::tuple_size_v < M >> 2)
            static_assert(str_t<std::tuple_element_t<2, M>>,
                          "third element should be a string comment");
      };

      template <class T, size_t... I>
      constexpr auto make_map_impl(std::index_sequence<I...>)
      {
         using value_t = value_tuple_variant_t<meta_t<T>>;
         return frozen::make_unordered_map<frozen::string, value_t,
                                           std::tuple_size_v<meta_t<T>>>(
            {std::make_pair<frozen::string, value_t>(
               frozen::string(std::get<0>(std::get<I>(meta_v<T>))),
               std::get<1>(std::get<I>(meta_v<T>)))...});
      }

      template <class T>
      constexpr auto make_map()
      {
         constexpr auto indices =
            std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_map_impl<std::decay_t<T>>(indices);
      }
      
      template <class T, size_t... I>
      constexpr auto make_int_map_impl(std::index_sequence<I...>)
      {
         using value_t = value_tuple_variant_t<meta_t<T>>;
         return frozen::make_unordered_map<size_t, value_t,
                                           std::tuple_size_v<meta_t<T>>>(
            {std::make_pair<size_t, value_t>(
               I,
               std::get<1>(std::get<I>(meta_v<T>)))...});
      }
      
      template <class T>
      constexpr auto make_int_map()
      {
         constexpr auto indices =
            std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_int_map_impl<T>(indices);
      }
      
      template <class T, size_t... I>
      constexpr auto make_key_int_map_impl(std::index_sequence<I...>)
      {
         return frozen::make_unordered_map<frozen::string, size_t,
                                           std::tuple_size_v<meta_t<T>>>(
            {std::make_pair<frozen::string, size_t>(
               frozen::string(std::get<0>(std::get<I>(meta_v<T>))),
                                                    I)...});
      }
      
      template <class T>
      constexpr auto make_key_int_map()
      {
         constexpr auto indices =
            std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_key_int_map_impl<T>(indices);
      }

      template <class T, size_t... I>
      constexpr auto make_enum_to_string_map_impl(std::index_sequence<I...>)
      {
         using key_t = std::underlying_type_t<T>;
         return frozen::make_unordered_map<key_t, frozen::string,
                                           std::tuple_size_v<meta_t<T>>>(
            {std::make_pair<key_t, frozen::string>(
               static_cast<key_t>(std::get<1>(std::get<I>(meta_v<T>))),
               frozen::string(std::get<0>(std::get<I>(meta_v<T>))))...});
      }

      template <class T>
      constexpr auto make_enum_to_string_map()
      {
         constexpr auto indices =
            std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_enum_to_string_map_impl<T>(indices);
      }

      template <class T, size_t... I>
      constexpr auto make_string_to_enum_map_impl(std::index_sequence<I...>)
      {
         return frozen::make_unordered_map<frozen::string, T,
                                           std::tuple_size_v<meta_t<T>>>(
            {std::make_pair<frozen::string, T>(
               frozen::string(std::get<0>(std::get<I>(meta_v<T>))),
               T(std::get<1>(std::get<I>(meta_v<T>))))...});
      }

      template <class T>
      constexpr auto make_string_to_enum_map()
      {
         constexpr auto indices =
            std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_string_to_enum_map_impl<T>(indices);
      }

      template <class T, class mptr_t>
      constexpr auto member_check()
      {
         using mptr_type = std::decay_t<mptr_t>;
         if constexpr (std::is_member_pointer_v<mptr_type>) {
            return std::declval<T>().*std::declval<mptr_type>();
         }
         else if constexpr (std::is_enum_v<std::decay_t<T>>) {
            return std::declval<T>();
         }
         else { // is a lambda function
            return mptr_type{}(std::declval<T>());
         }
      }

      template <class T,
                class = std::make_index_sequence<std::tuple_size<meta_t<T>>::value>>
      struct members_from_meta;

      template <class T, size_t... I>
      inline constexpr auto members_from_meta_impl() {
         if constexpr (std::is_enum_v<std::decay_t<T>>) {
            return std::tuple{};
         }
         else {
            return std::tuple<
               std::decay_t<decltype(member_check<T, std::tuple_element_t<
                              1, std::tuple_element_t<I, meta_t<T>>>>())>...>{};
         }
      }

      template <class T, size_t... I>
      struct members_from_meta<T, std::index_sequence<I...>>
      {
         using type = decltype(members_from_meta_impl<T, I...>());
      };

      template <class T>
      using member_tuple_t = typename members_from_meta<T>::type;
   }  // namespace detail

   constexpr auto array(auto&&... args)
   {
      return detail::Array{ std::make_tuple(args...) };
   }

   constexpr auto object(auto&&... args)
   {
      if constexpr (sizeof...(args) == 0) {
         return detail::Object{ std::make_tuple() };
      }
      else {
         return detail::Object{ group_builder<std::decay_t<decltype(std::make_tuple(args...))>>::op(std::make_tuple(args...)) };
      }
   }

   constexpr auto enumerate(auto &&...args)
   {
      return detail::Enum{
         group_builder<std::decay_t<decltype(std::make_tuple(args...))>>::op(
            std::make_tuple(args...))};
   }
}  // namespace glaze
