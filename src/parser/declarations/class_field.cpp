#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<ClassField> Parser::classField(Visibility visibility, bool isStatic) {
  // check for const modifier
  bool isConst = false;
  if (match({TokenType::CONST})) {
    isConst = true;
  }

  if (!check(TokenType::IDENTIFIER)) {
    error("Expected field name", peek().line);
    return nullptr;
  }

  Token fieldName = advance();
  ValueType fieldType = ValueType::INFERRED;
  std::string structTypeName = "";
  bool isOptional = false;
  bool hasDefault = false;
  std::variant<int64_t, double, std::string, bool, std::nullptr_t> defaultValue = nullptr;

  // parse type annotation
  if (match({TokenType::COLON})) {
    if (check(TokenType::IDENTIFIER)) {
      Token typeToken = advance();
      std::string typeName = typeToken.lexeme;

      if (declaredClasses.count(typeName) > 0 || declaredStructs.count(typeName) > 0) {
        fieldType = ValueType::STRUCT;
        structTypeName = typeName;
      } else if (declaredEnums.count(typeName) > 0) {
        fieldType = ValueType::ENUM;
        structTypeName = typeName;
      } else if (typeName == "number") {
        fieldType = ValueType::NUMBER;
      } else if (typeName == "string") {
        fieldType = ValueType::STRING;
      } else if (typeName == "bool") {
        fieldType = ValueType::BOOL;
      } else {
        error("Unknown type '" + typeName + "'", typeToken.line);
        return nullptr;
      }
    } else if (match({TokenType::TYPE_NUMBER})) {
      fieldType = ValueType::NUMBER;
    } else if (match({TokenType::TYPE_STRING})) {
      fieldType = ValueType::STRING;
    } else if (match({TokenType::TYPE_BOOL})) {
      fieldType = ValueType::BOOL;
    } else {
      error("Expected type after ':'", peek().line);
      return nullptr;
    }

    // check for optional marker
    if (match({TokenType::QUESTION})) {
      isOptional = true;
    }
  }

  // parse default value
  if (match({TokenType::ASSIGN})) {
    hasDefault = true;

    if (isConst) {
      // only allow literals for const fields
      if (match({TokenType::NUMBER})) {
        const auto &lit = previous().literal;
        if (std::holds_alternative<int64_t>(lit)) {
          defaultValue = std::get<int64_t>(lit);
          if (fieldType == ValueType::INFERRED) fieldType = ValueType::NUMBER;
        } else if (std::holds_alternative<double>(lit)) {
          defaultValue = std::get<double>(lit);
          if (fieldType == ValueType::INFERRED) fieldType = ValueType::NUMBER;
        }
      } else if (match({TokenType::STRING})) {
        defaultValue = std::get<std::string>(previous().literal);
        if (fieldType == ValueType::INFERRED) fieldType = ValueType::STRING;
      } else if (match({TokenType::TRUE})) {
        defaultValue = true;
        if (fieldType == ValueType::INFERRED) fieldType = ValueType::BOOL;
      } else if (match({TokenType::FALSE})) {
        defaultValue = false;
        if (fieldType == ValueType::INFERRED) fieldType = ValueType::BOOL;
      } else if (match({TokenType::NIL})) {
        defaultValue = nullptr;
      } else {
        error("Const fields must be initialized with literals", peek().line);
        return nullptr;
      }
    } else if (isStatic && declaredClasses.count(peek().lexeme) > 0) {
      error("Complex default values for static fields should be handled in initialization", peek().line);
      return nullptr;
    } else if (match({TokenType::NUMBER})) {
      const auto &lit = previous().literal;
      if (std::holds_alternative<int64_t>(lit)) {
        defaultValue = std::get<int64_t>(lit);
      } else if (std::holds_alternative<double>(lit)) {
        defaultValue = std::get<double>(lit);
      }
    } else if (match({TokenType::STRING})) {
      defaultValue = std::get<std::string>(previous().literal);
    } else if (match({TokenType::TRUE})) {
      defaultValue = true;
    } else if (match({TokenType::FALSE})) {
      defaultValue = false;
    } else if (match({TokenType::NIL})) {
      defaultValue = nullptr;
    } else {
      error("Default value must be a literal or class instantiation", peek().line);
      return nullptr;
    }
  } else {
    if (!isConst && fieldType == ValueType::INFERRED) {
      error("Non-const fields must have type annotation", peek().line);
      return nullptr;
    }
  }

  if (isConst && !hasDefault) {
    error("Const fields must be initialized", peek().line);
    return nullptr;
  }

  if (isConst && isOptional) {
    error("Const fields cannot be optional", peek().line);
    return nullptr;
  }

  auto field = std::make_unique<ClassField>(visibility, isStatic, fieldName.lexeme,
                                             fieldType, isOptional, hasDefault);
  field->isConst = isConst;
  field->structTypeName = structTypeName;
  if (hasDefault) {
    field->defaultValue = defaultValue;
  }

  return field;
}

} // namespace HolyLua