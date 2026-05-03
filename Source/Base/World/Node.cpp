#include "Node.h"

Node::Node() {}

Node::~Node()
{
    SetParent(nullptr);

    std::vector<Node*> removeChildrens = m_childrens;
    for (int i = 0; i < removeChildrens.size(); ++i)
    {
        removeChildrens[i]->SetParent(nullptr);
    }
    m_childrens.clear();
}

void Node::SetParent(Node* newParent)
{
    Node* oldParent = m_parent;

    if (oldParent == newParent)
    {
        return;
    }

    if (oldParent)
    {
        for (auto iter = oldParent->m_childrens.begin(); iter != oldParent->m_childrens.end(); iter++)
        {
            if (iter == oldParent->m_childrens.end())
            {
                break;
            }

            if ((*iter) == this)
            {
                oldParent->m_childrens.erase(iter);
                break;
            }
        }
    }

    m_parent = newParent;
    if (m_parent)
    {
        m_parent->m_childrens.push_back(this);
    }

    UpdateTransform();
}

void Node::SetLocalTranslation(const glm::vec3& pos)
{
    m_translation = pos;
    UpdateLocalTransform();
    UpdateTransform();
}

void Node::SetLocalScale(const glm::vec3& scale)
{
    m_scale = scale;
    UpdateLocalTransform();
    UpdateTransform();
}

void Node::SetLocalEuler(const glm::vec3& euler)
{
    m_euler = euler;
    m_rot = glm::quat(euler);
    UpdateLocalTransform();
    UpdateTransform();
}

void Node::SetLocalTransform(const glm::mat4& localTransform)
{
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(localTransform, m_scale, m_rot, m_translation, skew, perspective);
    m_euler = glm::eulerAngles(m_rot) * 3.14159f / 180.f;

    m_localTransform = localTransform;
    UpdateTransform();
}

glm::mat4 Node::GetLocalTransform() { return m_localTransform; }

glm::mat4 Node::GetWorldTransform() { return m_worldTransform; }

glm::vec3 Node::GetWorldRight()
{
    return glm::normalize(glm::vec3(m_worldTransform[0][0], m_worldTransform[0][1], m_worldTransform[0][2]));
}

glm::vec3 Node::GetWorldUp()
{
    return glm::normalize(glm::vec3(m_worldTransform[1][0], m_worldTransform[1][1], m_worldTransform[1][2]));
}

glm::vec3 Node::GetWorldFront()
{
    // The camera looks towards -z
    return glm::normalize(-glm::vec3(m_worldTransform[2][0], m_worldTransform[2][1], m_worldTransform[2][2]));
}

glm::vec3 Node::GetWorldTranslation()
{
    glm::vec3 ret(m_worldTransform[3][0], m_worldTransform[3][1], m_worldTransform[3][2]);
    return ret;
}

void Node::UpdateTransform()
{
    m_worldTransform = m_localTransform;
    if (m_parent)
    {
        m_worldTransform = m_parent->m_worldTransform * m_localTransform;
    }

    DirtyTransform();

    for (auto& child : m_childrens)
    {
        child->UpdateTransform();
    }
}

void Node::UpdateLocalTransform()
{
    glm::mat4 r, t, s;
    r = glm::toMat4(m_rot);
    t = glm::translate(glm::mat4(1.0), m_translation);
    s = glm::scale(glm::mat4(1.0), m_scale);
    m_localTransform = t * r * s;
}