#pragma once

#include <any>
#include <functional>
#include <memory>
#include <unordered_map>

class ReflectObject
{
public:
};

class ReflectProperty
{
public:
};

class ReflectType
{
public:
};

class ReflectTypeRegistry
{
public:
    static ReflectTypeRegistry* Get()
    {
        static ReflectTypeRegistry instance;
        return &instance;
    }

private:
    std::unordered_map<std::string, std::unique_ptr<ReflectType>> m_typesByName;
};