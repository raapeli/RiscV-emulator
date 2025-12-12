#include <string>

enum class TokenType {
  Symbol = 0,
  Lparen,
  Rparen,
  Plus,
  Minus,
  Dot,
  Colon,
  Comma,
  Newline,
  Comment,
  EndOfFile
};

class Token {
public:
  TokenType type;
  std::string literal;
};
