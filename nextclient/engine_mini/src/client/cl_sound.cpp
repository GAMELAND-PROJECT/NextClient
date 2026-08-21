#include "../engine.h"
#include "../common/zone.h"
#include "../common/com_strings.h"
#include "cl_sound.h"
#include "optick.h"
#include "ncl_entity/PlayerSyncSystem.h"
#include "ncl_entity/WeaponSyncSystem.h"
#include "ncl_entity/cl_ncl_entity_sync.h"

#include <format>
#include <iterator>

namespace
{
    bool HasSoundPrefix(const char* name, const char* prefix)
    {
        return name != nullptr && Q_strnicmp(name, prefix, Q_strlen(prefix)) == 0;
    }

    bool IsMutedGameSound(const sfx_t* sfx)
    {
        if (sfx == nullptr)
        {
            return false;
        }

        const char* name = sfx->name;
        if (HasSoundPrefix(name, "sound/"))
        {
            name += Q_strlen("sound/");
        }

        return HasSoundPrefix(name, "ambience/") ||
               HasSoundPrefix(name, "ambient/") ||
               HasSoundPrefix(name, "radio/") ||
               HasSoundPrefix(name, "hostage/") ||
               HasSoundPrefix(name, "vox/") ||
               Q_stristr(name, "MRAD_") != nullptr;
    }

    sfx_t* GetReplacementSfx(int entnum, int entchannel, sfx_t* sfx)
    {
        if (entchannel != CHAN_WEAPON && entchannel != CHAN_ITEM)
        {
            return nullptr;
        }

        PlayerSyncSystem* player_sync =
            static_cast<PlayerSyncSystem*>(CL_NclEntitySyncGetHandler(static_cast<uint8_t>(ncl_entity::EntityTypeId::Player)));

        WeaponSyncSystem* weapon_sync =
            static_cast<WeaponSyncSystem*>(CL_NclEntitySyncGetHandler(static_cast<uint8_t>(ncl_entity::EntityTypeId::Weapon)));

        if (player_sync == nullptr || weapon_sync == nullptr)
        {
            return nullptr;
        }

        if (cls == nullptr || cl == nullptr || cls->state != ca_active)
        {
            return nullptr;
        }

        if (entnum < 1 || entnum > MAX_CLIENTS)
        {
            return nullptr;
        }

        int active_weapon_index = player_sync->GetPlayerActiveWeapon(entnum);
        if (active_weapon_index == 0)
        {
            return nullptr;
        }

        const auto& overrides = weapon_sync->GetOverrides(active_weapon_index);

        auto replace_it = overrides.find(sfx);
        if (replace_it == overrides.end())
        {
            return nullptr;
        }

        return replace_it->second;
    }
} // namespace

sfx_t* S_PrecacheSound(char* sample)
{
    OPTICK_EVENT();

    return eng()->S_PrecacheSound(sample);
}

void S_StartDynamicSoundHook(
    int entnum,
    int entchannel,
    sfx_t* sfx,
    vec_t* origin,
    float fvol,
    float attenuation,
    int flags,
    int pitch,
    S_StartDynamicSoundChain* next
)
{
    OPTICK_EVENT();

    // Network voice owns dedicated channels and must never enter weapon/item
    // replacement logic. Keeping this path transparent avoids interference
    // with the engine's streaming voice cache and decoder lifecycle.
    if (entchannel >= CHAN_NETWORKVOICE_BASE && entchannel <= CHAN_NETWORKVOICE_END)
    {
        next->Invoke(entnum, entchannel, sfx, origin, fvol, attenuation, flags, pitch);
        return;
    }

    if (IsMutedGameSound(sfx))
    {
        return;
    }

    sfx_t* replacement_sfx = GetReplacementSfx(entnum, entchannel, sfx);
    if (replacement_sfx)
    {
        next->Invoke(entnum, entchannel, replacement_sfx, origin, fvol, attenuation, flags, pitch);
        return;
    }

    next->Invoke(entnum, entchannel, sfx, origin, fvol, attenuation, flags, pitch);
}

void S_StartStaticSoundHook(
    int entnum,
    int entchannel,
    sfx_t* sfx,
    vec_t* origin,
    float fvol,
    float attenuation,
    int flags,
    int pitch,
    S_StartStaticSoundChain* next
)
{
    OPTICK_EVENT();

    // Static channels are map ambience (loops, machinery and environmental
    // emitters). Suppress them at creation instead of continuously stopping
    // channels, which keeps the mixer and network voice paths untouched.
    if (IsMutedGameSound(sfx) || entchannel == CHAN_STATIC)
    {
        return;
    }

    next->Invoke(entnum, entchannel, sfx, origin, fvol, attenuation, flags, pitch);
}

void S_StopAllSounds(qboolean clear)
{
    OPTICK_EVENT();

    eng()->S_StopAllSounds.InvokeChained(clear);
}

void S_UnloadSounds(const std::unordered_set<std::string>& names)
{
    OPTICK_EVENT();

    if (p_known_sfx == nullptr || p_num_sfx == nullptr || *p_known_sfx == nullptr)
    {
        return;
    }

    sfx_t* known_sfx = *p_known_sfx;
    int num_sfx = *p_num_sfx;

    std::string buf;

    for (int i = 0; i < num_sfx; i++)
    {
        sfx_t* sfx = &known_sfx[i];

        buf.clear();
        std::format_to(std::back_inserter(buf), "{}{}", DEFAULT_SOUNDPATH, sfx->name);

        if (!names.contains(buf))
        {
            continue;
        }

        if (Cache_Check(&sfx->cache))
        {
            Cache_Free(&sfx->cache);
        }

        sfx->cache.data = nullptr;
        sfx->name[0] = '\0';
    }
}
