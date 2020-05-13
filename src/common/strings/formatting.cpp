/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cctype>

#include "common/list_utils.h"
#include "common/strings/formatting.h"
#include "common/strings/utf8.h"
#include "common/terminal.h"
#include "common/translation.h"

namespace mtx::string {

std::string
format_timestamp(int64_t timestamp,
                unsigned int precision) {
  bool negative = 0 > timestamp;
  if (negative)
    timestamp *= -1;

  if (9 > precision) {
    auto shift = 5ll;
    for (int shift_idx = 9 - precision; shift_idx > 1; --shift_idx)
      shift *= 10;
    timestamp += shift;
  }

  auto result = fmt::format("{0}{1:02}:{2:02}:{3:02}",
                            negative ? "-" : "",
                             timestamp / 60 / 60 / 1'000'000'000,
                            (timestamp      / 60 / 1'000'000'000) % 60,
                            (timestamp           / 1'000'000'000) % 60);

  if (9 < precision)
    precision = 9;

  if (precision) {
    auto decimals = fmt::format(".{0:09}", timestamp % 1'000'000'000);

    if (decimals.length() > (precision + 1))
      decimals.erase(precision + 1);

    result += decimals;
  }

  return result;
}

std::string
format_timestamp(int64_t timestamp,
                std::string const &format) {
  auto result  = std::string{};
  auto width   = 0u;
  auto escaped = false;

  for (auto const &c : format) {
    if (escaped) {
      if (std::isdigit(c))
        width = width * 10 + (c - '0');

      else if (mtx::included_in(c, 'h', 'm', 's', 'H', 'M', 'S', 'n')) {
        auto lc    = std::tolower(c);
        auto value = lc == 'h' ?  (timestamp / 60 / 60 / 1000000000ll)
                   : lc == 'm' ? ((timestamp      / 60 / 1000000000ll) % 60)
                   : lc == 's' ? ((timestamp           / 1000000000ll) % 60)
                   :              (timestamp                           % 1000000000ll);

        if (c == 'n') {
          auto temp = fmt::format("{0:09}", value);
          if (width && (temp.length() > width))
            temp.erase(width);

          result += temp;

        } else
          result += fmt::format(std::isupper(c) ? "{0:02}" : "{0}", value);

      } else {
        result  += c;
        escaped  = false;
      }

    } else if (c == '%') {
      escaped = true;
      width   = 0;

    } else
      result += c;
  }

  return result;
}

std::string
to_string(double value,
          unsigned int precision) {
  int64_t scale = 1;
  for (int i = 0; i < static_cast<int>(precision); ++i)
    scale *= 10;

  return to_string(static_cast<int64_t>(value * scale), scale, precision);
}

std::string
to_string(int64_t numerator,
          int64_t denominator,
          unsigned int precision) {
  std::string output      = to_string(numerator / denominator);
  int64_t fractional_part = numerator % denominator;

  if (0 == fractional_part)
    return output;

  output                    += fmt::format(".{0:0{1}}", fractional_part, precision);
  std::string::iterator end  = output.end() - 1;

  while (*end == '0')
    --end;
  if (*end == '.')
    --end;

  output.erase(end + 1, output.end());

  return output;
}

std::wstring
format_paragraph(const std::wstring &text_to_wrap,
                 int indent_column,
                 const std::wstring &indent_first_line,
                 std::wstring indent_following_lines,
                 int wrap_column,
                 const std::wstring &break_chars) {
  std::wstring text   = indent_first_line;
  int current_column  = get_width_in_em(text);
  bool break_anywhere = translation_c::get_active_translation().m_line_breaks_anywhere;

  if (WRAP_AT_TERMINAL_WIDTH == wrap_column)
    wrap_column = get_terminal_columns() - 1;

  if ((0 != indent_column) && (current_column >= indent_column)) {
    text           += L"\n";
    current_column  = 0;
  }

  if (indent_following_lines.empty())
    indent_following_lines = std::wstring(indent_column, L' ');

  text                                += std::wstring(indent_column - current_column, L' ');
  current_column                       = indent_column;
  std::wstring::size_type current_pos  = 0;
  bool first_word_in_line              = true;
  bool needs_space                     = false;

  while (text_to_wrap.length() > current_pos) {
    std::wstring::size_type word_start = text_to_wrap.find_first_not_of(L" ", current_pos);
    if (std::string::npos == word_start)
      break;

    if (word_start != current_pos)
      needs_space = true;

    std::wstring::size_type word_end = text_to_wrap.find_first_of(break_chars, word_start);
    bool next_needs_space            = false;
    if (std::wstring::npos == word_end)
      word_end = text_to_wrap.length();

    else if (text_to_wrap[word_end] != L' ')
      ++word_end;

    else
      next_needs_space = true;

    std::wstring word    = text_to_wrap.substr(word_start, word_end - word_start);
    bool needs_space_now = needs_space && (text_to_wrap.substr(word_start, 1).find_first_of(break_chars) == std::wstring::npos);
    size_t word_length   = get_width_in_em(word);
    size_t new_column    = current_column + (needs_space_now ? 0 : 1) + word_length;

    if (break_anywhere && (new_column >= static_cast<size_t>(wrap_column))) {
      size_t offset = 0;
      while (((word_end - 1) > word_start) && ((128 > text_to_wrap[word_end - 1]) || ((new_column - offset) >= static_cast<size_t>(wrap_column)))) {
        offset   += get_width_in_em(text_to_wrap[word_end - 1]);
        word_end -= 1;
      }

      if (0 != offset)
        next_needs_space = false;

      word_length -= offset;
      new_column  -= offset;
      word.erase(word_end - word_start);
    }

    if (!first_word_in_line && (new_column >= static_cast<size_t>(wrap_column))) {
      text               += L"\n" + indent_following_lines;
      current_column      = indent_column;
      first_word_in_line  = true;
    }

    if (!first_word_in_line && needs_space_now) {
      text += L" ";
      ++current_column;
    }

    text               += word;
    current_column     += word_length;
    current_pos         = word_end;
    first_word_in_line  = false;
    needs_space         = next_needs_space;
  }

  text += L"\n";

  return text;
}

std::string
format_paragraph(const std::string &text_to_wrap,
                 int indent_column,
                 const std::string &indent_first_line,
                 std::string indent_following_lines,
                 int wrap_column,
                 const char *break_chars) {
  return to_utf8(format_paragraph(to_wide(text_to_wrap), indent_column, to_wide(indent_first_line), to_wide(indent_following_lines), wrap_column, to_wide(break_chars)));
}

std::string
to_hex(const unsigned char *buf,
       size_t size,
       bool compact) {
  if (!buf || !size)
    return {};

  std::string hex;
  for (int idx = 0; idx < static_cast<int>(size); ++idx)
    hex += (compact || hex.empty() ? ""s : " "s) + fmt::format(compact ?  "{0:02x}" : "0x{0:02x}", static_cast<unsigned int>(buf[idx]));

  return hex;
}

std::string
create_minutes_seconds_time_string(unsigned int seconds,
                                   bool omit_minutes_if_zero) {
  unsigned int minutes = seconds / 60;
  seconds              = seconds % 60;

  std::string  result  = fmt::format(NY("{0} second", "{0} seconds", seconds), seconds);

  if (!minutes && omit_minutes_if_zero)
    return result;

  return fmt::format("{0} {1}", fmt::format(NY("{0} minute", "{0} minutes", minutes), minutes), result);
}

std::string
format_file_size(int64_t size) {
  return size <       1024ll ? fmt::format(NY("{0} byte", "{0} bytes", size), size)
       : size <    1048576ll ? fmt::format(Y("{0}.{1} KiB"),                  size / 1024,               (size * 10 /               1024) % 10)
       : size < 1073741824ll ? fmt::format(Y("{0}.{1} MiB"),                  size / 1024 / 1024,        (size * 10 /        1024 / 1024) % 10)
       :                       fmt::format(Y("{0}.{1} GiB"),                  size / 1024 / 1024 / 1024, (size * 10 / 1024 / 1024 / 1024) % 10);
}

std::string
format_number(uint64_t number) {
  std::string output;

  if (number == 0)
    return "0";

  while (number != 0) {
    if (((output.size() + 1) % 4) == 0)
      output += '.';

    output += ('0' + (number % 10));
    number /= 10;
  }

  std::reverse(output.begin(), output.end());

  return output;
}

std::string
format_number(int64_t n) {
  auto sign = std::string{ n < 0 ? "-" : "" };
  return sign + format_number(static_cast<uint64_t>(std::abs(n)));
}

std::string
elide_string(std::string s,
             unsigned int max_length) {
  if ((s.size() < max_length) || !max_length)
    return s;

  s.resize(max_length - 1);
  s += u8"…";

  return s;
}

} // mtx::string
