#include "application.hpp"

using namespace foundation;
auto main(i32 /* argc */, char** /* argv */) -> i32 {
    PROFILE_SCOPE;
    Application app;
    return app.run();
}