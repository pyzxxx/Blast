#include "UUID.h"

#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace {
static std::mt19937& GetGenerator()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

static std::uniform_int_distribution<uint16_t>& GetHexDist()
{
    static std::uniform_int_distribution<uint16_t> dist(0, 15);
    return dist;
}

static std::uniform_int_distribution<uint16_t>& GetVariantDist()
{
    static std::uniform_int_distribution<uint16_t> dist(8, 11);
    return dist;
}
}// namespace

std::string UUIDGenerator::Generate()
{
    auto& gen = GetGenerator();
    auto& hexDist = GetHexDist();
    auto& variantDist = GetVariantDist();

    std::stringstream ss;

    ss << std::hex << std::setfill('0');

    for (int i = 0; i < 32; i++)
    {
        if (i == 8 || i == 12 || i == 16 || i == 20)
        {
            ss << "-";
        }

        int n;
        if (i == 12)
        {
            n = 4;// UUID version 4
        }
        else if (i == 16)
        {
            n = variantDist(gen);// variant bits (8-11)
        }
        else
        {
            n = hexDist(gen);// random hex digit
        }

        ss << n;
    }

    std::string result = ss.str();

    return result;
}