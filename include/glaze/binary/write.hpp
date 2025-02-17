// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/binary/header.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/core/write.hpp"
#include "glaze/json/json_ptr.hpp"

#include <utility>

namespace glz
{
   namespace detail
   {      
      template <class T = void>
      struct to_binary {};

      template <>
      struct write<binary>
      {
         template <auto& Opts, class T, class B>
         static void op(T&& value, B&& b)
         {
            to_binary<std::decay_t<T>>::template op<Opts>(
               std::forward<T>(value), std::forward<B>(b));
         }
      };
      
      template <class T>
      requires (std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> || std::same_as<T, std::vector<bool>::const_reference>)
      struct to_binary<T>
      {
         template <auto& Opts>
         static void op(const bool value, auto&& b) noexcept
         {
            if (value) {
               dump<static_cast<std::byte>(1)>(b);
            }
            else {
               dump<static_cast<std::byte>(0)>(b);
            }
         }
      };

      template <func_t T>
      struct to_binary<T>
      {
         template <auto& Opts>
         static void op(auto&& /*value*/, auto&& /*b*/) noexcept
         {}
      };
      
      void dump_type(auto&& value, auto&& b) noexcept
      {
         dump(std::as_bytes(std::span{ &value, 1 }), b);
      }
      
      void dump_int(size_t i, auto&& b)
      {
         if (i < 64) {
            dump_type(header8{ 0, static_cast<uint8_t>(i) }, b);
         }
         else if (i < 16384) {
            dump_type(header16{ 1, static_cast<uint16_t>(i) }, b);
         }
         else if (i < 1073741824) {
            dump_type(header32{ 2, static_cast<uint32_t>(i) }, b);
         }
         else if (i < 4611686018427387904) {
            dump_type(header64{ 3, i }, b);
         }
         else {
            throw std::runtime_error("size not supported");
         }
      }
      
      template <class T>
      requires num_t<T> || char_t<T> || glaze_enum_t<T>
      struct to_binary<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump_type(value, b);
         }
      };
      
      template <str_t T>
      struct to_binary<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump_int(value.size(), b);
            dump(std::as_bytes(std::span{ value.data(), value.size() }), b);
         }
      };
      
      template <array_t T>
      struct to_binary<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b)
         {
            if constexpr (!has_static_size<T>) {
               dump_int(value.size(), b);
            }
            for (auto&& x : value) {
               write<binary>::op<Opts>(x, b);
            }
         }
      };
      
      template <map_t T>
      struct to_binary<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump_int(value.size(), b);
            for (auto&& [k, v] : value) {
               write<binary>::op<Opts>(k, b);
               write<binary>::op<Opts>(v, b);
            }
         }
      };
      
      template <nullable_t T>
      struct to_binary<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            if (value) {
               dump<static_cast<std::byte>(1)>(b);
               write<binary>::op<Opts>(*value, b);
            }
            else {
               dump<static_cast<std::byte>(0)>(b);
            }
         }
      };
      
      template <class T>
      requires glaze_object_t<T>
      struct to_binary<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            dump_int(N, b); // even though N is known at compile time in this case, it is not known for partial cases, so we still use a compressed integer
            
            for_each<N>([&](auto I) {
               static constexpr auto item = std::get<I>(meta_v<V>);
               dump_int(I, b); // dump the known key as an integer
               if constexpr (std::is_member_pointer_v<
                                std::tuple_element_t<1, decltype(item)>>) {
                  write<binary>::op<Opts>(value.*std::get<1>(item), b);
               }
               else {
                  write<binary>::op<Opts>(std::get<1>(item)(value), b);
               }
            });
         }
      };

      template <class T>
      requires glaze_array_t<T>
      struct to_binary<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<meta_t<V>>>([&](auto I) {
               write<binary>::op<Opts>(value.*std::get<I>(meta_v<V>), b);
            });
         }
      };
   }
   
   template <class T, class Buffer>
   inline void write_binary(T&& value, Buffer&& buffer) {
      write<opts{.format = binary}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
   
   template <class T>
   inline auto write_binary(T&& value) {
      std::string buffer{};
      write<opts{.format = binary}>(std::forward<T>(value), buffer);
      return buffer;
   }
   
   template <auto& Partial, opts Opts, class T, class Buffer>
   requires nano::ranges::input_range<Buffer> && (sizeof(nano::ranges::range_value_t<Buffer>) == sizeof(char))
   inline void write(T&& value, Buffer& buffer) noexcept
   {
      static constexpr auto partial = Partial;  // MSVC 16.11 hack

      if constexpr (nano::ranges::count(partial, "") > 0) {
         detail::write<binary>::op<Opts>(value, buffer);
      }
      else {
         static_assert(detail::glaze_object_t<std::decay_t<T>> ||
                          detail::map_t<std::decay_t<T>>,
                       "Only object types are supported for partial.");
         static constexpr auto sorted = sort_json_ptrs(partial);
         static constexpr auto groups = glz::group_json_ptrs<sorted>();
         static constexpr auto N =
            std::tuple_size_v<std::decay_t<decltype(groups)>>;

         detail::dump_int(N, buffer);

         if constexpr (detail::glaze_object_t<std::decay_t<T>>) {
            static constexpr auto key_to_int = detail::make_key_int_map<T>();
            glz::for_each<N>([&](auto I) {
               static constexpr auto group = []() {
                  return std::get<decltype(I)::value>(groups);
               }();  // MSVC internal compiler error workaround
               static constexpr auto key = std::get<0>(group);
               static constexpr auto sub_partial = std::get<1>(group);
               static constexpr auto frozen_map = detail::make_map<T>();
               static constexpr auto member_it = frozen_map.find(key);
               static_assert(member_it != frozen_map.end(),
                             "Invalid key passed to partial write");
               static constexpr auto member_ptr =
                  std::get<member_it->second.index()>(member_it->second);

               detail::dump_int(key_to_int.find(key)->second, buffer);
               write<sub_partial, Opts>(value.*member_ptr, buffer);
            });
         }
         else if constexpr (detail::map_t<std::decay_t<T>>) {
            glz::for_each<N>([&](auto I) {
               static constexpr auto group = []() {
                  return std::get<decltype(I)::value>(groups);
               }();  // MSVC internal compiler error workaround
               static constexpr auto key_value = std::get<0>(group);
               static constexpr auto sub_partial = std::get<1>(group);
               static thread_local auto key =
                  typename std::decay_t<T>::key_type(key_value); // TODO handle numeric keys
               detail::write<binary>::op<Opts>(key, buffer);
               auto it = value.find(key);
               if (it != value.end()) {
                  write<sub_partial, Opts>(it->second, buffer);
               }
               else {
                  throw std::runtime_error(
                     "Invalid key for map when writing out partial message");
               }      
            });
         }
      }
   }
   
   template <auto& Partial, class T, class Buffer>
   inline void write_binary(T&& value, Buffer&& buffer) {
      write<Partial, opts{}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}
