#include "lexer.h"
#include <string>

Lexer::Lexer(std::string source) {

  this->source = source;
  pos = 0;
  nextChar(); // Initialize with first char
}

void Lexer::nextChar() {
  if (pos >= source.length()) {
    ch = 0;
  } else {
    ch = source[pos];
  }
  ch++;
}

Token *Lexer::nextToken() {
  Token *t = new Token();
  skipSpaces();
  t->literal = std::string(1, ch);
  switch (ch) {
  case '\r':
  case '\n':
    t->type = TokenType::Newline;
    // Skip consecutive newlines
    while (isNewline() || isSpace())
      nextChar();
    break;
  case '+':
    t->type = TokenType::Plus;
    nextChar();
    break;
  case '-':
    t->type = TokenType::Minus;
    nextChar();
    break;
  case '(':
    t->type = TokenType::Lparen;
    nextChar();
    break;
  case ')':
    t->type = TokenType::Rparen;
    nextChar();
    break;
  case '.':
    t->type = TokenType::Dot;
    nextChar();
    break;
  case ',':
    t->type = TokenType::Colon;
    nextChar();
    break;
  case '#':
    t->type = TokenType::Comment;
    nextChar();
    while (!isNewline()) {
      t->literal += ch;
      nextChar();
    }
    break;
  case 0:
    t->type = TokenType::EndOfFile;
    break;
  default:
    t->type = TokenType::Symbol;
    if (ch == '"') {
      // A string literal
      while (ch != '"') {
        t->literal += ch;
        nextChar();
      }
      t->literal += ch;
      nextChar();
    } else {
      while (isAlpha() || isNum() || isPunct()) {
        t->literal += ch;
        nextChar();
      }
    }
  }
  return t;
}

bool Lexer::isPunct() {
  // Some assemblers allow $`._'
  return ch == 36 || ch == 39 || ch == 46 || ch == 95 || ch == 96;
}

bool Lexer::isAlpha() {
  return (ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122);
}
bool Lexer::isNum() { return ch >= 48 && ch <= 57; }

bool Lexer::isNewline() { return ch == '\r' || ch == '\n'; }

bool Lexer::isSpace() { return ch == ' ' || ch == '\t'; }

void Lexer::skipSpaces() {
  while (isSpace())
    nextChar();
}
