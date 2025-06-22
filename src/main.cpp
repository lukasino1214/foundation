#include <application.hpp>

#if defined(LPP_ENABLED)
#include "LPP_API_x64_CPP.h"
#endif

using namespace foundation;
auto main() -> i32 {
    PROFILE_SCOPE;

#if defined(LPP_ENABLED)
    std::wstring text_wchar(std::strlen(LPP_PATH), L'#');
    std::mbstowcs( text_wchar.data(), LPP_PATH, text_wchar.size());

    lpp::LppDefaultAgent lppAgent = lpp::LppCreateDefaultAgent(nullptr, text_wchar.c_str());
    if (!lpp::LppIsValidDefaultAgent(&lppAgent)) { return 1; }

    lppAgent.EnableModule(lpp::LppGetCurrentModulePath(), lpp::LPP_MODULES_OPTION_ALL_IMPORT_MODULES, nullptr, nullptr);
#endif
    // Logger::init();

    Application app;
    app.run();

#if defined(LPP_ENABLED)
    lpp::LppDestroyDefaultAgent(&lppAgent);
#endif

    return 0;
}