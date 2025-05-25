#pragma once

namespace foundation {
    struct UIWindow {
        virtual ~UIWindow() = default;

        virtual void draw() = 0;
        virtual auto should_close() -> bool = 0;
    };
}