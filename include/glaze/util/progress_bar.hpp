// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cmath>
#include <iostream>
#include <string>

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif  // !FMT_HEADER_ONLY
#include "fmt/format.h"
#include "fmt/compile.h"

namespace glz
{
   struct progress_bar final
   {
      size_t width{};
      size_t completed{};
      size_t total{};
      double time_taken{};
      
      std::string string() const
      {
         std::string s{};
         
         const size_t one = 1;
         const auto total = std::max(this->total, one);
         const auto completed = std::min(this->completed, total);
         const auto progress = static_cast<double>(completed) / total;
         const auto percentage = static_cast<size_t>(std::round(progress * 100));

         if (width > 2) {
            const auto len = width - 2;
            const auto filled = static_cast<size_t>(std::round(progress * len));

            s += "[";

            for (size_t i = 0; i < filled; ++i) {
               s += '=';
            }

            for (size_t i = 0; i < len - filled; ++i) {
               s += '-';
            }

            s += "]";
         }

         const auto eta_s = static_cast<size_t>(
            std::round(((total - completed) * time_taken) /
                       std::max(completed, one)));
         const auto minutes = eta_s / 60;
         const auto seconds = eta_s - minutes * 60;
         fmt::format_to(std::back_inserter(s), FMT_COMPILE(" {}% | ETA: {}m {}s | {}/{}"),
                        std::round(percentage),
                        minutes,
                        seconds,
                        completed,
                        total);
         return s;
      }
   };

   inline std::ostream& operator<<(std::ostream &o, const progress_bar& bar)
   {
      o << bar.string();
      return o;
   }
}
