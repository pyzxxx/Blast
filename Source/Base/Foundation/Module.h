#pragma once

#include "PCH.h"

class ModuleBase
{
public:
    virtual ~ModuleBase() = default;
    virtual void Initialize() {}
    virtual void Terminate() {}
    virtual void Update(float dt) {}
};

struct ModuleNode
{
    std::string name;
    ModuleBase* module;
    std::vector<std::string> dependencies;
    int32_t updateOrder = 0;
};

class ModuleRegistry
{
public:
    static ModuleRegistry& Get()
    {
        static ModuleRegistry s_instance;
        return s_instance;
    }

    void Register(const std::string& name, ModuleBase* module, const std::vector<std::string>& deps,
                  int32_t updateOrder)
    {
        ModuleNode node;
        node.name = name;
        node.module = module;
        node.dependencies = deps;
        node.updateOrder = updateOrder;
        m_nodes.push_back(node);
        m_nameToIndex[name] = m_nodes.size() - 1;
    }

    void InitializeAll()
    {
        if (!m_initialized)
        {
            m_initOrder = TopologicalSort();
            m_updateOrder = m_nodes;

            for (size_t i = 0; i < m_updateOrder.size(); ++i)
            {
                for (size_t j = i + 1; j < m_updateOrder.size(); ++j)
                {
                    if (m_updateOrder[j].updateOrder < m_updateOrder[i].updateOrder)
                    {
                        auto temp = m_updateOrder[i];
                        m_updateOrder[i] = m_updateOrder[j];
                        m_updateOrder[j] = temp;
                    }
                }
            }

            m_initialized = true;
        }

        for (auto& node : m_initOrder)
        {
            node.module->Initialize();
        }
    }

    void TerminateAll()
    {
        assert(m_initialized && "InitializeAll() must be called before TerminateAll()");

        for (int32_t i = static_cast<int32_t>(m_initOrder.size()) - 1; i >= 0; --i)
        {
            m_initOrder[i].module->Terminate();
        }
    }

    void UpdateAll(float dt)
    {
        assert(m_initialized && "InitializeAll() must be called before UpdateAll()");

        for (auto& node : m_updateOrder)
        {
            node.module->Update(dt);
        }
    }

private:
    std::vector<ModuleNode> m_nodes;
    std::vector<ModuleNode> m_initOrder;
    std::vector<ModuleNode> m_updateOrder;
    std::unordered_map<std::string, size_t> m_nameToIndex;
    bool m_initialized = false;

    std::vector<ModuleNode> TopologicalSort()
    {
        std::unordered_map<std::string, std::unordered_set<std::string>> graph;
        std::unordered_map<std::string, int> inDegree;

        for (auto& node : m_nodes)
        {
            inDegree[node.name] = 0;
        }

        for (auto& node : m_nodes)
        {
            for (auto& dep : node.dependencies)
            {
                assert(m_nameToIndex.find(dep) != m_nameToIndex.end() && "Dependency module not found");
                graph[dep].insert(node.name);
                inDegree[node.name]++;
            }
        }

        std::vector<std::string> queue;
        for (auto& [name, degree] : inDegree)
        {
            if (degree == 0)
            {
                queue.push_back(name);
            }
        }

        std::vector<std::string> result;
        while (!queue.empty())
        {
            std::string current = queue.back();
            queue.pop_back();
            result.push_back(current);

            for (auto& neighbor : graph[current])
            {
                inDegree[neighbor]--;
                if (inDegree[neighbor] == 0)
                {
                    queue.push_back(neighbor);
                }
            }
        }

        std::vector<ModuleNode> sorted;
        for (auto& name : result)
        {
            sorted.push_back(m_nodes[m_nameToIndex[name]]);
        }
        return sorted;
    }
};

template<class T>
class Module : public ModuleBase
{
public:
    static T* Get()
    {
        static T s_instance;
        return &s_instance;
    }

protected:
    Module() = default;
};

struct ModuleBuilder
{
    std::string name;
    ModuleBase* module;
    std::vector<std::string> deps;
    int32_t order = 0;

    ModuleBuilder(const std::string& n, ModuleBase* m) : name(n), module(m) {}

    ~ModuleBuilder() { ModuleRegistry::Get().Register(name, module, deps, order); }

    ModuleBuilder& Depends(const std::string& dep)
    {
        deps.push_back(dep);
        return *this;
    }

    ModuleBuilder& UpdateOrder(int32_t o)
    {
        order = o;
        return *this;
    }
};

#define MODULE(Class) ModuleBuilder(#Class, Class::Get())
#define DEPENDS(Dep) .Depends(#Dep)
#define ORDER(Val) .UpdateOrder(Val)
