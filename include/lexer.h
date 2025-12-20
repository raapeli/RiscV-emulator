#pragma once

#include "token.h"
#include <cstdint>

class Lexer {

public:
  Lexer(std::string source);
  Token *NextToken();

private:
  std::string source;
  uint64_t pos; // Pos of next char to read
  char ch;      // Current char
  void NextChar();
  bool IsPunct();
  bool IsAlpha();
  bool IsNum();
  bool IsNewline();
  bool IsSpace();
  void SkipSpaces();
};
