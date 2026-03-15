#pragma once

#include "PCH.h"
#include "Math/TransformUtils.h"

class Node
{
public:
    Node();

    virtual ~Node();

    void SetParent(Node* newParent);

    Node* GetParent() { return m_parent; }

    const std::vector<Node*>& GetChildrens() { return m_childrens; }

    void SetLocalTranslation(const glm::vec3& pos);

    glm::vec3 GetLocalTranslation() { return m_translation; }

    void SetLocalScale(const glm::vec3& scale);

    glm::vec3 GetLocalScale() { return m_scale; }

    void SetLocalEuler(const glm::vec3& euler);

    glm::vec3 GetLocalEuler() { return m_euler; }

    glm::quat GetLocalRotation() { return m_rot; }

    void SetLocalTransform(const glm::mat4& localTransform);

    glm::mat4 GetLocalTransform();

    glm::mat4 GetWorldTransform();

    glm::vec3 GetWorldRight();

    glm::vec3 GetWorldUp();

    glm::vec3 GetWorldFront();

    glm::vec3 GetWorldTranslation();

protected:
    void UpdateTransform();

    void UpdateLocalTransform();

protected:
    glm::vec3 m_translation;
    glm::vec3 m_scale;
    glm::vec3 m_euler;
    glm::quat m_rot;
    glm::mat4 m_localTransform;
    glm::mat4 m_worldTransform;
    Node* m_parent = nullptr;
    std::vector<Node*> m_childrens;
};