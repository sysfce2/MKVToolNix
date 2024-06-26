/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   locale handling functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cerrno>
#include <clocale>
#include <codecvt>
#if HAVE_NL_LANGINFO
# include <langinfo.h>
#elif HAVE_LOCALE_CHARSET
# include <libcharset.h>
#endif
#if defined(SYS_WINDOWS)
# include <windows.h>
#endif

#include <QRegularExpression>

#include "common/memory.h"
#include "common/mm_io.h"
#include "common/mm_mem_io.h"
#include "common/mm_proxy_io.h"
#include "common/mm_text_io.h"
#include "common/qt.h"
#include "common/strings/parsing.h"
#ifdef SYS_WINDOWS
# include "common/fs_sys_helpers.h"
# include "common/strings/formatting.h"
#endif

charset_converter_cptr g_cc_local_utf8;

std::map<std::string, charset_converter_cptr> charset_converter_c::s_converters;

charset_converter_c::charset_converter_c(std::string charset)
  : m_charset{std::move(charset)}
{
}

std::string
charset_converter_c::utf8(const std::string &source) {
  return source;
}

std::string
charset_converter_c::native(const std::string &source) {
  return source;
}

std::string const &
charset_converter_c::get_charset()
  const {
  return m_charset;
}

charset_converter_cptr
charset_converter_c::init(const std::string &charset,
                          bool ignore_errors) {
  std::string actual_charset = charset.empty() ? get_local_charset() : charset;

  auto converter = s_converters.find(actual_charset);
  if (converter != s_converters.end())
    return (*converter).second;

#if defined(SYS_WINDOWS)
  if (windows_charset_converter_c::is_available(actual_charset))
    return charset_converter_cptr(new windows_charset_converter_c(actual_charset));
#endif

  if (ignore_errors && !iconv_charset_converter_c::is_available(actual_charset))
    return {};

  return charset_converter_cptr(new iconv_charset_converter_c(actual_charset));
}

bool
charset_converter_c::is_utf8_charset_name(const std::string &charset) {
  return Q(charset).contains(QRegularExpression{"^utf-?8$", QRegularExpression::CaseInsensitiveOption});
}

void
charset_converter_c::enable_byte_order_marker_detection(bool enable) {
  m_detect_byte_order_marker = enable;
}

bool
charset_converter_c::handle_string_with_bom(const std::string &source,
                                            std::string &recoded) {
  if (!m_detect_byte_order_marker)
    return false;

  if (!mm_text_io_c::has_byte_order_marker(source))
    return false;

  recoded.clear();
  mm_text_io_c io(std::make_shared<mm_mem_io_c>(reinterpret_cast<const uint8_t *>(source.c_str()), source.length()));
  std::string line;
  while (io.getline2(line))
    recoded += line;

  return true;
}

// ------------------------------------------------------------
static iconv_t const s_iconv_t_error_value = reinterpret_cast<iconv_t>(-1);

iconv_charset_converter_c::iconv_charset_converter_c(const std::string &charset)
  : charset_converter_c(charset)
  , m_is_utf8(false)
  , m_to_utf8_handle(s_iconv_t_error_value)
  , m_from_utf8_handle(s_iconv_t_error_value)
{
  if (is_utf8_charset_name(charset)) {
    m_is_utf8 = true;
    return;
  }

  m_to_utf8_handle = iconv_open("UTF-8", charset.c_str());
  if (s_iconv_t_error_value == m_to_utf8_handle)
    mxwarn(fmt::format(FY("Could not initialize the iconv library for the conversion from {0} to UTF-8. "
                          "Some strings will not be converted to UTF-8 and the resulting Matroska file "
                          "might not comply with the Matroska specs (error: {1}, {2}).\n"),
                       charset, errno, strerror(errno)));

  m_from_utf8_handle = iconv_open(charset.c_str(), "UTF-8");
  if (s_iconv_t_error_value == m_from_utf8_handle)
    mxwarn(fmt::format(FY("Could not initialize the iconv library for the conversion from UTF-8 to {0}. "
                          "Some strings cannot be converted from UTF-8 and might be displayed incorrectly (error: {1}, {2}).\n"),
                       charset, errno, strerror(errno)));
}

iconv_charset_converter_c::~iconv_charset_converter_c() {
  if (s_iconv_t_error_value != m_to_utf8_handle)
    iconv_close(m_to_utf8_handle);

  if (s_iconv_t_error_value != m_from_utf8_handle)
    iconv_close(m_from_utf8_handle);
}

std::string
iconv_charset_converter_c::utf8(const std::string &source) {
  std::string recoded;
  if (handle_string_with_bom(source, recoded))
    return recoded;

  return m_is_utf8 ? source : iconv_charset_converter_c::convert(m_to_utf8_handle, source);
}

std::string
iconv_charset_converter_c::native(const std::string &source) {
  return m_is_utf8 ? source : iconv_charset_converter_c::convert(m_from_utf8_handle, source);
}

std::string
iconv_charset_converter_c::convert(iconv_t handle,
                                   const std::string &source) {
  if (s_iconv_t_error_value == handle)
    return source;

  int length        = source.length() * 4;
  char *destination = (char *)safemalloc(length + 1);
  memset(destination, 0, length + 1);

  iconv(handle, nullptr, nullptr, nullptr, nullptr); // Reset the iconv state.

  size_t length_source      = length / 4;
  size_t length_destination = length;
  char *source_copy         = safestrdup(source.c_str());
  char *ptr_source          = source_copy;
  char *ptr_destination     = destination;
  iconv(handle, (ICONV_CONST char **)&ptr_source, &length_source, &ptr_destination, &length_destination);
  iconv(handle, nullptr, nullptr, &ptr_destination, &length_destination);

  safefree(source_copy);
  std::string result = destination;
  safefree(destination);

  return result;
}

bool
iconv_charset_converter_c::is_available(const std::string &charset) {
  if (is_utf8_charset_name(charset))
    return true;

  iconv_t handle = iconv_open("UTF-8", charset.c_str());
  if (s_iconv_t_error_value == handle)
    return false;

  iconv_close(handle);

  return true;
}

// ------------------------------------------------------------

#if defined(SYS_WINDOWS)

windows_charset_converter_c::windows_charset_converter_c(const std::string &charset)
  : charset_converter_c(charset)
  , m_is_utf8(is_utf8_charset_name(charset))
  , m_code_page(extract_code_page(charset))
{
}

windows_charset_converter_c::~windows_charset_converter_c() {
}

std::string
windows_charset_converter_c::utf8(const std::string &source) {
  std::string recoded;
  if (handle_string_with_bom(source, recoded))
    return recoded;

  return m_is_utf8 ? source : windows_charset_converter_c::convert(m_code_page, CP_UTF8, source);
}

std::string
windows_charset_converter_c::native(const std::string &source) {
  return m_is_utf8 ? source : windows_charset_converter_c::convert(CP_UTF8, m_code_page, source);
}

std::string
windows_charset_converter_c::convert(unsigned int source_code_page,
                                     unsigned int destination_code_page,
                                     const std::string &source) {
  if (source_code_page == destination_code_page)
    return source;

  int num_wide_chars = MultiByteToWideChar(source_code_page, 0, source.c_str(), -1, nullptr, 0);
  wchar_t *wbuffer   = new wchar_t[num_wide_chars];
  MultiByteToWideChar(source_code_page, 0, source.c_str(), -1, wbuffer, num_wide_chars);

  int num_bytes = WideCharToMultiByte(destination_code_page, 0, wbuffer, -1, nullptr, 0, nullptr, nullptr);
  char *buffer  = new char[num_bytes];
  WideCharToMultiByte(destination_code_page, 0, wbuffer, -1, buffer, num_bytes, nullptr, nullptr);

  std::string result = buffer;

  delete []wbuffer;
  delete []buffer;

  return result;
}

bool
windows_charset_converter_c::is_available(const std::string &charset) {
  unsigned int code_page = extract_code_page(charset);
  if (0 == code_page)
    return false;

  return IsValidCodePage(code_page);
}

unsigned int
windows_charset_converter_c::extract_code_page(const std::string &charset) {
  if (charset.substr(0, 2) != "CP")
    return 0;

  std::string number_as_str = charset.substr(2, charset.length() - 2);
  uint64_t number           = 0;
  if (!mtx::string::parse_number(number_as_str.c_str(), number))
    return 0;

  return number;
}

#endif  // defined(SYS_WINDOWS)

// ------------------------------------------------------------

std::string
get_local_charset() {
  std::string lc_charset;

#if defined(COMP_MINGW) || defined(COMP_MSC)
  lc_charset = fmt::format("CP{0}", GetACP());
#elif defined(SYS_SOLARIS)
  int i;

  lc_charset = nl_langinfo(CODESET);
  if (mtx::string::parse_number(lc_charset, i))
    lc_charset = "ISO"s + lc_charset + "-US"s;
#elif HAVE_NL_LANGINFO
  lc_charset = nl_langinfo(CODESET);
#elif HAVE_LOCALE_CHARSET
  lc_charset = locale_charset();
#endif

  return lc_charset;
}

std::string
get_local_console_charset() {
#if defined(SYS_WINDOWS)
  return fmt::format("CP{0}", GetACP());
#else
  return get_local_charset();
#endif
}

void
initialize_std_and_boost_filesystem_locales() {
  auto debug = debugging_c::requested("locale");

  std::vector<std::string> locales_to_try{""};

#if defined(SYS_UNIX)
  locales_to_try.emplace_back("en_US.UTF-8");
  locales_to_try.emplace_back("C.UTF-8");
#endif

  [[maybe_unused]] auto boost_initialized = false;
  [[maybe_unused]] auto ctype_initialized = false;

  for (auto const &locale_name : locales_to_try) {
    if (std::setlocale(LC_CTYPE, locale_name.c_str())) {
      ctype_initialized = true;
      mxdebug_if(debug, fmt::format("initialize_std_and_boost_filesystem_locales: LC_CTYPE initialized from '{0}'\n", locale_name));
      break;
    }
  }

  for (auto const &locale_name : locales_to_try) {
    auto locale      = locale_name.empty() ? std::locale() : std::locale{locale_name};
    auto utf8_locale = std::locale{ locale, new std::codecvt_utf8<wchar_t> };

    try {
      std::locale::global(utf8_locale);
      boost::filesystem::path::imbue(utf8_locale);

      boost_initialized = true;
      mxdebug_if(debug, fmt::format("initialize_std_and_boost_filesystem_locales: boost::filesystem initialized from '{0}' ({1})\n", locale_name, utf8_locale.name()));

      break;

    } catch (std::runtime_error &) {
    }
  }

#if defined(SYS_UNIX)
  if (!boost_initialized || !ctype_initialized)
    mxerror("Setting up the locale system based on the system's locale configuration failed. "
            "The fallback values of 'en_US.UTF-8' and 'C.UTF-8' did not work either. "
            "MKVToolNix requires a correctly configured & working locale system.");
#endif
}
