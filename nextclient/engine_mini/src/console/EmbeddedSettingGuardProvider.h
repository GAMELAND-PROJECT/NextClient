#pragma once
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <nitro_utils/config/ConfigProviderInterface.h>
#include <nitro_utils/config/FileConfigProvider.h>

class EmbeddedSettingGuardProvider : public nitro_utils::ConfigProviderInterface
{
private:
    std::shared_ptr<nitro_utils::FileConfigProvider> disk_provider_;

public:
    explicit EmbeddedSettingGuardProvider(const std::string& ini_path = "setting_guard.ini");

    [[nodiscard]] std::optional<std::vector<std::string>> get_list(const std::string& list_section) override;
    [[nodiscard]] std::optional<std::string> get_value(const std::string& key_value_section, const std::string& key) override;
    [[nodiscard]] std::optional<nitro_utils::transparent_string_map<std::string>> get_all_values(const std::string& key_value_section) override;
};
