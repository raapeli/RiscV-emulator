#include "token.h"

class Lexer {

public:
  Lexer(std::string source);
  Token *NextToken();

private:
  std::string source;
  int pos; // Pos of next char to read;
  char ch; // Current char;
  void NextChar();
  bool IsPunct();
  bool IsAlpha();
  bool IsNum();
  bool IsNewline();
  bool IsSpace();
  void SkipSpaces();
};
