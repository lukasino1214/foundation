#include <application.hpp>

using namespace foundation;
auto main() -> i32 {
    ZoneScoped;
    Application app;
    app.run();
    return 0;
}