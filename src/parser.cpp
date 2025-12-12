#include "parser.h"
#include <stdexcept>

Parser::Parser(Lexer *l) {
  this->l = l;
  this->token = nullptr;
  this->peek = nullptr;
  NextToken();
  NextToken(); // Called twice to init token and peek
}

bool Parser::CheckToken(TokenType type) { return type == token->type; }

bool Parser::ChekPeek(TokenType type) { return type == peek->type; }

void Parser::Match(TokenType type) {
  if (!CheckToken(type)) {
    throw std::runtime_error(
        "Got wrong TokenType, Expected: " + TokenTypeStrings[type] +
        "\nGot: " + token->literal);
  }
  NextToken();
}

void Parser::NextToken() {
  this->token = peek;
  this->peek = this->l->NextToken();
}

void Parser::Parse() {
  while (!CheckToken(TokenType::EndOfFile)) {
    // Label:
    if (ChekPeek(TokenType::Colon)) {
      Label();
    }
    // .Directive
    else if (CheckToken(TokenType::Dot)) {
      Directive();
    } else if (CheckToken(TokenType::Symbol)) {
      Instruction();
    }
    if (CheckToken(TokenType::Comment)) {
      NextToken();
    }

    Match(TokenType::Newline);
  }
}

void Parser::Instruction() {
  NextToken();

  // Is there at least one operand?
  if (!CheckToken(TokenType::Newline) && !CheckToken(TokenType::Comment)) {
    Operand();

    while (CheckToken(TokenType::Comma)) {
      NextToken();
      Operand();
    }
  }
}

void Parser::Operand() {
  if (CheckToken(TokenType::Plus) || CheckToken(TokenType::Minus))
    NextToken();

  if (CheckToken(TokenType::Symbol) && !ChekPeek(TokenType::Lparen)) {
    NextToken();
  } else {
    Match(TokenType::Symbol);
    Match(TokenType::Lparen);
    Match(TokenType::Symbol);
    Match(TokenType::Rparen);
  }
}

void Parser::Label() {
  Match(TokenType::Symbol);
  NextToken();
}

void Parser::Directive() {
  NextToken();
  Match(TokenType::Symbol);

  // Is there at least one operand?
  if (!CheckToken(TokenType::Newline) && !CheckToken(TokenType::Comment)) {

    if (CheckToken(TokenType::Dot) || CheckToken(TokenType::Plus) ||
        CheckToken(TokenType::Minus)) {
      NextToken();
    }

    Match(TokenType::Symbol);

    while (CheckToken(TokenType::Comma)) {
      if (CheckToken(TokenType::Plus) || CheckToken(TokenType::Minus)) {
        NextToken();
      }
      Match(TokenType::Symbol);
    }
  }
}
