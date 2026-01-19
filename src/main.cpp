#include "application.hpp"

using namespace foundation;
auto main(int argc, char **argv) -> i32 {
    PROFILE_SCOPE;
    Application app;
    return app.run();
}