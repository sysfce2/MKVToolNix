def create_iso639_language_list_file
  content = Mtx::OnlineFile.download("https://www.loc.gov/standards/iso639-2/php/code_list.php", "iso-639-2.html")

  entries_by_alpha_3 = {}

  parse_html_extract_table_data(content, %r{^.*?<table[^>]+>.*?<table[^>]+>}im).
    drop(1).
    each do |row|
    if %r{^([a-z]{3}) *\(([bt])\)<.*?>([a-z]{3})}.match(row[0].downcase)
      alpha_3_b = $2 == 'b' ? $1 : $3
      alpha_3_t = $2 == 'b' ? $3 : $1
    else
      alpha_3_b = row[0]
      alpha_3_t = row[0]
    end

    entries_by_alpha_3[alpha_3_b] = {
      "name"           => row[2],
      "bibliographic"  => alpha_3_b == alpha_3_t ? nil : alpha_3_b,
      "alpha_2"        => row[1],
      "alpha_3"        => alpha_3_t,
      "alpha_3_to_use" => alpha_3_b,
      "has_639_2"      => true,
    }
  end

  lines = Mtx::OnlineFile.download("https://iso639-3.sil.org/sites/iso639-3/files/downloads/iso-639-3.tab").
    split(%r{\n+}).
    map(&:chomp)

  headers = Hash[ *
    lines.
    shift.
    split(%r{\t}).
    map(&:downcase).
    each_with_index.
    map { |name, index| [ index, name ] }.
    flatten
  ]

  lines.
    map do |line|
    parts = line.split(%r{\t})
    entry = Hash[ *
      (0..parts.size).
      map { |idx| [ headers[idx], !parts[idx] || parts[idx].empty? ? nil : parts[idx] ] }.
      flatten
    ]

    entry
  end.
    reject { |entry| !%r{^[CLS]$}.match(entry["language_type"]) }. # Constructed, Living & Special
    each do |entry|
    alpha_3_to_use = entry["part2b"] || entry["id"]

    entry_639_2                        = entries_by_alpha_3[alpha_3_to_use]
    entries_by_alpha_3[alpha_3_to_use] = {
      "name"           => entry["ref_name"],
      "bibliographic"  => entry["part2b"] && (entry["part2b"] != entry["part2t"]) ? entry["part2b"] : nil,
      "alpha_2"        => entry["part1"],
      "alpha_3"        => entry["part2t"] || entry["id"],
      "alpha_3_to_use" => alpha_3_to_use,
      "has_639_2"      => !!entry_639_2,
    }
  end

  rows = entries_by_alpha_3.
    values.
    map do |entry|
    [ entry["name"].to_u8_cpp_string,
      entry["alpha_3_to_use"].to_cpp_string,
      (entry["alpha_2"] || '').to_cpp_string,
      entry["bibliographic"] ? entry["alpha_3"].to_cpp_string : '""s',
      entry["has_639_2"].to_s,
    ]
  end

  rows += ("a".."d").map do |letter|
    [ %Q{u8"Reserved for local use: qa#{letter}"s},
      %Q{u8"qa#{letter}"s},
      '""s',
      '""s',
      'true ',
    ]
  end

  header = <<EOT
/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   ISO 639 language definitions, lookup functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

// -----------------------------------------------------------------------
// NOTE: this file is auto-generated by the "dev:iso639_list" rake target.
// -----------------------------------------------------------------------

#include "common/iso639_types.h"

using namespace std::string_literals;

namespace mtx::iso639 {

std::vector<language_t> g_languages;

void
init() {
  g_languages.reserve(#{rows.size});

EOT

  footer = <<EOT
}

} // namespace mtx::iso639
EOT

  content       = header + format_table(rows.sort, :column_suffix => ',', :row_prefix => "  g_languages.emplace_back(", :row_suffix => ");").join("\n") + "\n" + footer
  cpp_file_name = "src/common/iso639_language_list.cpp"

  runq("write", cpp_file_name) { IO.write("#{$source_dir}/#{cpp_file_name}", content); 0 }
end
