#include "CmdChecker.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <utility>
#include <nitro_utils/string_utils.h>

namespace
{
    std::string_view TrimAscii(std::string_view value)
    {
        while (!value.empty() && static_cast<unsigned char>(value.front()) <= ' ')
            value.remove_prefix(1);
        while (!value.empty() && static_cast<unsigned char>(value.back()) <= ' ')
            value.remove_suffix(1);
        return value;
    }

    bool IsAllowedLocalExec(std::string_view arguments)
    {
        arguments = TrimAscii(arguments);
        if (arguments.empty())
            return false;

        if (arguments.front() == '"')
        {
            if (arguments.size() < 3 || arguments.back() != '"')
                return false;
            arguments.remove_prefix(1);
            arguments.remove_suffix(1);
        }
        else
        {
            // CFG paths used by the game contain no spaces. Extra tokens are
            // therefore malformed and must not be silently ignored.
            for (const char ch : arguments)
            {
                if (static_cast<unsigned char>(ch) <= ' ')
                    return false;
            }
        }

        std::string normalized(arguments);
        for (char& ch : normalized)
        {
            if (ch == '\\')
                ch = '/';
            else
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        if (normalized.empty() || normalized.front() == '/' ||
            normalized.find(':') != std::string::npos ||
            normalized.find("..") != std::string::npos ||
            normalized.find("//") != std::string::npos)
        {
            return false;
        }

        static constexpr auto kAllowedFiles = std::to_array<std::string_view>({
            "config.cfg",
            "userconfig.cfg",
            "autoexec.cfg",
            "listenserver.cfg",
            "server.cfg",
            "valve.rc",
            "skill.cfg",
            "game.cfg",
            "joystick.cfg",
            "language.cfg",
            "violence.cfg",
            "default/config.cfg",
            "hw/opengl.cfg",
            "hw/3dfxvoodoo1.cfg",
            "hw/3dfxvoodoo2.cfg",
            "hw/3dfx.cfg",
            "hw/riva128.cfg",
            "hw/rivatnt.cfg",
            "hw/geforce.cfg",
            "hw/powervrpcx2.cfg",
            "hw/powervrsg.cfg",
            "hw/v2200.cfg",
            "hw/3dlabs.cfg",
            "hw/matrox.cfg",
            "hw/atirage128.cfg",
            "hw/g200d3d.cfg",
            "hw/atirage128d3d.cfg",
            "hw/nvidiad3d.cfg",
        });

        if (std::find(kAllowedFiles.begin(), kAllowedFiles.end(), normalized) != kAllowedFiles.end())
            return true;

        // Stock-compatible per-map configuration, limited to one plain file
        // directly under maps/. Its commands still pass through this filter.
        constexpr std::string_view kMapsPrefix = "maps/";
        constexpr std::string_view kCfgSuffix = ".cfg";
        if (normalized.starts_with(kMapsPrefix) && normalized.ends_with(kCfgSuffix))
        {
            const std::string_view filename(normalized.data() + kMapsPrefix.size(),
                                            normalized.size() - kMapsPrefix.size() - kCfgSuffix.size());
            return !filename.empty() && filename.find('/') == std::string_view::npos;
        }

        return false;
    }

    std::string_view StripQuotesAndAscii(std::string_view value)
    {
        value = TrimAscii(value);
        if (value.size() >= 2)
        {
            const char f = value.front();
            const char b = value.back();
            if ((f == '"' && b == '"') || (f == 39 && b == 39))
            {
                value.remove_prefix(1);
                value.remove_suffix(1);
                value = TrimAscii(value);
            }
        }
        return value;
    }

    bool EqualsIgnoreCase(std::string_view a, std::string_view b)
    {
        return nitro_utils::equals(a, b, nitro_utils::CompareOptions::RegisterIndependent);
    }

    bool IsAllowedAliasName(std::string_view alias_name)
    {
        alias_name = StripQuotesAndAscii(alias_name);
        if (alias_name.empty())
            return false;

        static constexpr auto kWhitelistedAliases = std::to_array<std::string_view>({
            // Host match & round management
            "r",
            "warm",
            "warmup",
            "mix",
            "1v1",
            "ff0",
            "ff1",
            "fr0",
            "fr1",
            "fr2",
            "fr3",
            "fr4",
            "fr5",
            "fr6",
            "fr7",
            "fr8",
            "fr9",
            "fr10",
            "fr11",
            "fr12",
            "w5",
            "lv",
            "live",

            // Client convenience shortcuts
            "d",
            "q",
            "ret",

            // LAN host profiles
            "gl_lan_perf",
            "gl_lan_quality",
            "gl_lan_debug",
        });

        for (const auto& allowed : kWhitelistedAliases)
        {
            if (EqualsIgnoreCase(alias_name, allowed))
                return true;
        }

        return false;
    }
}

CmdChecker::CmdChecker(
    std::shared_ptr<CommandLoggerInterface> cmd_logger,
    std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider
) :
    cmd_logger_(std::move(cmd_logger))
{
    InitBlockedCommands(config_provider);
}

void CmdChecker::FilterCmd(const std::string_view& cmd, CommandSource command_source, std::string& out)
{
    out.clear();

    if (cmd.empty())
    {
        return;
    }

    if (cmd.length() == 1 && cmd[0] == '\n')
    {
        out = '\n';
        return;
    }

    size_t cur_pos = 0;
    SplitData split_data;

    while (GetNextSplitToken(cmd, [](char ch) { return ch == ';'; }, &cur_pos, split_data))
    {
        if (split_data.token.empty())
        {
            continue;
        }

        bool is_cmd_allowed = FilterSingleCmd(split_data.token, command_source, out);
        if (is_cmd_allowed && split_data.has_delimiter())
        {
            out += split_data.delimiter;
        }

        LogCmd(is_cmd_allowed, split_data.token, command_source);
    }

    if (IsCommandFromServer(command_source) && !out.empty() && !out.ends_with('\n') && !out.ends_with(';'))
    {
        out += ';';
    }
}

bool CmdChecker::FilterSingleCmd(const std::string_view& cmd, CommandSource command_source, std::string& out)
{
    size_t pos = 0;
    SplitData first_cmd_token;
    if (!GetNextSplitToken(cmd, [](char ch) { return ch <= ' ' || ch == ':'; }, &pos, first_cmd_token))
    {
        out.append(cmd);
        return true;
    }

    const std::string_view cmd_name = StripQuotesAndAscii(first_cmd_token.token);

    if (nitro_utils::contains(cmd_name, "dlfile", nitro_utils::CompareOptions::RegisterIndependent))
    {
        return false;
    }

    // Direct cheat commands: blocked under all circumstances
    if (EqualsIgnoreCase(cmd_name, "god") ||
        EqualsIgnoreCase(cmd_name, "noclip") ||
        EqualsIgnoreCase(cmd_name, "notarget") ||
        EqualsIgnoreCase(cmd_name, "fly"))
    {
        return false;
    }

    // sv_cheats and sv_cheat: can NEVER be set to non-zero under any state
    if (EqualsIgnoreCase(cmd_name, "sv_cheats") || EqualsIgnoreCase(cmd_name, "sv_cheat"))
    {
        size_t arg_pos = pos;
        SplitData arg_token;
        if (GetNextSplitToken(cmd, [](char ch) { return ch <= ' '; }, &arg_pos, arg_token))
        {
            std::string_view arg = StripQuotesAndAscii(arg_token.token);
            if (!arg.empty() && arg != "0" && arg != "0.0" && arg != "0.000000")
            {
                return false;
            }
        }
    }

    // impulse: only allow legitimate gameplay impulses (100 = flashlight, 201 = spray logo, 0 = clear)
    // Block impulse 101 (give money/weapons), impulse 102, etc.
    if (EqualsIgnoreCase(cmd_name, "impulse"))
    {
        size_t arg_pos = pos;
        SplitData arg_token;
        if (GetNextSplitToken(cmd, [](char ch) { return ch <= ' '; }, &arg_pos, arg_token))
        {
            std::string_view arg = StripQuotesAndAscii(arg_token.token);
            if (!arg.empty() && arg != "100" && arg != "201" && arg != "0")
            {
                return false;
            }
        }
    }

    // alias: users can NEVER define custom aliases, only inspect or use whitelisted aliases
    if (EqualsIgnoreCase(cmd_name, "alias"))
    {
        size_t alias_pos = pos;
        SplitData alias_token;
        if (GetNextSplitToken(cmd, [](char ch) { return ch <= ' '; }, &alias_pos, alias_token))
        {
            std::string_view target_alias = StripQuotesAndAscii(alias_token.token);

            while (alias_pos < cmd.size() && static_cast<unsigned char>(cmd[alias_pos]) <= ' ')
                alias_pos++;

            if (alias_pos < cmd.size())
            {
                // This command attempts to define or redefine an alias!
                if (!IsAllowedAliasName(target_alias))
                {
                    return false;
                }
            }
        }
    }

    // Do not persist a protected server cvar, cheat command, or custom alias inside a key binding.
    const bool is_bind = cmd_name.size() == 4 &&
        nitro_utils::contains(cmd_name, "bind", nitro_utils::CompareOptions::RegisterIndependent);
    if (is_bind)
    {
        if (nitro_utils::contains(cmd, "sv_clienttrace", nitro_utils::CompareOptions::RegisterIndependent) ||
            nitro_utils::contains(cmd, "sv_cheats", nitro_utils::CompareOptions::RegisterIndependent) ||
            nitro_utils::contains(cmd, "sv_cheat", nitro_utils::CompareOptions::RegisterIndependent) ||
            nitro_utils::contains(cmd, "impulse 101", nitro_utils::CompareOptions::RegisterIndependent))
        {
            return false;
        }
    }

    const bool is_exec = cmd_name.size() == 4 &&
        nitro_utils::contains(cmd_name, "exec", nitro_utils::CompareOptions::RegisterIndependent);
    if (is_exec)
    {
        // Remote exec is never legitimate. Local exec is restricted to the
        // engine/client's known configuration files; their contents are still
        // filtered again when inserted into the command buffer.
        if (IsCommandFromServer(command_source) || !IsAllowedLocalExec(cmd.substr(pos)))
            return false;
    }

    auto it = blocked_commands_.find(cmd_name);
    if (it == blocked_commands_.end())
    {
        it = blocked_commands_.find(first_cmd_token.token);
    }
    if (it == blocked_commands_.end())
    {
        out.append(cmd);
        return true;
    }

    CmdBlockType block_type = it->second;
    if ((block_type == CmdBlockType::Any) || ((block_type == CmdBlockType::OnlyServer) && IsCommandFromServer(command_source)))
    {
        return false;
    }

    out.append(cmd);
    return true;
}

void CmdChecker::LogCmd(bool is_cmd_allowed, const std::string_view& cmd, CommandSource command_source)
{
    std::string_view log_cmd_name;
    std::string_view log_cmd_value;
    TokenizeCmdForLogger(cmd, log_cmd_name, log_cmd_value);

    if (is_cmd_allowed)
    {
        if (IsCommandFromServer(command_source) && !log_cmd_name.empty())
        {
            std::string log_cmd_name_string(log_cmd_name);
            std::string log_cmd_value_string(log_cmd_value);

            cmd_logger_->LogCommand(log_cmd_name_string.c_str(), log_cmd_value_string.c_str(), LogCommandType::AllowServerCommand);
        }
    }
    else
    {
        if (!log_cmd_name.empty())
        {
            std::string log_cmd_name_string(log_cmd_name);
            std::string log_cmd_value_string(log_cmd_value);

            LogCommandType log_command = GetBlockedLogCommandType(command_source);
            cmd_logger_->LogCommand(log_cmd_name_string.c_str(), log_cmd_value_string.c_str(), log_command);
        }
    }
}

void CmdChecker::InitBlockedCommands(std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider)
{
    auto blocked_commands = config_provider->get_all_values("cmd_filter");
    if (!blocked_commands)
    {
        return;
    }

    for (const auto& [key, value] : *blocked_commands)
    {
        CmdBlockType block_type;

        if (value == "0")
        {
            block_type = CmdBlockType::Any;
        }
        else if (value == "1" || value == "2")
        {
            block_type = CmdBlockType::OnlyServer;
        }
        else
        {
            continue;
        }

        blocked_commands_[key] = block_type;
    }
}

bool CmdChecker::GetNextSplitToken(
    const std::string_view& text,
    const std::function<bool(char)>& is_delim,
    size_t* cur_pos,
    SplitData& split_data
)
{
    size_t text_length = text.length();

    size_t token_start = 0;
    size_t token_length = 0;

    char delimiter = 0;
    size_t i = 0;

    for (i = *cur_pos; i < text_length;)
    {
        token_start = i;
        delimiter = 0;

        size_t quotes = 0;

        for (; i < text_length; i++)
        {
            if (text[i] == '"')
            {
                quotes++;
            }

            if (quotes % 2 == 0 && is_delim(text[i]))
            {
                delimiter = text[i];
                break; // don't break if inside a quoted string
            }

            if (text[i] == '\n')
            {
                delimiter = '\n';
                break;
            }
        }

        token_length = i - token_start;

        // Necessarily step over \n or other delimiter
        if (i < text_length && (text[i] == '\n' || is_delim(text[i])))
        {
            i++;
        }

        if (token_length > 0)
        {
            break;
        }
    }

    *cur_pos = i;
    if (token_length > 0)
    {
        split_data.token = std::string_view(text).substr(token_start, token_length);
        split_data.delimiter = delimiter;

        return true;
    }
    return false;
}

bool CmdChecker::TokenizeCmdForLogger(const std::string_view& text, std::string_view& name, std::string_view& value)
{
    size_t cur_pos = 0;
    SplitData split_data;

    if (GetNextSplitToken(text, [](char ch) { return ch <= ' ' || ch == ':'; }, &cur_pos, split_data))
    {
        name = split_data.token;
        value = std::string_view(text).substr(cur_pos);

        return true;
    }

    return false;
}

bool CmdChecker::IsCommandFromServer(CommandSource command_source)
{
    return command_source != CommandSource::Console;
}

LogCommandType CmdChecker::GetBlockedLogCommandType(CommandSource command_source)
{
    LogCommandType log_command;

    switch (command_source)
    {
        case CommandSource::Stufftext:
            log_command = LogCommandType::BlockedStufftextCommand;
            break;

        case CommandSource::Director:
            log_command = LogCommandType::BlockedDirectorCommand;
            break;

        default:
            log_command = LogCommandType::BlockedAllCommand;
            break;
    }

    return log_command;
}
