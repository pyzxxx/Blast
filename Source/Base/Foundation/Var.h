#pragma once

#include "Log.h"
#include "PCH.h"

class VarBase
{
public:
    virtual ~VarBase() = default;

    const std::string& GetName() const { return m_name; }
    const std::string& GetDescription() const { return m_description; }

    virtual std::string GetValueAsString() const = 0;
    virtual bool SetValueFromString(const std::string& value) = 0;
    virtual std::string GetTypeName() const = 0;

protected:
    std::string m_name;
    std::string m_description;
};

template<typename T>
class Var : public VarBase
{
public:
    using OnChangedCallback = std::function<void(const T& oldValue, const T& newValue)>;

    Var(const std::string& name, const T& defaultValue, const std::string& description = "")
        : m_value(defaultValue), m_defaultValue(defaultValue)
    {
        m_name = name;
        m_description = description;
        Register();
    }

    const T& Get() const { return m_value; }

    void Set(const T& value)
    {
        if (m_value != value)
        {
            T oldValue = m_value;
            m_value = value;
            if (m_onChanged)
            {
                m_onChanged(oldValue, m_value);
            }
        }
    }

    void Reset() { Set(m_defaultValue); }

    void OnChanged(OnChangedCallback callback) { m_onChanged = callback; }

    std::string GetValueAsString() const override { return ToString(m_value); }

    bool SetValueFromString(const std::string& value) override
    {
        T parsedValue;
        if (FromString(value, parsedValue))
        {
            Set(parsedValue);
            return true;
        }
        return false;
    }

    std::string GetTypeName() const override { return TypeToString<T>(); }

private:
    T m_value;
    T m_defaultValue;
    OnChangedCallback m_onChanged;

    void Register();

    template<typename U>
    std::string ToString(const U& value) const
    {
        if constexpr (std::is_same_v<U, bool>)
        {
            return value ? "true" : "false";
        }
        else if constexpr (std::is_same_v<U, std::string>)
        {
            return value;
        }
        else
        {
            return std::to_string(value);
        }
    }

    template<typename U>
    bool FromString(const std::string& str, U& out) const
    {
        if constexpr (std::is_same_v<U, bool>)
        {
            std::string lower;
            lower.reserve(str.size());
            for (char c : str)
            {
                lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            if (lower == "true" || lower == "1" || lower == "yes" || lower == "on")
            {
                out = true;
                return true;
            }
            if (lower == "false" || lower == "0" || lower == "no" || lower == "off")
            {
                out = false;
                return true;
            }
            return false;
        }
        else if constexpr (std::is_same_v<U, int>)
        {
            try
            {
                size_t pos = 0;
                out = std::stoi(str, &pos);
                return pos == str.size();
            } catch (...)
            {
                return false;
            }
        }
        else if constexpr (std::is_same_v<U, float>)
        {
            try
            {
                size_t pos = 0;
                out = std::stof(str, &pos);
                return pos == str.size();
            } catch (...)
            {
                return false;
            }
        }
        else if constexpr (std::is_same_v<U, std::string>)
        {
            out = str;
            return true;
        }
        return false;
    }

    template<typename U>
    std::string TypeToString() const
    {
        if constexpr (std::is_same_v<U, bool>)
        {
            return "bool";
        }
        else if constexpr (std::is_same_v<U, int>)
        {
            return "int";
        }
        else if constexpr (std::is_same_v<U, float>)
        {
            return "float";
        }
        else if constexpr (std::is_same_v<U, std::string>)
        {
            return "string";
        }
        return "unknown";
    }
};

class VarRegistry
{
public:
    static VarRegistry& Get()
    {
        static VarRegistry s_instance;
        return s_instance;
    }

    void RegisterVar(VarBase* var)
    {
        if (m_vars.find(var->GetName()) != m_vars.end())
        {
            LOGW("Var '%s' already registered, overwriting", var->GetName().c_str());
        }
        m_vars[var->GetName()] = var;
    }

    VarBase* FindVar(const std::string& name)
    {
        auto it = m_vars.find(name);
        if (it != m_vars.end())
        {
            return it->second;
        }
        return nullptr;
    }

    template<typename T>
    Var<T>* FindVarTyped(const std::string& name)
    {
        VarBase* base = FindVar(name);
        if (base)
        {
            return dynamic_cast<Var<T>*>(base);
        }
        return nullptr;
    }

    std::vector<VarBase*> GetAllVars() const
    {
        std::vector<VarBase*> result;
        result.reserve(m_vars.size());
        for (const auto& [name, var] : m_vars)
        {
            result.push_back(var);
        }
        return result;
    }

    std::vector<VarBase*> FindVarsWithPrefix(const std::string& prefix) const
    {
        std::vector<VarBase*> result;
        for (const auto& [name, var] : m_vars)
        {
            if (name.find(prefix) == 0)
            {
                result.push_back(var);
            }
        }
        return result;
    }

    void ListAllVars() const
    {
        LOGI("=== Console Variables ===");
        for (const auto& [name, var] : m_vars)
        {
            LOGI("  %s (%s) = %s - %s", name.c_str(), var->GetTypeName().c_str(), var->GetValueAsString().c_str(),
                 var->GetDescription().c_str());
        }
    }

    bool SetVar(const std::string& name, const std::string& value)
    {
        VarBase* var = FindVar(name);
        if (var)
        {
            if (var->SetValueFromString(value))
            {
                return true;
            }
            LOGE("Failed to parse value '%s' for var '%s'", value.c_str(), name.c_str());
        }
        else
        {
            LOGE("Var '%s' not found", name.c_str());
        }
        return false;
    }

    std::optional<std::string> GetVar(const std::string& name) const
    {
        auto it = m_vars.find(name);
        if (it != m_vars.end())
        {
            return it->second->GetValueAsString();
        }
        return std::nullopt;
    }

private:
    std::unordered_map<std::string, VarBase*> m_vars;
};

template<typename T>
void Var<T>::Register()
{
    VarRegistry::Get().RegisterVar(this);
}
