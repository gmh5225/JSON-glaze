// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string>
#include <optional>

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif  // !FMT_HEADER_ONLY
#include "fmt/format.h"
#include "fmt/compile.h"

namespace glz
{
   namespace detail
   {
      struct source_info
      {
         size_t line{};
         size_t column{};
         std::string context;
      };

      inline std::optional<source_info> get_source_info(const auto& buffer,
                                                        const std::size_t index)
      {
         if (index >= buffer.size()) {
            return std::nullopt;
         }

         const std::size_t r_index = buffer.size() - index - 1;
         const auto start = std::begin(buffer) + index;
         const auto count = std::count(std::begin(buffer), start, '\n');
         const auto rstart = std::rbegin(buffer) + r_index;
         const auto pnl = std::find(rstart, std::rend(buffer), '\n');
         const auto dist = std::distance(rstart, pnl);
         const auto nnl = std::find(start, std::end(buffer), '\n');

         std::string context{
            std::begin(buffer) +
               (pnl == std::rend(buffer) ? 0 : index - dist + 1),
            nnl};
         return source_info{static_cast<std::size_t>(count + 1),
                           static_cast<std::size_t>(dist), context};
      }

      inline std::string generate_error_string(const sv error,
                                               const source_info& info,
                                               const sv filename = "")
      {
         std::string s{};
         auto it = std::back_inserter(s);
         
         if (!filename.empty()) {
            fmt::format_to(it, FMT_COMPILE("{}:"), filename);
         }
         
         fmt::format_to(it, FMT_COMPILE("{}:{}: {}\n"), info.line, info.column, error);
         fmt::format_to(it, FMT_COMPILE("   {}\n   "), info.context);
         fmt::format_to(it, FMT_COMPILE("{: <{}}^\n"), "", info.column - 1);
         return s;
      }
   }  // namespace detail
}  // namespace test
