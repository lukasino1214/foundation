#include <application.hpp>

using namespace foundation;
auto main() -> i32 {
    PROFILE_SCOPE;
    Application app;
    app.run();
    return 0;
}