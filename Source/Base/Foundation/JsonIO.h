#pragma once

#pragma once

#include "Math/BoundingBox.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>

#include <functional>
#include <stack>
#include <string>

template<typename>
constexpr bool kJsonFieldTypeUnsupported = false;

class JsonWriter
{
public:
    JsonWriter()
    {
        m_buffer = std::make_unique<rapidjson::StringBuffer>();
        m_writer = std::make_unique<rapidjson::PrettyWriter<rapidjson::StringBuffer>>(*m_buffer);
    }

    const char* GetString() { return m_buffer->GetString(); }

    void Object(std::function<void()> func)
    {
        m_writer->StartObject();
        func();
        m_writer->EndObject();
    }

    void Array(const char* key, std::function<void()> func)
    {
        m_writer->Key(key);
        m_writer->StartArray();
        func();
        m_writer->EndArray();
    }

    void Key(const char* key)
    {
        m_writer->Key(key);
    }

    template<typename T>
    void Field(const char* key, T val)
    {
        m_writer->Key(key);
        Field(val);
    }

    template<typename T>
    void Field(T val)
    {
        using ValueType = std::remove_cv_t<std::remove_reference_t<T>>;

        if constexpr (std::is_integral_v<ValueType>) m_writer->Int(val);
        else if constexpr (std::is_same_v<ValueType, float>)
            m_writer->Double(val);
        else if constexpr (std::is_same_v<ValueType, double>)
            m_writer->Double(val);
        else if constexpr (std::is_same_v<ValueType, bool>)
            m_writer->Bool(val);
        else if constexpr (std::is_same_v<ValueType, std::string>)
            m_writer->String(val.c_str());
        else if constexpr (std::is_same_v<ValueType, glm::vec2>)
        {
            m_writer->StartArray();
            m_writer->Double(val[0]);
            m_writer->Double(val[1]);
            m_writer->EndArray();
        }
        else if constexpr (std::is_same_v<ValueType, glm::vec3>)
        {
            m_writer->StartArray();
            m_writer->Double(val[0]);
            m_writer->Double(val[1]);
            m_writer->Double(val[2]);
            m_writer->EndArray();
        }
        else if constexpr (std::is_same_v<ValueType, glm::vec4>)
        {
            m_writer->StartArray();
            m_writer->Double(val[0]);
            m_writer->Double(val[1]);
            m_writer->Double(val[2]);
            m_writer->Double(val[3]);
            m_writer->EndArray();
        }
        else if constexpr (std::is_same_v<ValueType, AABB>)
        {
            glm::vec3 aabbMin = val.GetMin();
            glm::vec3 aabbMax = val.GetMax();

            m_writer->StartArray();
            m_writer->Double(aabbMin.x);
            m_writer->Double(aabbMin.y);
            m_writer->Double(aabbMin.z);
            m_writer->Double(aabbMax.x);
            m_writer->Double(aabbMax.y);
            m_writer->Double(aabbMax.z);
            m_writer->EndArray();
        }
        else if constexpr (std::is_enum_v<ValueType>)
        {
            m_writer->Int(static_cast<std::underlying_type_t<ValueType>>(val));
        }
        else
        {
            static_assert(kJsonFieldTypeUnsupported<ValueType>, "Unsupported type");
        }
    }

private:
    std::unique_ptr<rapidjson::StringBuffer> m_buffer;
    std::unique_ptr<rapidjson::PrettyWriter<rapidjson::StringBuffer>> m_writer;
};

class JsonReader
{
public:
    JsonReader(const char* buffer, uint32_t bufferSize)
    {
        m_dom.Parse(buffer, bufferSize);
        m_current = &m_dom;
    }

    bool HasMember(const char* key) const
    {
        return m_current->HasMember(key);
    }

    void Object(const char* key, std::function<void()> func)
    {
        if (m_current && m_current->HasMember(key))
        {
            const auto& obj = (*m_current)[key];
            if (obj.IsObject())
            {
                m_stack.push(m_current);
                m_current = &obj;
                func();
                m_current = m_stack.top();
                m_stack.pop();
            }
        }
    }

    void Array(const char* key, std::function<void()> item_processor)
    {
        if (m_current->HasMember(key))
        {
            const auto& array = (*m_current)[key];
            if (array.IsArray())
            {
                for (const auto& item : array.GetArray())
                {
                    m_stack.push(m_current);
                    m_current = &item;
                    item_processor();
                    m_current = m_stack.top();
                    m_stack.pop();
                }
            }
        }
    }

    template<typename T>
    bool Field(const char* key, T& out_value) const
    {
        if (!m_current || !m_current->HasMember(key))
        {
            return false;
        }
        return Field((*m_current)[key], out_value);
    }

    template<typename T>
    bool Field(int key, T& out_value) const
    {
        if (!m_current || !m_current->IsArray() || key >= m_current->Size())
        {
            return false;
        }
        return Field((*m_current)[key], out_value);
    }

    template<typename T>
    bool Field(const rapidjson::Value& value, T& out_value) const
    {
        if constexpr (std::is_integral_v<T>)
        {
            if (value.IsInt())
            {
                out_value = value.GetInt();
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
        {
            if (value.IsNumber())
            {
                out_value = value.GetDouble();
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            if (value.IsBool())
            {
                out_value = value.GetBool();
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            if (value.IsString())
            {
                out_value = value.GetString();
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, glm::vec2>)
        {
            if (value.IsArray())
            {
                out_value = glm::vec2(value[0].GetDouble(), value[1].GetDouble());
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, glm::vec3>)
        {
            if (value.IsArray())
            {
                out_value = glm::vec3(value[0].GetDouble(), value[1].GetDouble(), value[2].GetDouble());
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, glm::vec4>)
        {
            if (value.IsArray())
            {
                out_value = glm::vec4(value[0].GetDouble(), value[1].GetDouble(), value[2].GetDouble(), value[3].GetDouble());
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, AABB>)
        {
            if (value.IsArray())
            {
                glm::vec3 aabbMin = glm::vec3(value[0].GetDouble(), value[1].GetDouble(), value[2].GetDouble());
                glm::vec3 aabbMax = glm::vec3(value[4].GetDouble(), value[4].GetDouble(), value[5].GetDouble());
                out_value = AABB(aabbMin, aabbMax);
                return true;
            }
        }
        else if constexpr (std::is_enum_v<T>)
        {
            if (value.IsInt())
            {
                out_value = static_cast<T>(value.GetInt());
                return true;
            }
        }
        return false;
    }

private:
    rapidjson::Document m_dom;
    const rapidjson::Value* m_current;
    std::stack<const rapidjson::Value*> m_stack;
};