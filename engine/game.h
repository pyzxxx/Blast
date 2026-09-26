#pragma once

struct Services;

class Game {
public:
    virtual ~Game() = default;

    virtual bool init(const Services& services) = 0;
    virtual void update(float dt) = 0;
    virtual void shutdown() = 0;
};
