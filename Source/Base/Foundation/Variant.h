#pragma once

#include "PCH.h"

enum class VariantType
{
    Invalid,
    Bool,
    Int8,
    Int16,
    Int32,
    Int64,
    U8,
    U16,
    U32,
    U64,
    Float,
    Double,
    String,
    Pointer,
    Array
};

class Variant
{
public:
    Variant() : m_type(VariantType::Invalid) {}

    explicit Variant(bool value) : m_type(VariantType::Bool) { m_data.boolValue = value; }

    explicit Variant(int8_t value) : m_type(VariantType::Int8) { m_data.int8Value = value; }

    explicit Variant(int16_t value) : m_type(VariantType::Int16) { m_data.int16Value = value; }

    explicit Variant(int32_t value) : m_type(VariantType::Int32) { m_data.int32Value = value; }

    explicit Variant(int64_t value) : m_type(VariantType::Int64) { m_data.int64Value = value; }

    explicit Variant(uint8_t value) : m_type(VariantType::U8) { m_data.u8Value = value; }

    explicit Variant(uint16_t value) : m_type(VariantType::U16) { m_data.u16Value = value; }

    explicit Variant(uint32_t value) : m_type(VariantType::U32) { m_data.u32Value = value; }

    explicit Variant(uint64_t value) : m_type(VariantType::U64) { m_data.u64Value = value; }

    explicit Variant(float value) : m_type(VariantType::Float) { m_data.floatValue = value; }

    explicit Variant(double value) : m_type(VariantType::Double) { m_data.doubleValue = value; }

    explicit Variant(const std::string& value) : m_type(VariantType::String)
    {
        m_data.stringValue = new std::string(value);
    }

    explicit Variant(const char* value) : m_type(VariantType::String)
    {
        m_data.stringValue = new std::string(value ? value : "");
    }

    explicit Variant(void* value) : m_type(VariantType::Pointer) { m_data.pointerValue = value; }

    explicit Variant(const std::vector<Variant>& value) : m_type(VariantType::Array)
    {
        m_data.arrayValue = new std::vector<Variant>(value);
    }

    Variant(std::initializer_list<Variant> list) : m_type(VariantType::Array)
    {
        m_data.arrayValue = new std::vector<Variant>(list);
    }

    Variant(const Variant& other) { CopyFrom(other); }

    ~Variant() { Cleanup(); }

    Variant& operator=(const Variant& other)
    {
        if (this != &other)
        {
            CopyFrom(other);
        }
        return *this;
    }

    VariantType Type() const { return m_type; }

    bool IsInvalid() const { return m_type == VariantType::Invalid; }
    bool IsBool() const { return m_type == VariantType::Bool; }
    bool IsInt8() const { return m_type == VariantType::Int8; }
    bool IsInt16() const { return m_type == VariantType::Int16; }
    bool IsInt32() const { return m_type == VariantType::Int32; }
    bool IsInt64() const { return m_type == VariantType::Int64; }
    bool IsU8() const { return m_type == VariantType::U8; }
    bool IsU16() const { return m_type == VariantType::U16; }
    bool IsU32() const { return m_type == VariantType::U32; }
    bool IsU64() const { return m_type == VariantType::U64; }
    bool IsFloat() const { return m_type == VariantType::Float; }
    bool IsDouble() const { return m_type == VariantType::Double; }
    bool IsString() const { return m_type == VariantType::String; }
    bool IsPointer() const { return m_type == VariantType::Pointer; }
    bool IsArray() const { return m_type == VariantType::Array; }

    bool GetBool() const
    {
        if (IsBool())
        {
            return m_data.boolValue;
        }

        switch (m_type)
        {
            case VariantType::Int8: return m_data.int8Value != 0;
            case VariantType::Int16: return m_data.int16Value != 0;
            case VariantType::Int32: return m_data.int32Value != 0;
            case VariantType::Int64: return m_data.int64Value != 0;
            case VariantType::U8: return m_data.u8Value != 0;
            case VariantType::U16: return m_data.u16Value != 0;
            case VariantType::U32: return m_data.u32Value != 0;
            case VariantType::U64: return m_data.u64Value != 0;
            case VariantType::Float: return m_data.floatValue != 0.0f;
            case VariantType::Double: return m_data.doubleValue != 0.0;
            case VariantType::String: return !m_data.stringValue->empty();
            default: return false;
        }
    }

    int8_t GetInt8() const { return ConvertFromNumeric<int8_t>(); }

    int16_t GetInt16() const { return ConvertFromNumeric<int16_t>(); }

    int32_t GetInt32() const { return ConvertFromNumeric<int32_t>(); }

    int64_t GetInt64(int64_t default_value = 0) const { return ConvertFromNumeric<int64_t>(); }

    uint8_t GetU8(uint8_t default_value = 0) const { return ConvertFromNumeric<uint8_t>(); }

    uint16_t GetU16(uint16_t default_value = 0) const { return ConvertFromNumeric<uint16_t>(); }

    uint32_t GetU32(uint32_t default_value = 0) const { return ConvertFromNumeric<uint32_t>(); }

    uint64_t GetU64(uint64_t default_value = 0) const { return ConvertFromNumeric<uint64_t>(); }

    float GetFloat(float default_value = 0.0f) const { return ConvertFromNumeric<float>(); }

    double GetDouble(double default_value = 0.0) const { return ConvertFromNumeric<double>(); }

    std::string GetString() const
    {
        if (IsString())
        {
            return *m_data.stringValue;
        }

        return "";
    }

    void* GetPointer() const
    {
        if (IsPointer())
        {
            return m_data.pointerValue;
        }

        return nullptr;
    }

    template<typename T>
    T* GetPointerAs() const
    {
        return static_cast<T*>(GetPointer());
    }

    std::vector<Variant>& GetArray()
    {
        if (!IsArray())
        {
            throw std::runtime_error("Variant is not an array");
        }

        return *m_data.arrayValue;
    }

    const std::vector<Variant>& GetArray() const
    {
        if (!IsArray())
        {
            throw std::runtime_error("Variant is not an array");
        }

        return *m_data.arrayValue;
    }

    void set_bool(bool value)
    {
        Cleanup();
        m_type = VariantType::Bool;
        m_data.boolValue = value;
    }

    void SetInt8(int8_t value)
    {
        Cleanup();
        m_type = VariantType::Int8;
        m_data.int8Value = value;
    }

    void SetInt16(int16_t value)
    {
        Cleanup();
        m_type = VariantType::Int16;
        m_data.int16Value = value;
    }

    void SetInt32(int32_t value)
    {
        Cleanup();
        m_type = VariantType::Int32;
        m_data.int32Value = value;
    }

    void SetInt64(int64_t value)
    {
        Cleanup();
        m_type = VariantType::Int64;
        m_data.int64Value = value;
    }

    void SetU8(uint8_t value)
    {
        Cleanup();
        m_type = VariantType::U8;
        m_data.u8Value = value;
    }

    void SetU16(uint16_t value)
    {
        Cleanup();
        m_type = VariantType::U16;
        m_data.u16Value = value;
    }

    void SetU32(uint32_t value)
    {
        Cleanup();
        m_type = VariantType::U32;
        m_data.u32Value = value;
    }

    void SetU64(uint64_t value)
    {
        Cleanup();
        m_type = VariantType::U64;
        m_data.u64Value = value;
    }

    void SetFloat(float value)
    {
        Cleanup();
        m_type = VariantType::Float;
        m_data.floatValue = value;
    }

    void SetDouble(double value)
    {
        Cleanup();
        m_type = VariantType::Double;
        m_data.doubleValue = value;
    }

    void SetString(const std::string& value)
    {
        Cleanup();
        m_type = VariantType::String;
        m_data.stringValue = new std::string(value);
    }

    void SetPointer(void* value)
    {
        Cleanup();
        m_type = VariantType::Pointer;
        m_data.pointerValue = value;
    }

    void SetArray(const std::vector<Variant>& value)
    {
        Cleanup();
        m_type = VariantType::Array;
        m_data.arrayValue = new std::vector<Variant>(value);
    }

    size_t ArraySize() const
    {
        if (!IsArray())
        {
            return 0;
        }

        return m_data.arrayValue->size();
    }

    bool ArrayEmpty() const
    {
        if (!IsArray())
        {
            return true;
        }

        return m_data.arrayValue->empty();
    }

    Variant& ArrayAt(size_t index)
    {
        if (!IsArray())
        {
            throw std::runtime_error("Variant is not an array");
        }

        return m_data.arrayValue->at(index);
    }

    const Variant& ArrayAt(size_t index) const
    {
        if (!IsArray())
        {
            throw std::runtime_error("Variant is not an array");
        }

        return m_data.arrayValue->at(index);
    }

    void ArrayPushBack(const Variant& value)
    {
        if (!IsArray())
        {
            Cleanup();
            m_type = VariantType::Array;
            m_data.arrayValue = new std::vector<Variant>();
        }
        m_data.arrayValue->push_back(value);
    }

    void ArrayPopBack()
    {
        if (IsArray() && !m_data.arrayValue->empty())
        {
            m_data.arrayValue->pop_back();
        }
    }

    void ArrayClear()
    {
        if (IsArray())
        {
            m_data.arrayValue->clear();
        }
    }

    void ArrayResize(size_t newSize)
    {
        if (!IsArray())
        {
            Cleanup();
            m_type = VariantType::Array;
            m_data.arrayValue = new std::vector<Variant>();
        }
        m_data.arrayValue->resize(newSize);
    }

    bool operator==(const Variant& other) const
    {
        if (m_type != other.m_type)
        {
            return false;
        }

        switch (m_type)
        {
            case VariantType::Bool: return m_data.boolValue == other.m_data.boolValue;
            case VariantType::Int8: return m_data.int8Value == other.m_data.int8Value;
            case VariantType::Int16: return m_data.int16Value == other.m_data.int16Value;
            case VariantType::Int32: return m_data.int32Value == other.m_data.int32Value;
            case VariantType::Int64: return m_data.int64Value == other.m_data.int64Value;
            case VariantType::U8: return m_data.u8Value == other.m_data.u8Value;
            case VariantType::U16: return m_data.u16Value == other.m_data.u16Value;
            case VariantType::U32: return m_data.u32Value == other.m_data.u32Value;
            case VariantType::U64: return m_data.u64Value == other.m_data.u64Value;
            case VariantType::Float: return m_data.floatValue == other.m_data.floatValue;
            case VariantType::Double: return m_data.doubleValue == other.m_data.doubleValue;
            case VariantType::String: return *m_data.stringValue == *other.m_data.stringValue;
            case VariantType::Pointer: return m_data.pointerValue == other.m_data.pointerValue;
            case VariantType::Array: return *m_data.arrayValue == *other.m_data.arrayValue;
            case VariantType::Invalid: return true;
        }
        return false;
    }

    bool operator!=(const Variant& other) const { return !(*this == other); }

    Variant& operator[](size_t index) { return ArrayAt(index); }

    const Variant& operator[](size_t index) const { return ArrayAt(index); }

    bool Empty() const
    {
        switch (m_type)
        {
            case VariantType::Invalid: return true;
            case VariantType::String: return m_data.stringValue->empty();
            case VariantType::Array: return m_data.arrayValue->empty();
            case VariantType::Pointer: return m_data.pointerValue == nullptr;
            default: return false;
        }
    }

    static const char* TypeName(VariantType type)
    {
        switch (type)
        {
            case VariantType::Invalid: return "Invalid";
            case VariantType::Bool: return "Bool";
            case VariantType::Int8: return "Int8";
            case VariantType::Int16: return "Int16";
            case VariantType::Int32: return "Int32";
            case VariantType::Int64: return "Int64";
            case VariantType::U8: return "U8";
            case VariantType::U16: return "U16";
            case VariantType::U32: return "U32";
            case VariantType::U64: return "U64";
            case VariantType::Float: return "Float";
            case VariantType::Double: return "Double";
            case VariantType::String: return "String";
            case VariantType::Pointer: return "Pointer";
            case VariantType::Array: return "Array";
            default: return "Unknown";
        }
    }

private:
    void Cleanup()
    {
        switch (m_type)
        {
            case VariantType::String:
                delete m_data.stringValue;
                m_data.stringValue = nullptr;
                break;
            case VariantType::Array:
                delete m_data.arrayValue;
                m_data.arrayValue = nullptr;
                break;
            default: break;
        }
        m_type = VariantType::Invalid;
    }

    void CopyFrom(const Variant& other)
    {
        Cleanup();
        m_type = other.m_type;

        switch (m_type)
        {
            case VariantType::Bool: m_data.boolValue = other.m_data.boolValue; break;
            case VariantType::Int8: m_data.int8Value = other.m_data.int8Value; break;
            case VariantType::Int16: m_data.int16Value = other.m_data.int16Value; break;
            case VariantType::Int32: m_data.int32Value = other.m_data.int32Value; break;
            case VariantType::Int64: m_data.int64Value = other.m_data.int64Value; break;
            case VariantType::U8: m_data.u8Value = other.m_data.u8Value; break;
            case VariantType::U16: m_data.u16Value = other.m_data.u16Value; break;
            case VariantType::U32: m_data.u32Value = other.m_data.u32Value; break;
            case VariantType::U64: m_data.u64Value = other.m_data.u64Value; break;
            case VariantType::Float: m_data.floatValue = other.m_data.floatValue; break;
            case VariantType::Double: m_data.doubleValue = other.m_data.doubleValue; break;
            case VariantType::String: m_data.stringValue = new std::string(*other.m_data.stringValue); break;
            case VariantType::Pointer: m_data.pointerValue = other.m_data.pointerValue; break;
            case VariantType::Array: m_data.arrayValue = new std::vector<Variant>(*other.m_data.arrayValue); break;
            case VariantType::Invalid: break;
        }
    }

    template<typename T>
    T ConvertFromNumeric() const
    {
        switch (m_type)
        {
            case VariantType::Bool: return static_cast<T>(m_data.boolValue ? 1 : 0);
            case VariantType::Int8: return static_cast<T>(m_data.int8Value);
            case VariantType::Int16: return static_cast<T>(m_data.int16Value);
            case VariantType::Int32: return static_cast<T>(m_data.int32Value);
            case VariantType::Int64: return static_cast<T>(m_data.int64Value);
            case VariantType::U8: return static_cast<T>(m_data.u8Value);
            case VariantType::U16: return static_cast<T>(m_data.u16Value);
            case VariantType::U32: return static_cast<T>(m_data.u32Value);
            case VariantType::U64: return static_cast<T>(m_data.u64Value);
            case VariantType::Float: return static_cast<T>(m_data.floatValue);
            case VariantType::Double: return static_cast<T>(m_data.doubleValue);
            default: return T(0);
        }
    }

    union Data {
        bool boolValue;
        int8_t int8Value;
        int16_t int16Value;
        int32_t int32Value;
        int64_t int64Value;
        uint8_t u8Value;
        uint16_t u16Value;
        uint32_t u32Value;
        uint64_t u64Value;
        float floatValue;
        double doubleValue;
        std::string* stringValue;
        void* pointerValue;
        std::vector<Variant>* arrayValue;
    };
    Data m_data;
    VariantType m_type = VariantType::Invalid;
};