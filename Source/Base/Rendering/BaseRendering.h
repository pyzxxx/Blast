#pragma once

#include "PCH.h"
#include "RHI/RHI.h"

class RenderContext;
template<class T> class DrawListWapper;

template<typename T>
void ExecuteDrawCall(RHI::CommandBuffer* cmd, const T& drawCall)
{
}

class DrawList
{
public:
    virtual ~DrawList() = default;
    virtual void Clear() = 0;
    virtual void Execute(RHI::CommandBuffer* cmd) = 0;
};

class DrawListRegister
{
public:
    static void ClearAll()
    {
        for (DrawList* list : GetAllLists())
        {
            list->Clear();
        }
    }

    static void AddList(DrawList* list)
    {
        GetAllLists().push_back(list);
    }

    static void RemoveList(DrawList* list)
    {
        auto& lists = GetAllLists();
        for (auto it = lists.begin(); it != lists.end(); ++it)
        {
            if (*it == list)
            {
                lists.erase(it);
                break;
            }
        }
    }

private:
    static std::vector<DrawList*>& GetAllLists()
    {
        static std::vector<DrawList*> lists;
        return lists;
    }
};

template<class T>
class DrawListWapper : public DrawList
{
public:
    using DrawCallType = T;

    void Add(const T& drawCall)
    {
        m_drawCalls.push_back(drawCall);
    }

    void Clear() override
    {
        m_drawCalls.clear();
    }

    void Execute(RHI::CommandBuffer* cmd) override
    {
        for (const auto& drawCall : m_drawCalls)
        {
            ExecuteDrawCall(cmd, drawCall);
        }
    }

private:
    std::vector<T> m_drawCalls;
};

#define DRAW_LIST_DECLARE(ListName, DrawCallType) \
    struct ListName : public DrawListWapper<DrawCallType> \
    { \
        ListName(); \
        ~ListName(); \
        static ListName* Get(); \
    };

#define DRAW_LIST_IMPLEMENT(ListName, DrawCallType) \
    ListName::ListName() { DrawListRegister::AddList(this); } \
    ListName::~ListName() { DrawListRegister::RemoveList(this); } \
    ListName* ListName::Get() \
    { \
        static ListName instance; \
        return &instance; \
    }