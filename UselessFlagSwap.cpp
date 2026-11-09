// UselessFlagSwap.cpp
//
// BZFS plugin: makes the Useless (US) flag useful.
//
//  * Registered players who grab a Useless flag can type /use <flag> to trade
//    it for any flag that exists on the current map (minus bad flags, team
//    flags and a few excluded flags).
//  * Unregistered players who grab a Useless flag are told that registering
//    unlocks this feature.
//  * Unregistered players get a short welcome notice ~10 seconds after joining.
//
// No config file needed. Load with:
//     -loadplugin /path/to/UselessFlagSwap
//
// Written against the BZFS 2.4.x plugin API. Check names against your
// bzfsAPI.h if anything fails to compile.

#include "bzfsAPI.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <string>

static const char*  REGISTER_URL  = "https://forums.bzflag.org/";
static const double WELCOME_DELAY = 10.0;   // seconds after join

class UselessFlagSwap : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name() { return "Useless Flag Swap"; }
    virtual void Init(const char* config);
    virtual void Cleanup();
    virtual void Event(bz_EventData* eventData);
    virtual bool SlashCommand(int playerID, bz_ApiString command,
                              bz_ApiString message, bz_APIStringList* params);

private:
    std::set<std::string> allowed;          // flags currently offered
    std::string           allowedList;      // "SW,ST,L,..." in map order
    std::map<int, double> pendingWelcome;   // playerID -> time to send welcome

    void refreshAllowed();
    bool isRegistered(int playerID);
    void sendRegisterPitch(int playerID, const char* intro);
};

BZ_PLUGIN(UselessFlagSwap)

// ---------------------------------------------------------------------------

static bool isUseless(const char* abbr)
{
    return abbr && std::strcmp(abbr, "US") == 0;
}

void UselessFlagSwap::Init(const char* /*config*/)
{
    Register(bz_eFlagGrabbedEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPartEvent);
    Register(bz_eTickEvent);

    bz_registerCustomSlashCommand("use", this);
}

void UselessFlagSwap::Cleanup()
{
    Flush();
    bz_removeCustomSlashCommand("use");
    pendingWelcome.clear();
}

// Build the list of flags players may trade for, from the flags that
// actually exist in the running world.
void UselessFlagSwap::refreshAllowed()
{
    static const char* excludedArr[] = {
        "US",                                    // Useless: don't swap into another one
        "G",                                     // Genocide: too disruptive to hand out
        "B", "BY", "CB", "FO", "JM", "LT", "M",  // bad flags
        "NJ", "O", "RC", "RO", "RT", "TR", "WA"
    };
    static const std::set<std::string> excluded(
        excludedArr, excludedArr + sizeof(excludedArr) / sizeof(excludedArr[0]));

    allowed.clear();
    allowedList.clear();

    int count = (int)bz_getNumFlags();
    for (int i = 0; i < count; i++)
    {
        bz_ApiString nameStr = bz_getFlagName(i);   // returns a bz_ApiString, not a char*
        const char* name = nameStr.c_str();
        if (!name || !*name) continue;

        std::string abbr = name;
        if (abbr[abbr.size() - 1] == '*') continue;   // team flags (R*, G*, B*, P*)
        if (excluded.count(abbr))         continue;   // excluded / bad flags

        if (allowed.insert(abbr).second)
        {
            if (!allowedList.empty()) allowedList += ",";
            allowedList += abbr;
        }
    }
}

// "Registered" = authenticated via global login.
bool UselessFlagSwap::isRegistered(int playerID)
{
    bz_BasePlayerRecord* pr = bz_getPlayerByIndex(playerID);
    if (!pr) return false;
    bool ok = pr->verified;
    bz_freePlayerRecord(pr);
    return ok;
}

void UselessFlagSwap::sendRegisterPitch(int playerID, const char* intro)
{
    bz_sendTextMessage(BZ_SERVER, playerID, intro);
    bz_sendTextMessage(BZ_SERVER, playerID,
        bz_format("Register for free at %s and log in with your callsign to unlock it!",
                  REGISTER_URL));
}

// ---------------------------------------------------------------------------

void UselessFlagSwap::Event(bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
    case bz_ePlayerJoinEvent:
    {
        bz_PlayerJoinPartEventData_V1* data = (bz_PlayerJoinPartEventData_V1*)eventData;
        pendingWelcome[data->playerID] = bz_getCurrentTime() + WELCOME_DELAY;
        break;
    }

    case bz_ePlayerPartEvent:
    {
        // Drop any pending welcome so a recycled player ID isn't messaged by mistake
        bz_PlayerJoinPartEventData_V1* data = (bz_PlayerJoinPartEventData_V1*)eventData;
        pendingWelcome.erase(data->playerID);
        break;
    }

    case bz_eTickEvent:
    {
        if (pendingWelcome.empty()) break;

        double now = bz_getCurrentTime();
        for (std::map<int, double>::iterator it = pendingWelcome.begin();
             it != pendingWelcome.end(); )
        {
            if (now < it->second) { ++it; continue; }

            int id = it->first;
            pendingWelcome.erase(it++);

            if (!isRegistered(id))
            {
                bz_sendTextMessage(BZ_SERVER, id,
                    "Welcome! Registered players get access to extra features on this server.");
                bz_sendTextMessage(BZ_SERVER, id,
                    bz_format("Register for free at %s and log in with your callsign to unlock them.",
                              REGISTER_URL));
            }
        }
        break;
    }

    case bz_eFlagGrabbedEvent:
    {
        bz_FlagGrabbedEventData_V1* data = (bz_FlagGrabbedEventData_V1*)eventData;
        if (!isUseless(data->flagType)) break;

        if (isRegistered(data->playerID))
        {
            refreshAllowed();
            bz_sendTextMessage(BZ_SERVER, data->playerID,
                "Useless flag! Type /use <flag> to trade it for a flag of your choice.");
            if (!allowedList.empty())
                bz_sendTextMessage(BZ_SERVER, data->playerID,
                    bz_format("Available: %s", allowedList.c_str()));
        }
        else
        {
            sendRegisterPitch(data->playerID,
                "Useless flag! Registered players can trade this for a flag of their choice with /use.");
        }
        break;
    }

    default:
        break;
    }
}

// ---------------------------------------------------------------------------

bool UselessFlagSwap::SlashCommand(int playerID, bz_ApiString /*command*/,
                                   bz_ApiString /*message*/, bz_APIStringList* params)
{
    if (!isRegistered(playerID))
    {
        bz_sendTextMessage(BZ_SERVER, playerID,
            bz_format("Only registered players can use /use. Register at %s", REGISTER_URL));
        return true;
    }

    if (!isUseless(bz_getPlayerFlag(playerID)))
    {
        bz_sendTextMessage(BZ_SERVER, playerID, "You need to be holding a Useless flag to use /use.");
        return true;
    }

    refreshAllowed();
    if (allowed.empty())
    {
        bz_sendTextMessage(BZ_SERVER, playerID, "No flags are available to trade for on this map.");
        return true;
    }

    if (!params || params->size() == 0)
    {
        bz_sendTextMessage(BZ_SERVER, playerID,
            bz_format("Usage: /use <flag>. Available: %s", allowedList.c_str()));
        return true;
    }

    std::string want = params->get(0).c_str();
    std::transform(want.begin(), want.end(), want.begin(), ::toupper);

    if (!allowed.count(want))
    {
        bz_sendTextMessage(BZ_SERVER, playerID,
            bz_format("'%s' isn't available. Choose from: %s", want.c_str(), allowedList.c_str()));
        return true;
    }

    bz_removePlayerFlag(playerID);
    bz_givePlayerFlag(playerID, want.c_str(), true);

    bz_sendTextMessage(BZ_SERVER, playerID,
        bz_format("You traded your Useless flag for %s.", want.c_str()));
    return true;
}
