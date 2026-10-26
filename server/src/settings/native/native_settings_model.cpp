#include "settings/native/native_settings_model.h"

#include "engine/contracts/webview/schema.h"

#include <algorithm>

namespace native_settings
{
namespace
{
const Json *Walk(const Json &root, std::string_view path)
{
    const Json *node = &root;
    while (!path.empty())
    {
        const size_t dot = path.find('.');
        const std::string key(path.substr(0, dot));
        if (!node->is_object())
            return nullptr;
        const auto it = node->find(key);
        if (it == node->end())
            return nullptr;
        node = &*it;
        path = dot == std::string_view::npos ? std::string_view() : path.substr(dot + 1);
    }
    return node;
}
} // namespace

SettingsModel::SettingsModel(Sender sender) : sender_(std::move(sender))
{
}

const Json &SettingsModel::Config() const
{
    return config_;
}

bool SettingsModel::HasConfig() const
{
    return hasConfig_;
}

const Json *SettingsModel::Find(std::string_view path) const
{
    const Json *node = Walk(config_, path);
    return node && !node->is_null() ? node : nullptr;
}

bool SettingsModel::GetBool(std::string_view path, bool fallback) const
{
    const Json *node = Find(path);
    return node && node->is_boolean() ? node->get<bool>() : fallback;
}

std::string SettingsModel::GetString(std::string_view path, const std::string &fallback) const
{
    const Json *node = Find(path);
    return node && node->is_string() ? node->get<std::string>() : fallback;
}

int64_t SettingsModel::GetInt(std::string_view path, int64_t fallback) const
{
    const Json *node = Find(path);
    if (!node || !node->is_number())
        return fallback;
    return node->is_number_float() ? static_cast<int64_t>(node->get<double>()) : node->get<int64_t>();
}

double SettingsModel::GetNumber(std::string_view path, double fallback) const
{
    const Json *node = Find(path);
    return node && node->is_number() ? node->get<double>() : fallback;
}

void SettingsModel::Update(std::string_view path, Json value)
{
    // Write into the local copy first, creating the section objects on the way.
    Json *node = &config_;
    std::string_view rest = path;
    while (true)
    {
        const size_t dot = rest.find('.');
        const std::string key(rest.substr(0, dot));
        if (!node->is_object())
            *node = Json::object();
        if (dot == std::string_view::npos)
        {
            (*node)[key] = value;
            break;
        }
        node = &(*node)[key];
        rest = rest.substr(dot + 1);
    }
    Post("configUpdate", Json{{"path", std::string(path)}, {"value", std::move(value)}});
    NotifyConfig();
}

void SettingsModel::Post(const std::string &type, Json data)
{
    Json message = {{"type", type}, {"protocolVersion", metasequoia::webview::Version}};
    if (!data.is_null())
        message["data"] = std::move(data);
    if (sender_)
        sender_(message.dump());
}

std::string SettingsModel::NewRequestId(std::string_view prefix)
{
    return std::string(prefix) + "-" + std::to_string(nextRequestId_++);
}

void SettingsModel::AddConfigListener(ConfigListener listener)
{
    configListeners_.emplace_back(listenerScope_, std::move(listener));
}

void SettingsModel::AddMessageListener(const std::string &type, MessageListener listener)
{
    messageListeners_[type].emplace_back(listenerScope_, std::move(listener));
}

void SettingsModel::SetListenerScope(size_t scope)
{
    listenerScope_ = scope;
}

void SettingsModel::RemoveListenerScope(size_t scope)
{
    configListeners_.erase(std::remove_if(configListeners_.begin(), configListeners_.end(),
                                          [scope](const auto &entry) { return entry.first == scope; }),
                           configListeners_.end());
    for (auto iterator = messageListeners_.begin(); iterator != messageListeners_.end();)
    {
        auto &listeners = iterator->second;
        listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                                       [scope](const auto &entry) { return entry.first == scope; }),
                        listeners.end());
        if (listeners.empty())
            iterator = messageListeners_.erase(iterator);
        else
            ++iterator;
    }
}

void SettingsModel::Receive(const std::string &serializedMessage)
{
    Json message = Json::parse(serializedMessage, nullptr, false);
    if (!message.is_object())
        return;
    const std::string type = message.value("type", std::string());
    if (type == "configSnapshot")
    {
        if (message.contains("data") && message["data"].is_object())
        {
            config_ = std::move(message["data"]);
            hasConfig_ = true;
            NotifyConfig();
        }
        return;
    }
    const auto it = messageListeners_.find(type);
    if (it == messageListeners_.end())
        return;
    // Copy: a listener may register more listeners while it runs.
    const auto listeners = it->second;
    for (const auto &listener : listeners)
    {
        const size_t previousScope = listenerScope_;
        listenerScope_ = listener.first;
        listener.second(message);
        listenerScope_ = previousScope;
    }
}

void SettingsModel::NotifyConfig()
{
    // A listener that updates the model re-enters here; finish the current pass first.
    if (notifying_)
    {
        notifyAgain_ = true;
        return;
    }
    notifying_ = true;
    do
    {
        notifyAgain_ = false;
        for (size_t i = 0; i < configListeners_.size(); ++i)
        {
            const auto listener = configListeners_[i].second;
            const size_t previousScope = listenerScope_;
            listenerScope_ = configListeners_[i].first;
            listener();
            listenerScope_ = previousScope;
        }
    } while (notifyAgain_);
    notifying_ = false;
}
} // namespace native_settings
