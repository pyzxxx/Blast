#pragma once

class Panel
{
public:
    Panel() = default;
    virtual ~Panel() = default;

    virtual const char* GetName() const = 0;
    virtual void DrawContent() = 0;
};