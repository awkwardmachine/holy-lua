#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<Expr> Parser::postfix() {
  auto expr = primary();

  int loopCount = 0;
  while (true) {
    loopCount++;
    if (loopCount > 10) {
      error("Parser stuck in infinite loop", peek().line);
      break;
    }

    if (match({TokenType::DOT})) {
      if (!check(TokenType::IDENTIFIER)) {
        error("Expected member name after '.'", peek().line);
        return expr;
      }

      Token member = advance();
      // check if the left side is an enum type
      if (auto *varExpr = dynamic_cast<VarExpr*>(expr.get())) {
        if (declaredEnums.count(varExpr->name) > 0) {
          std::string enumName = varExpr->name;
          std::string valueName = member.lexeme;
          
          // verify the value exists in the enum
          if (enumValues.count(enumName)) {
            auto &values = enumValues[enumName];
            bool found = false;
            for (const auto &v : values) {
              if (v == valueName) {
                found = true;
                break;
              }
            }
            if (!found) {
              error("Enum '" + enumName + "' has no value '" + valueName + "'", member.line);
            }
          }
          
          auto enumExpr = std::make_unique<EnumAccessExpr>(enumName, valueName);
          enumExpr->line = member.line;
          expr = std::move(enumExpr);
          continue;
        }
      }

      // check if it's a method call
      if (check(TokenType::LPAREN)) {
        advance();

        std::vector<std::unique_ptr<Expr>> arguments;

        if (!check(TokenType::RPAREN)) {
          do {
            arguments.push_back(expression());
          } while (match({TokenType::COMMA}));
        }

        if (!match({TokenType::RPAREN})) {
          error("Expected ')' after arguments", peek().line);
          return expr;
        }

        auto methodCall = std::make_unique<MethodCall>(std::move(expr),
                                                        member.lexeme,
                                                        std::move(arguments));
        methodCall->line = member.line;
        expr = std::move(methodCall);
      } else {
        auto fieldAccess = std::make_unique<FieldAccessExpr>(std::move(expr),
                                                              member.lexeme);
        fieldAccess->line = member.line;
        expr = std::move(fieldAccess);
      }
    } else if (match({TokenType::BANG})) {
      int line = previous().line;
      auto unwrap = std::make_unique<ForceUnwrapExpr>(std::move(expr));
      unwrap->line = line;
      expr = std::move(unwrap);
    } else {
      break;
    }
  }
  return expr;
}

} // namespace HolyLua