#include "chirp/interpreter.h"
#include <stdexcept>
#include <algorithm>
#include <sstream>

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

std::shared_ptr<const Type> getAnyType() {
    static auto instance = std::make_shared<AnyType>();
    return instance;
}

std::shared_ptr<const Type> getEmptyType() {
    static auto instance = std::make_shared<EmptyType>();
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

std::shared_ptr<const Type> getStringType() {
    static auto instance = std::make_shared<StringType>();
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

const Value& TypeVal() {
    static Value instance = Value::make_type(getMetaType());
    return instance;
}

const Value& Any() {
    static Value instance = Value(getAnyType(), std::monostate{});
    return instance;
}

const Value& AnyTypeVal() {
    static Value instance = Value::make_type(getAnyType());
    return instance;
}

const Value& Empty() {
    static Value instance = Value(getEmptyType(), std::monostate{});
    return instance;
}

const Value& EmptyTypeVal() {
    static Value instance = Value::make_type(getEmptyType());
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

Value Value::make_int(int64_t val) {
    return Value(getIntType(), val);
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

Value Value::make_range(int64_t start, int64_t end, bool inclusive_end) {
    return Value(getRangeType(), RangeTag{start, end, inclusive_end});
}

Value Value::make_constructed_set(const frontend::ConstructedSetExpr& set) {
    return Value(getConstructedSetType(), ConstructedSetTag{&set});
}

Value Value::make_composite_set(Value left, Value right, CompositeSetOp op) {
    return Value(getCompositeSetType(), CompositeSetTag{std::make_shared<Value>(std::move(left)), std::make_shared<Value>(std::move(right)), op});
}

Value Value::make_lambda(const frontend::LambdaExpr& lambda) {
    return Value(getFunctionType(), LambdaTag{&lambda});
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
    return std::holds_alternative<int64_t>(payload_);
}

int64_t Value::asInt() const {
    if (!isInt()) {
        throw std::runtime_error("Value is not an int");
    }
    return std::get<int64_t>(payload_);
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

bool Value::isLambda() const {
    return std::holds_alternative<LambdaTag>(payload_);
}

const frontend::LambdaExpr& Value::asLambda() const {
    if (!isLambda()) {
        throw std::runtime_error("Value is not a Function");
    }
    return *std::get<LambdaTag>(payload_).lambda;
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

bool Value::operator==(const Value& other) const {
    if (isVoid() || other.isVoid()) {
        return false;
    }
    if (type_ != other.type_) {
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

std::string Value::toString() const {
    if (isVoid()) {
        return "`void";
    }
    if (isBool()) {
        return asBool() ? "true" : "false";
    }
    if (isInt()) {
        return std::to_string(asInt());
    }
    if (isString()) {
        return "\"" + asString() + "\"";
    }
    if (isType()) {
        return std::string(asType()->name());
    }
    if (isBinding()) {
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
    if (isRange()) {
        auto range = asRange();
        std::stringstream ss;
        ss << range.start << (range.inclusive_end ? "..=" : "..") << range.end;
        return ss.str();
    }
    if (isConstructedSet()) {
        return "<constructed-set>";
    }
    if (isLambda()) {
        return "<function>";
    }
    if (type_ == getAnyType()) {
        return "any";
    }
    if (type_ == getEmptyType()) {
        return "empty";
    }
    if (type_ == getSetType()) {
        return "set";
    }
    return "Value(type=" + std::string(type_->name()) + ")";
}

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
    return Value::make_bool(v.getType() == S.asType());
}

Value MetaType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true), Value::make_bool(false)});
}

// --- AnyType bp/br implementations (any) ---

Value AnyType::bp(const Value& S, const Value& v) const {
    return Value::make_bool(true);
}

Value AnyType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(true)});
}

// --- EmptyType bp/br implementations (empty) ---

Value EmptyType::bp(const Value& S, const Value& v) const {
    return Value::make_bool(false);
}

Value EmptyType::br(const Value& S, const Value& lc) const {
    return Value::make_enumerated_set({Value::make_bool(false)});
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
    if (!v.isInt()) {
        return Value::make_bool(false);
    }

    auto range = S.asRange();
    int64_t value = v.asInt();
    bool in_range = value >= range.start &&
        (range.inclusive_end ? value <= range.end : value < range.end);
    return Value::make_bool(in_range);
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

} // namespace chirp::interpreter
