#include "chirp/interpreter.h"
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <utility>
#include <iostream>

namespace chirp::interpreter {

// --- Type Getters / Singletons ---

std::shared_ptr<const Type> getMetaType() {
    static auto instance = std::make_shared<MetaType>();
    return instance;
}

std::shared_ptr<const Type> getBoolType() {
    static auto instance = std::make_shared<BoolType>();
    return instance;
}

std::shared_ptr<const Type> getUndecidedType() {
    static auto instance = std::make_shared<UndecidedType>();
    return instance;
}



std::shared_ptr<const Type> getSetType() {
    static auto instance = std::make_shared<SetType>();
    return instance;
}

std::shared_ptr<const Type> getVoidType() {
    static auto instance = std::make_shared<VoidType>();
    return instance;
}

std::shared_ptr<const Type> getEnumeratedSetType() {
    static auto instance = std::make_shared<EnumeratedSetType>();
    return instance;
}

std::shared_ptr<const Type> getRangeType() {
    static auto instance = std::make_shared<RangeType>();
    return instance;
}

std::shared_ptr<const Type> getConstructedSetType() {
    static auto instance = std::make_shared<ConstructedSetType>();
    return instance;
}

std::shared_ptr<const Type> getCompositeSetType() {
    static auto instance = std::make_shared<CompositeSetType>();
    return instance;
}

std::shared_ptr<const Type> getBindingType() {
    static auto instance = std::make_shared<BindingType>();
    return instance;
}

std::shared_ptr<const Type> getFunctionType() {
    static auto instance = std::make_shared<FunctionType>();
    return instance;
}

std::shared_ptr<const Type> getIntType() {
    static auto instance = std::make_shared<IntType>();
    return instance;
}

std::shared_ptr<const Type> getCharType() {
    static auto instance = std::make_shared<CharType>();
    return instance;
}

std::shared_ptr<const Type> getStringType() {
    static auto instance = std::make_shared<StringType>();
    return instance;
}

std::shared_ptr<const Type> getSymbolType() {
    static auto instance = std::make_shared<SymbolType>();
    return instance;
}

std::shared_ptr<const Type> getListType() {
    static auto instance = std::make_shared<ListType>();
    return instance;
}

std::shared_ptr<const Type> getTraitType() {
    static auto instance = std::make_shared<TraitType>();
    return instance;
}

std::shared_ptr<const Type> getModuleType() {
    static auto instance = std::make_shared<ModuleType>();
    return instance;
}

std::shared_ptr<const Type> getHeapAllocationType() {
    static auto instance = std::make_shared<HeapAllocationType>();
    return instance;
}

std::shared_ptr<const Type> getHeapSharedAllocationType() {
    static auto instance = std::make_shared<HeapSharedAllocationType>();
    return instance;
}

std::shared_ptr<const Type> getEnumFamilyType() {
    static auto instance = std::make_shared<EnumFamilyType>();
    return instance;
}

std::shared_ptr<const Type> getEnumVariantType() {
    static auto instance = std::make_shared<EnumVariantType>();
    return instance;
}


// --- Core Value Accessors ---

const Value& Bool() {
    static Value instance = Value::make_type(getBoolType());
    return instance;
}

const Value& True() {
    static Value instance = Value::make_bool(true);
    return instance;
}

const Value& False() {
    static Value instance = Value::make_bool(false);
    return instance;
}

const Value& Undecided() {
    static Value instance = Value::make_type(getUndecidedType());
    return instance;
}

const Value& UndecidedVal() {
    static Value instance = Value(getUndecidedType(), std::monostate{});
    return instance;
}

const Value& TypeVal() {
    static Value instance = Value::make_type(getMetaType());
    return instance;
}



const Value& Set() {
    static Value instance = Value(getSetType(), std::monostate{});
    return instance;
}

const Value& SetTypeVal() {
    static Value instance = Value::make_type(getSetType());
    return instance;
}

const Value& Void() {
    static Value instance = Value::make_type(getVoidType());
    return instance;
}

const Value& VoidVal() {
    static Value instance = Value();
    return instance;
}

// --- Value Implementation ---

Value::Value() : type_(getVoidType()), payload_(std::monostate{}) {}

Value Value::make_bool(bool val) {
    return Value(getBoolType(), val);
}

Value Value::make_int(BigInt val) {
    return Value(getIntType(), std::move(val));
}

Value Value::make_char(uint32_t codepoint) {
    return Value(getCharType(), CharTag{codepoint});
}

Value Value::make_string(std::string val) {
    return Value(getStringType(), std::move(val));
}

Value Value::make_type(std::shared_ptr<const Type> type_val) {
    return Value(getMetaType(), TypeTag{std::move(type_val)});
}

Value Value::make_binding(std::shared_ptr<Binding> binding_val) {
    return Value(getBindingType(), BindingTag{std::move(binding_val)});
}

Value Value::make_enumerated_set(std::vector<Value> elements) {
    return Value(getEnumeratedSetType(), EnumeratedSetTag{std::make_shared<std::vector<Value>>(std::move(elements))});
}

Value Value::make_range(Value start, Value end, bool inclusive_end) {
    return Value(getRangeType(), RangeTag{
        std::make_shared<Value>(std::move(start)),
        std::make_shared<Value>(std::move(end)),
        inclusive_end
    });
}

Value Value::make_constructed_set(const frontend::ConstructedSetExpr& set, std::shared_ptr<const RuntimeScopeChain> captured_scopes) {
    return Value(getConstructedSetType(), ConstructedSetTag{&set, std::move(captured_scopes)});
}

Value Value::make_composite_set(Value left, Value right, CompositeSetOp op) {
    return Value(getCompositeSetType(), CompositeSetTag{std::make_shared<Value>(std::move(left)), std::make_shared<Value>(std::move(right)), op});
}

Value Value::make_lambda(const frontend::LambdaExpr& lambda, std::shared_ptr<const RuntimeScopeChain> captured_scopes) {
    return Value(getFunctionType(), LambdaTag{&lambda, std::move(captured_scopes)});
}

Value Value::make_host_function(HostFunction fn) {
    return Value(getFunctionType(), HostFunctionTag{fn});
}

Value Value::make_symbol(std::string name) {
    return Value(getSymbolType(), SymbolTag{std::move(name)});
}

Value Value::make_list(std::vector<Value> elements) {
    return Value(getListType(), ListTag{std::make_shared<std::vector<Value>>(std::move(elements))});
}

Value Value::make_minted(std::shared_ptr<const Type> type, uint64_t id) {
    return Value(std::move(type), MintedTag{id});
}

Value Value::make_trait(uint64_t id, Value interface) {
    return Value(getTraitType(), TraitTag{id, std::make_shared<Value>(std::move(interface))});
}

Value Value::make_setness_impl(Value bp, Value br) {
    return Value(getFunctionType(), SetnessImplTag{
        std::make_shared<Value>(std::move(bp)),
        std::make_shared<Value>(std::move(br))
    });
}

Value Value::make_struct_instance(std::shared_ptr<const Type> type, std::map<std::string, Value> fields) {
    return Value(std::move(type), StructInstanceTag{std::make_shared<std::map<std::string, Value>>(std::move(fields))});
}

Value Value::make_module(std::string identity, std::map<std::string, std::shared_ptr<Binding>> exports) {
    return Value(getModuleType(), ModuleTag{
        std::move(identity),
        std::make_shared<std::map<std::string, std::shared_ptr<Binding>>>(std::move(exports))
    });
}

Value Value::make_heap_allocation(uint64_t id, Value stored) {
    return Value(getHeapAllocationType(), HeapAllocationTag{
        std::make_shared<HeapAllocationState>(id, std::move(stored))
    });
}

Value Value::make_heap_shared_allocation(uint64_t id, Value stored) {
    return Value(getHeapSharedAllocationType(), HeapAllocationTag{
        std::make_shared<HeapAllocationState>(id, std::move(stored))
    });
}

Value Value::make_enum_family(uint64_t node_id, std::vector<std::string> variants) {
    return Value(getEnumFamilyType(), EnumFamilyTag{node_id, std::move(variants)});
}

Value Value::make_enum_variant(uint64_t enum_node_id, std::string variant_name, size_t index) {
    return Value(getEnumVariantType(), EnumVariantTag{enum_node_id, std::move(variant_name), index});
}


std::shared_ptr<const Type> Value::getType() const {
    return type_;
}

bool Value::isVoid() const {
    return std::holds_alternative<std::monostate>(payload_) && type_ == getVoidType();
}

bool Value::isBool() const {
    return std::holds_alternative<bool>(payload_);
}

bool Value::asBool() const {
    if (!isBool()) {
        throw std::runtime_error("Value is not a Bool");
    }
    return std::get<bool>(payload_);
}

bool Value::isInt() const {
    return std::holds_alternative<BigInt>(payload_);
}

const BigInt& Value::asInt() const {
    if (!isInt()) {
        throw std::runtime_error("Value is not an int");
    }
    return std::get<BigInt>(payload_);
}

bool Value::isString() const {
    return std::holds_alternative<std::string>(payload_);
}

const std::string& Value::asString() const {
    if (!isString()) {
        throw std::runtime_error("Value is not a string");
    }
    return std::get<std::string>(payload_);
}

bool Value::isChar() const {
    return std::holds_alternative<CharTag>(payload_);
}

uint32_t Value::asChar() const {
    if (!isChar()) {
        throw std::runtime_error("Value is not a char");
    }
    return std::get<CharTag>(payload_).codepoint;
}

bool Value::isSymbol() const {
    return std::holds_alternative<SymbolTag>(payload_);
}

const std::string& Value::asSymbol() const {
    if (!isSymbol()) {
        throw std::runtime_error("Value is not a Symbol");
    }
    return std::get<SymbolTag>(payload_).name;
}

bool Value::isType() const {
    return std::holds_alternative<TypeTag>(payload_);
}

std::shared_ptr<const Type> Value::asType() const {
    if (!isType()) {
        throw std::runtime_error("Value is not a Type tag");
    }
    return std::get<TypeTag>(payload_).t;
}

bool Value::isBinding() const {
    return std::holds_alternative<BindingTag>(payload_);
}

std::shared_ptr<Binding> Value::asBinding() const {
    if (!isBinding()) {
        throw std::runtime_error("Value is not a Binding");
    }
    return std::get<BindingTag>(payload_).b;
}

bool Value::isEnumeratedSet() const {
    return std::holds_alternative<EnumeratedSetTag>(payload_);
}

const std::vector<Value>& Value::asEnumeratedSet() const {
    if (!isEnumeratedSet()) {
        throw std::runtime_error("Value is not an EnumeratedSet");
    }
    return *std::get<EnumeratedSetTag>(payload_).elements;
}

bool Value::isRange() const {
    return std::holds_alternative<RangeTag>(payload_);
}

Value::RangeTag Value::asRange() const {
    if (!isRange()) {
        throw std::runtime_error("Value is not a Range");
    }
    return std::get<RangeTag>(payload_);
}

bool Value::isConstructedSet() const {
    return std::holds_alternative<ConstructedSetTag>(payload_);
}

const frontend::ConstructedSetExpr& Value::asConstructedSet() const {
    if (!isConstructedSet()) {
        throw std::runtime_error("Value is not a ConstructedSet");
    }
    return *std::get<ConstructedSetTag>(payload_).set;
}

const Value::ConstructedSetTag& Value::asConstructedSetTag() const {
    if (!isConstructedSet()) {
        throw std::runtime_error("Value is not a ConstructedSet");
    }
    return std::get<ConstructedSetTag>(payload_);
}

bool Value::isList() const {
    return std::holds_alternative<ListTag>(payload_);
}

const std::vector<Value>& Value::asList() const {
    if (!isList()) {
        throw std::runtime_error("Value is not a List");
    }
    return *std::get<ListTag>(payload_).elements;
}


bool Value::isLambda() const {
    return std::holds_alternative<LambdaTag>(payload_);
}

const frontend::LambdaExpr& Value::asLambda() const {
    if (!isLambda()) {
        throw std::runtime_error("Value is not a Function");
    }
    return *std::get<LambdaTag>(payload_).lambda;
}

const Value::LambdaTag& Value::asLambdaTag() const {
    if (!isLambda()) {
        throw std::runtime_error("Value is not a Function");
    }
    return std::get<LambdaTag>(payload_);
}

bool Value::isHostFunction() const {
    return std::holds_alternative<HostFunctionTag>(payload_);
}

Value::HostFunction Value::asHostFunction() const {
    if (!isHostFunction()) {
        throw std::runtime_error("Value is not a host Function");
    }
    return std::get<HostFunctionTag>(payload_).fn;
}

bool Value::isCompositeSet() const {
    return std::holds_alternative<CompositeSetTag>(payload_);
}

const Value::CompositeSetTag& Value::asCompositeSet() const {
    if (!isCompositeSet()) {
        throw std::runtime_error("Value is not a CompositeSet");
    }
    return std::get<CompositeSetTag>(payload_);
}

bool Value::isMinted() const {
    return std::holds_alternative<MintedTag>(payload_);
}

uint64_t Value::asMintedId() const {
    if (!isMinted()) {
        throw std::runtime_error("Value is not a minted value");
    }
    return std::get<MintedTag>(payload_).id;
}

bool Value::isTrait() const {
    return std::holds_alternative<TraitTag>(payload_);
}

uint64_t Value::asTraitId() const {
    if (!isTrait()) {
        throw std::runtime_error("Value is not a trait");
    }
    return std::get<TraitTag>(payload_).id;
}

const Value& Value::asTraitInterface() const {
    if (!isTrait()) {
        throw std::runtime_error("Value is not a trait");
    }
    const auto& interface = std::get<TraitTag>(payload_).interface;
    if (!interface) {
        throw std::runtime_error("Trait has no interface");
    }
    return *interface;
}

bool Value::isSetnessImpl() const {
    return std::holds_alternative<SetnessImplTag>(payload_);
}

const Value::SetnessImplTag& Value::asSetnessImpl() const {
    if (!isSetnessImpl()) {
        throw std::runtime_error("Value is not a setness implementation");
    }
    return std::get<SetnessImplTag>(payload_);
}

bool Value::isStructInstance() const {
    return std::holds_alternative<StructInstanceTag>(payload_);
}

const Value::StructInstanceTag& Value::asStructInstance() const {
    if (!isStructInstance()) {
        throw std::runtime_error("Value is not a struct instance");
    }
    return std::get<StructInstanceTag>(payload_);
}

bool Value::isModule() const {
    return std::holds_alternative<ModuleTag>(payload_);
}

const Value::ModuleTag& Value::asModule() const {
    if (!isModule()) {
        throw std::runtime_error("Value is not a module");
    }
    return std::get<ModuleTag>(payload_);
}

bool Value::isHeapAllocation() const {
    return std::holds_alternative<HeapAllocationTag>(payload_);
}

const Value::HeapAllocationTag& Value::asHeapAllocation() const {
    if (!isHeapAllocation()) {
        throw std::runtime_error("Value is not a heap allocation");
    }
    return std::get<HeapAllocationTag>(payload_);
}

bool Value::isEnumFamily() const {
    return std::holds_alternative<EnumFamilyTag>(payload_);
}

const Value::EnumFamilyTag& Value::asEnumFamily() const {
    if (!isEnumFamily()) {
        throw std::runtime_error("Value is not an enum family");
    }
    return std::get<EnumFamilyTag>(payload_);
}

bool Value::isEnumVariant() const {
    return std::holds_alternative<EnumVariantTag>(payload_);
}

const Value::EnumVariantTag& Value::asEnumVariant() const {
    if (!isEnumVariant()) {
        throw std::runtime_error("Value is not an enum variant");
    }
    return std::get<EnumVariantTag>(payload_);
}

bool Value::TypeTag::operator==(const TypeTag& other) const {
    if (!t || !other.t) {
        return t == other.t;
    }
    return t->equals(*other.t) || other.t->equals(*t);
}

bool Value::operator==(const Value& other) const {
    if (isVoid() && other.isVoid()) {
        return true;
    }
    if (isVoid() || other.isVoid()) {
        return false;
    }
    if (type_ != other.type_ && (!type_ || !other.type_ || !type_->equals(*other.type_))) {
        return false;
    }
    return payload_ == other.payload_;
}

bool Value::EnumeratedSetTag::operator==(const EnumeratedSetTag& other) const {
    if (!elements || !other.elements) {
        return elements == other.elements;
    }
    return *elements == *other.elements;
}

bool Value::ListTag::operator==(const ListTag& other) const {
    if (!elements || !other.elements) {
        return elements == other.elements;
    }
    return *elements == *other.elements;
}

bool Value::SetnessImplTag::operator==(const SetnessImplTag& other) const {
    if (!bp || !other.bp) {
        if (bp != other.bp) return false;
    } else if (*bp != *other.bp) {
        return false;
    }

    if (!br || !other.br) {
        return br == other.br;
    }
    return *br == *other.br;
}

bool Value::StructInstanceTag::operator==(const StructInstanceTag& other) const {
    if (!fields || !other.fields) {
        return fields == other.fields;
    }
    return *fields == *other.fields;
}


std::string Value::toString() const {
    if (isVoid()) {
        return "`void";
    }
    if (isEnumFamily()) {
        std::stringstream ss;
        ss << "enum {";
        const auto& vars = asEnumFamily().variants;
        for (size_t i = 0; i < vars.size(); ++i) {
            ss << vars[i];
            if (i + 1 < vars.size()) ss << ", ";
        }
        ss << "}";
        return ss.str();
    }
    if (isEnumVariant()) {
        return asEnumVariant().variant_name;
    }
    if (isBool()) {
        return asBool() ? "true" : "false";
    }
    if (isInt()) {
        return asInt().to_string();
    }
    if (isString()) {
        return "\"" + asString() + "\"";
    }
    if (isChar()) {
        uint32_t cp = asChar();
        std::string s;
        s.push_back('\'');
        if (cp == '\\') s += "\\\\";
        else if (cp == '\'') s += "\\'";
        else if (cp == '\n') s += "\\n";
        else if (cp == '\r') s += "\\r";
        else if (cp == '\t') s += "\\t";
        else if (cp == '\0') s += "\\0";
        else if (cp < 32 || cp == 127) {
            char buf[16];
            snprintf(buf, sizeof(buf), "\\u%04x", cp);
            s += buf;
        } else {
            if (cp < 0x80) {
                s.push_back(static_cast<char>(cp));
            } else if (cp < 0x800) {
                s.push_back(static_cast<char>(0xc0 | (cp >> 6)));
                s.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            } else if (cp < 0x10000) {
                s.push_back(static_cast<char>(0xe0 | (cp >> 12)));
                s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                s.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            } else {
                s.push_back(static_cast<char>(0xf0 | (cp >> 18)));
                s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
                s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                s.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            }
        }
        s.push_back('\'');
        return s;
    }
    if (isSymbol()) {
        return asSymbol();
    }
    if (isType()) {
        return std::string(asType()->name());
    }
    if (isBinding()) {
        if (const auto* ref_type = dynamic_cast<const ReferenceType*>(type_.get())) {
            return (ref_type->is_mut() ? "&mut " : "&") + asBinding()->getCV().toString();
        }
        return "Binding(" + asBinding()->getCV().toString() + ")";
    }
    if (isEnumeratedSet()) {
        std::stringstream ss;
        ss << "{";
        const auto& elems = asEnumeratedSet();
        for (size_t i = 0; i < elems.size(); ++i) {
            ss << elems[i].toString();
            if (i + 1 < elems.size()) ss << ", ";
        }
        ss << "}";
        return ss.str();
    }
    if (isList()) {
        std::stringstream ss;
        ss << "[";
        const auto& elems = asList();
        for (size_t i = 0; i < elems.size(); ++i) {
            ss << elems[i].toString();
            if (i + 1 < elems.size()) ss << ", ";
        }
        ss << "]";
        return ss.str();
    }
    if (isRange()) {
        auto range = asRange();
        std::stringstream ss;
        ss << range.start->toString() << (range.inclusive_end ? "..=" : "..") << range.end->toString();
        return ss.str();
    }
    if (isMinted()) {
        return "<mint " + std::to_string(asMintedId()) + ">";
    }
    if (isTrait()) {
        return "<trait " + std::to_string(asTraitId()) + ">";
    }
    if (isConstructedSet()) {
        return "<constructed-set>";
    }
    if (isLambda()) {
        return "<function>";
    }
    if (isHostFunction()) {
        return "<host-function>";
    }
    if (isSetnessImpl()) {
        return "<setness-impl>";
    }
    if (isStructInstance()) {
        return "<struct-instance>";
    }
    if (isModule()) {
        return "<module>";
    }
    if (isHeapAllocation()) {
        const auto& state = asHeapAllocation().state;
        if (!state) {
            return "<heap allocation>";
        }
        if (type_ == getHeapSharedAllocationType()) {
            return "<shared heap allocation " + std::to_string(state->id) + ">";
        }
        return "<heap allocation " + std::to_string(state->id) + ">";
    }
    if (type_ == getUndecidedType()) {
        return "undecided";
    }

    if (type_ == getSetType()) {
        return "set";
    }
    return "Value(type=" + std::string(type_->name()) + ")";
}

MintedType::MintedType(uint64_t id)
    : id_(id), name_("MintType(" + std::to_string(id) + ")") {}

Value::HeapAllocationState::HeapAllocationState(uint64_t id, Value stored)
    : id(id), stored(std::make_shared<Value>(std::move(stored))) {}

// --- Set belonging/range helper implementations ---

Value belongsTo(const Value& S, const Value& v) {
    if (!S.getType()->hasSetness()) {
        throw std::runtime_error("Type '" + std::string(S.getType()->name()) + "' of value does not support set-ness");
    }
    return S.getType()->bp(S, v);
}

Value belongsRange(const Value& S, const Value& lc) {
    if (!S.getType()->hasSetness()) {
        throw std::runtime_error("Type '" + std::string(S.getType()->name()) + "' of value does not support set-ness");
    }
    return S.getType()->br(S, lc);
}

// --- Type bp/br default implementations ---

Value Type::bp(const Value& S, const Value& v) const {
    throw std::runtime_error("Type " + std::string(name()) + " does not support bp");
}

Value Type::br(const Value& S, const Value& lc) const {
    throw std::runtime_error("Type " + std::string(name()) + " does not support br");
}

// --- MetaType bp/br implementations (Type) ---

Value MetaType::bp(const Value& S, const Value& v) const {
    if (!S.isType()) {
        throw std::runtime_error("MetaType::bp: first argument S must be a type tag value");
    }
    auto t1 = v.getType();
    auto t2 = S.asType();
    if (dynamic_cast<const ReferenceType*>(t2.get())) {
        return t2->bp(S, v);
    }
    return Value::make_bool(t1 == t2 || (t1 && t2 && t1->equals(*t2)));
}

Value MetaType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true), Value::make_bool(false)});
}


// --- SetType bp/br implementations (set) ---

Value SetType::bp(const Value& S, const Value& v) const {
    return Value::make_bool(v.getType()->hasSetness());
}

Value SetType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true), Value::make_bool(false)});
}

// --- EnumeratedSetType bp/br implementations ({1, 2, 3}) ---

Value EnumeratedSetType::bp(const Value& S, const Value& v) const {
    if (!S.isEnumeratedSet()) {
        throw std::runtime_error("EnumeratedSetType::bp: S must be an EnumeratedSet value");
    }
    const auto& elems = S.asEnumeratedSet();
    bool found = std::find(elems.begin(), elems.end(), v) != elems.end();
    return Value::make_bool(found);
}

Value EnumeratedSetType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true), Value::make_bool(false)});
}

// --- RangeType bp/br implementations (1..5, 1..=5) ---

Value RangeType::bp(const Value& S, const Value& v) const {
    if (!S.isRange()) {
        throw std::runtime_error("RangeType::bp: S must be a Range value");
    }
    auto range = S.asRange();
    if (!range.start || !range.end) {
        return Value::make_bool(false);
    }
    if (v.getType() != range.start->getType()) {
        return Value::make_bool(false);
    }
    if (v.isInt() && range.start->isInt() && range.end->isInt()) {
        bool gte = range.start->asInt() <= v.asInt();
        bool lte = range.inclusive_end ? v.asInt() <= range.end->asInt() : v.asInt() < range.end->asInt();
        return Value::make_bool(gte && lte);
    }
    if (v.isChar() && range.start->isChar() && range.end->isChar()) {
        bool gte = range.start->asChar() <= v.asChar();
        bool lte = range.inclusive_end ? v.asChar() <= range.end->asChar() : v.asChar() < range.end->asChar();
        return Value::make_bool(gte && lte);
    }
    throw std::runtime_error("RangeType::bp on user-defined types requires evaluator context");
}

Value RangeType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true), Value::make_bool(false)});
}

// --- ConstructedSetType bp/br implementations ({x | predicate}) ---

Value ConstructedSetType::bp(const Value& S, const Value& v) const {
    throw std::runtime_error("ConstructedSetType::bp requires evaluator context");
}

Value ConstructedSetType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true), Value::make_bool(false)});
}

// --- CompositeSetType bp/br implementations (A ∪ B, A ∩ B) ---

Value CompositeSetType::bp(const Value& S, const Value& v) const {
    if (!S.isCompositeSet()) {
        throw std::runtime_error("CompositeSetType::bp: S must be a CompositeSet value");
    }
    const auto& comp = S.asCompositeSet();
    Value left_res = belongsTo(*comp.left, v);
    if (!left_res.isBool()) {
        throw std::runtime_error("Left operand of composite set did not return Bool for belonging");
    }

    if (comp.op == Value::CompositeSetOp::Union) {
        if (left_res.asBool()) return Value::make_bool(true);
        return belongsTo(*comp.right, v);
    } else {
        // Intersection
        if (!left_res.asBool()) return Value::make_bool(false);
        return belongsTo(*comp.right, v);
    }
}

Value CompositeSetType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true), Value::make_bool(false)});
}

// --- TraitType bp/br implementations (user-defined traits) ---

Value TraitType::bp(const Value& S, const Value& v) const {
    throw std::runtime_error("TraitType::bp requires evaluator context");
}

Value TraitType::br(const Value& S, const Value& lc) const {
    return Value::make_bool(false);
}

Value StructType::bp(const Value& S, const Value& v) const {
    if (!v.isStructInstance()) return Value::make_bool(false);
    return Value::make_bool(v.getType() == S.asType());
}

Value StructType::br(const Value& S, const Value& lc) const {
    throw std::runtime_error("Range operations on struct types are not supported");
}

Value EnumFamilyType::bp(const Value& S, const Value& v) const {
    if (!S.isEnumFamily()) {
        throw std::runtime_error("EnumFamilyType::bp: S must be an EnumFamily value");
    }
    if (!v.isEnumVariant()) {
        return Value::make_bool(false);
    }
    return Value::make_bool(S.asEnumFamily().node_id == v.asEnumVariant().enum_node_id);
}

Value EnumFamilyType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true), Value::make_bool(false)});
}

std::string_view ReferenceType::name() const {
    if (name_.empty()) {
        name_ = (is_mut_ ? "->mut " : "-> ") + target_type_.toString();
    }
    return name_;
}

bool ReferenceType::equals(const Type& other) const {
    if (const auto* o = dynamic_cast<const ReferenceType*>(&other)) {
        return is_mut_ == o->is_mut_ && target_type_ == o->target_type_;
    }
    return false;
}

Value ReferenceType::bp(const Value& S, const Value& v) const {
    if (!S.isType() || dynamic_cast<const ReferenceType*>(S.asType().get()) == nullptr) {
        throw std::runtime_error("ReferenceType::bp: S must be a ReferenceType value");
    }
    const auto* S_ref_t = dynamic_cast<const ReferenceType*>(S.asType().get());

    if (!v.isBinding()) {
        return Value::make_bool(false);
    }

    const auto* v_ref_t = dynamic_cast<const ReferenceType*>(v.getType().get());
    if (!v_ref_t) {
        return Value::make_bool(false);
    }

    // Mutability check: S_ref_t->is_mut() implies v_ref_t->is_mut()
    if (S_ref_t->is_mut() && !v_ref_t->is_mut()) {
        return Value::make_bool(false);
    }

    // Target constraint check
    return Value::make_bool(S_ref_t->target_type() == v_ref_t->target_type());
}

Value ReferenceType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true), Value::make_bool(false)});
}

} // namespace chirp::interpreter
