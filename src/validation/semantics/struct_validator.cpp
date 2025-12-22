#include "../../../include/validation/semantics/struct_validator.h"

namespace HolyLua {

StructValidator::StructValidator(ErrorReporter &reporter) 
    : reporter(reporter) {}

bool StructValidator::collectStructDeclarations(const Program &program,
                                              std::map<std::string, StructInfo> &structTable) {
    for (size_t i = 0; i < program.statements.size(); i++) {
        auto &stmt = program.statements[i];
        if (!stmt) {
            continue;
        }
        
        if (auto *structDecl = dynamic_cast<StructDecl *>(stmt.get())) {
            // check if struct is already defined
            if (structTable.count(structDecl->name)) {
                reporter.reportError("Struct '" + structDecl->name + "' is already defined",
                                   structDecl->line);
                return false;
            }

            // add struct to table
            StructInfo info;
            info.name = structDecl->name;
            info.fields = structDecl->fields;

            // build field type map
            for (const auto &field : structDecl->fields) {
                info.fieldTypes[field.name] = {field.type, field.isOptional};
            }

            structTable[structDecl->name] = info;
        }
    }
    
    return true;
}

bool StructValidator::validateStructFieldAccess(const std::string &structName,
                                              const std::string &fieldName,
                                              const std::map<std::string, StructInfo> &structTable,
                                              int line) {
    if (!structTable.count(structName)) {
        reporter.reportError("Struct '" + structName + "' is not defined", line);
        return false;
    }

    const auto &structInfo = structTable.at(structName);
    
    for (const auto &field : structInfo.fields) {
        if (field.name == fieldName) {
            return true;
        }
    }

    reporter.reportError("Struct '" + structName + "' has no field '" + fieldName + "'", line);
    return false;
}

std::string StructValidator::getFieldStructType(const FieldAccessExpr *field,
                                               const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                               const std::map<std::string, StructInfo> &structTable,
                                               const std::string &currentClass) {
    if (!field)
        return "";
        
    std::string objectStructName;
    
    if (auto *v = dynamic_cast<const VarExpr *>(field->object.get())) {
        if (symbolTable.count(v->name)) {
            objectStructName = symbolTable.at(v->name).structTypeName;
        }
    }
    else if (dynamic_cast<const SelfExpr *>(field->object.get())) {
        objectStructName = currentClass;
    }
    else if (auto *innerField = dynamic_cast<const FieldAccessExpr *>(field->object.get())) {
        objectStructName = getFieldStructType(innerField, symbolTable, structTable, currentClass);
    }
    
    if (objectStructName.empty()) {
        return "";
    }
    
    if (structTable.count(objectStructName)) {
        auto &info = structTable.at(objectStructName);
        for (const auto &f : info.fields) {
            if (f.name == field->fieldName) {
                if (f.type == ValueType::STRUCT && !f.structTypeName.empty()) {
                    return f.structTypeName;
                }
                return objectStructName;
            }
        }
    }
    
    return "";
}

} // namespace HolyLua