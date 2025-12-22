#pragma once
#include "ast.h"
#include <map>
#include <string>
#include <vector>

namespace HolyLua {

struct TypeInfo {
    ValueType type;
    bool isConst;
    bool isDefined;
    bool isOptional;
    bool isFunction;
    bool isStruct;
    std::string structTypeName;
};

struct ClassInfo {
  std::string name;
  std::vector<ClassField> fields;
  std::map<std::string, std::pair<ValueType, Visibility>> fieldInfo;
  std::map<std::string, std::pair<ValueType, Visibility>> methodInfo;
  std::map<std::string, bool> methodIsStatic;
  std::map<std::string, bool> fieldIsStatic;
  
  bool hasConstructor = false;
  std::vector<std::pair<std::string, ValueType>> constructorParams;
  std::vector<bool> constructorParamOptionals;
  std::vector<std::string> constructorParamTypeNames;
  
  ClassInfo() = default;
  
  ClassInfo(ClassInfo&& other) = default;
  ClassInfo& operator=(ClassInfo&& other) = default;
  
  ClassInfo(const ClassInfo&) = delete;
  ClassInfo& operator=(const ClassInfo&) = delete;
};


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

  Variable() = default;
  Variable(ValueType t, bool c, bool d, bool o, bool f, bool s = false, std::string stn = "")
      : type(t), isConst(c), isDefined(d), isOptional(o), isFunction(f), isStruct(s), structTypeName(stn) {}
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