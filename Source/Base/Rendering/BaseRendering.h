#pragma once

#include <vector>

class DrawList;
template<class T> class DrawListWapper;

class DrawListRegister
{
public:
    template<typename T>
    static DrawListWapper<T>* GetList()
    {
        static DrawListWapper<T> instance;
        return &instance;
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

class DrawList
{
public:
    virtual ~DrawList() = default;
    virtual void Clear() = 0;
};

template<class T>
class DrawListWapper : public DrawList
{
public:
    DrawListWapper()
    {
        DrawListRegister::AddList(this);
    }

    ~DrawListWapper() override
    {
        DrawListRegister::RemoveList(this);
    }

    void Add(const T& dc)
    {
        m_drawCalls.push_back(dc);
    }

    void Clear() override
    {
        m_drawCalls.clear();
    }

private:
    std::vector<T> m_drawCalls;
};

#define REGISTER_DRAW_LIST(type) auto s_drawList##type = DrawListRegister::GetList<type>();