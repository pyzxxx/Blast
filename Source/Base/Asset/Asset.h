#pragma once

#include <string>

class Asset
{
public:
    Asset(const std::string& assetPath = "") {}

    virtual ~Asset() {}
};