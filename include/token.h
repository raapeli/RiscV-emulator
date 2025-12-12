#include <string>

enum TokenType {
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

const std::string TokenTypeStrings[] = {"Symbol",
                                        "Left-Parenthesis",
                                        "Right-Parenthesis",
                                        "Plus",
                                        "Minus",
                                        "Dot",
                                        "Colon",
                                        "Comma",
                                        "Newline",
                                        "Comment",
                                        "End-Of-File"};

class Token {
public:
  TokenType type;
  std::string literal;
};
