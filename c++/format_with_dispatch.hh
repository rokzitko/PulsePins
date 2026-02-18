#include <cctype>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

// Dispatcher: token -> function(param_string) -> replacement
using FormatDispatch =
    std::unordered_map<char, std::function<std::string(std::string_view /*param*/)>>;

struct FormatError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Format syntax:
//   - "%%"          -> literal '%'
//   - "%a"          -> token 'a' with empty param
//   - "%12a"        -> token 'a' with param "12"
//   - "%foo_barb"   -> token 'b' with param "foo_bar" (anything between % and token)
// Token is a single character present in dispatch.
inline std::string format_with_dispatch(std::string_view fmt, const FormatDispatch& dispatch) {
  std::string out;
  out.reserve(fmt.size());

  const auto is_token_char = [&](unsigned char ch) -> bool {
    return dispatch.find(static_cast<char>(ch)) != dispatch.end();
  };

  for (std::size_t i = 0; i < fmt.size(); ++i) {
    char ch = fmt[i];
    if (ch != '%') {
      out.push_back(ch);
      continue;
    }

    // '%' at end is an error
    if (i + 1 >= fmt.size())
      throw FormatError("Dangling '%' at end of format string");

    // "%%" -> literal '%'
    if (fmt[i + 1] == '%') {
      out.push_back('%');
      ++i;
      continue;
    }

    // Parse param: everything after '%' up to (but not including) the token char.
    std::size_t param_begin = i + 1;
    std::size_t j = param_begin;

    // We need to find the token (a char that exists in dispatch).
    // The token is defined as the FIRST subsequent character that is a known token.
    while (j < fmt.size() && !is_token_char(static_cast<unsigned char>(fmt[j]))) {
      // You can optionally reject control chars inside param:
      if (static_cast<unsigned char>(fmt[j]) < 0x20)
        throw FormatError("Control character inside placeholder parameter");
      ++j;
    }

    if (j >= fmt.size()) {
      throw FormatError("Unterminated placeholder: no known token after '%'");
    }

    const char token = fmt[j];
    const auto it = dispatch.find(token);
    if (it == dispatch.end()) {
      // Should not happen because is_token_char checked it, but keep robust.
      throw FormatError("Unknown token in placeholder");
    }

    std::string_view param = fmt.substr(param_begin, j - param_begin);
    out += it->second(param);

    i = j; // continue after token
  }

  return out;
}
