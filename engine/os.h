#pragma once

#include "input_event.h"

#include <cstdint>

class OS {
public:
    virtual ~OS() = default;

    virtual void poll_events() = 0;
    virtual bool should_quit() const = 0;
    virtual void window_size(uint32_t& w, uint32_t& h) const = 0;
    virtual double time_seconds() const = 0;

    virtual bool poll_input_event(InputEvent& out) = 0;

    virtual void* native_handle() const = 0;

    virtual const char* asset_root() const = 0;
};
