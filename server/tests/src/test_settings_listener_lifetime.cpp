#include "settings/native/native_settings_model.h"

#include <memory>
#include <stdexcept>
#include <string>

int main()
{
    native_settings::SettingsModel model([](const std::string &) {});
    int globalUpdates = 0;
    model.AddConfigListener([&globalUpdates]() { ++globalUpdates; });
    for (size_t scope = 1; scope <= 100; ++scope)
    {
        auto page = std::make_shared<std::string>(1024 * 1024, 'p');
        std::weak_ptr<std::string> released = page;
        model.SetListenerScope(scope);
        model.AddConfigListener([page]() {});
        model.AddMessageListener("probe", [page](const auto &) {});
        model.SetListenerScope(0);
        page.reset();
        model.RemoveListenerScope(scope);
        if (!released.expired())
            throw std::runtime_error("An evicted page is retained by a settings listener");
        model.Receive(native_settings::Json{{"type", "configSnapshot"}, {"data", {{"revision", scope}}}}.dump());
        model.Receive(R"({"type":"probe"})");
    }
    if (globalUpdates != 100)
        throw std::runtime_error("Eviction removed a window-level config listener");
    const std::string sameSnapshot = R"({"type":"configSnapshot","data":{"revision":100}})";
    for (size_t repeated = 0; repeated < 100; ++repeated)
        model.Receive(sameSnapshot);
    if (globalUpdates != 100)
        throw std::runtime_error("An unchanged settings snapshot rebuilt page previews");
    model.Update("appearance.candidate_skin", "fluent");
    if (globalUpdates != 101 || model.GetString("appearance.candidate_skin") != "fluent")
        throw std::runtime_error("An optimistic skin selection did not notify settings pages");
    model.Receive(R"({"type":"configSnapshot","data":{"revision":100,"appearance":{"candidate_skin":"wechat"}}})");
    if (globalUpdates != 102 || model.GetString("appearance.candidate_skin") != "wechat")
        throw std::runtime_error("An authoritative settings change was suppressed");
    return 0;
}
