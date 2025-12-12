#include <string>

#include "token.h"

class Lexer {

public:
  Lexer(std::string source);

private:
  std::string source;
  int pos; // Pos of next char to read;
  char ch; // Current char;
  void nextChar();
  Token *nextToken();
  bool isPunct();
  bool isAlpha();
  bool isNum();
  bool isNewline();
  bool isSpace();
  void skipSpaces();
};
