#include "emitter.h"
#include "lexer.h"

class Parser {
public:
  Parser(Lexer *l, Emitter *e);
  bool CheckToken(TokenType type);
  bool ChekPeek(TokenType type);
  void Match(TokenType type);
  void NextToken();

  void Parse();
  void Label();
  void Directive();
  void Instruction();
  void Operand();

private:
  Lexer *l;
  Token *token;
  Token *peek;
};
