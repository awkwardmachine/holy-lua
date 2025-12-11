#pragma once
#include "ast.h"
#include <map>
#include <string>
#include <vector>


namespace HolyLua {

struct StructInfo {
  std::string name;
  std::vector<StructField> fields;
  std::map<std::string, std::pair<ValueType, bool>>
      fieldTypes;
};

struct Variable {
  ValueType type;
  bool isConst;
  bool isDefined;
  bool isOptional;
  bool isFunction;
  bool isStruct;
  std::string structTypeName;

  Variable()
      : type(ValueType::INFERRED), isConst(false), isDefined(false),
        isOptional(false), isFunction(false), isStruct(false) {}
  Variable(ValueType t, bool c, bool d, bool o, bool f, bool s = false,
           const std::string &st = "")
      : type(t), isConst(c), isDefined(d), isOptional(o), isFunction(f),
        isStruct(s), structTypeName(st) {}
};

struct FunctionInfo {
  std::string name;
  ValueType returnType;
  std::vector<std::pair<std::string, ValueType>> parameters;
  std::vector<bool> parameterOptionals;
  bool isGlobal;
  std::vector<FunctionInfo> nestedFunctions;
};

} // namespace HolyLua