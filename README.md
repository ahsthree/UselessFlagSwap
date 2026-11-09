# UselessFlagSwap

A BZFS plugin that makes the Useless (`US`) flag useful.

When a registered player grabs a Useless flag, they can type `/use <flag>` to trade it for any flag available on the map. Unregistered players are told that registering unlocks the feature.

## Features

- **Flag trading:** `/use SW` swaps your Useless flag for a Shock Wave flag.
- **Registered players only:** Registration is checked via global login (`verified` on the player record).
- **No config file:** The offered flags are taken from the flags that actually exist in the running world, so the server's flag settings (`-f` options, map flag objects) are respected.
- **Safe flag list:** Bad flags, team flags, Useless and Genocide are never offered.
- **Helpful messages:**
  - Registered players see the list of available flags (comma-separated, e.g. `SW,ST,GM,L,SB`) when they grab a Useless flag.
  - Unregistered players are pointed to <https://forums.bzflag.org/> when they grab a Useless flag, try `/use`, or join the server (the welcome notice is sent 10 seconds after joining so it isn't buried in the initial messages).

## Usage

**Registered players**

1. Grab a Useless flag.
2. Type `/use <flag>` using one of the abbreviations shown in the chat message, for example:

   ```
   /use SW
   ```

3. The Useless flag is replaced with the requested flag.

Typing `/use` with no argument lists the available flags. Abbreviations are not case-sensitive.

**Unregistered players**

They receive a message explaining that registered players can trade a Useless flag for a flag of their choice, with a link to register for free. After registering, they log in with their callsign and password in the client.

## Which flags are offered

The list is rebuilt each time it's needed from the flags currently in the world, minus the following exclusions:

| Excluded | Reason |
|----------|--------|
| `US` | Useless: no point swapping into another one |
| `G` | Genocide: too disruptive to hand out |
| `B`, `BY`, `CB`, `FO`, `JM`, `LT`, `M`, `NJ`, `O`, `RC`, `RO`, `RT`, `TR`, `WA` | Bad flags |
| `R*`, `G*`, `B*`, `P*` | Team flags (any flag abbreviation ending in `*`) |

See the [BZFlag flag reference](https://www.bzflag.org/documentation/flags/) for the full list of flags.

Because the list comes from flags that exist in the world, a flag type is only offered if the server has at least one flag of that type. Disabling a flag on the server (for example with `-f -SW`) also removes it from the trade list.

## Requirements

- A BZFS server with plugin support (written against the 2.4.x plugin API)
- Players need global registration for `/use` to work

## Building

The plugin is a single file, `UselessFlagSwap.cpp`.

**Inside the BZFlag source tree:** place it in `plugins/UselessFlagSwap/` with the same build setup as the other plugins in that folder, then rebuild.

**Standalone**, as a shared library (adjust the include path to wherever your BZFlag headers are):

```
g++ -shared -fPIC -I/path/to/bzflag/include -o UselessFlagSwap.so UselessFlagSwap.cpp
```

If anything fails to compile, check the API names used here against your `bzfsAPI.h`, since a few functions differ slightly between 2.4.x builds (`bz_getFlagName`, `bz_format`, `bz_removePlayerFlag`, and the `verified` field on `bz_BasePlayerRecord`).

## Loading

Add the plugin to your server's command line or config:

```
-loadplugin /path/to/UselessFlagSwap
```

No plugin argument is required.

## Customizing

Everything is set at the top of `UselessFlagSwap.cpp`:

- **Registration URL:** the `REGISTER_URL` constant.
- **Welcome delay:** the `WELCOME_DELAY` constant (seconds).
- **Excluded flags:** the `excludedArr` array in `refreshAllowed()`. Add any abbreviation you don't want players handing themselves. Shock Wave (`SW`) and Steamroller (`SR`) are the usual candidates if they cause problems.
- **Message wording:** the strings in `Event()` and `SlashCommand()`.

## Testing checklist

- Join with an unregistered callsign and confirm the welcome notice arrives after about 10 seconds.
- Grab a Useless flag while unregistered and confirm the register message appears and `/use` is refused.
- Log in with a registered callsign, grab a Useless flag, and confirm the flag list appears.
- Run `/use SW` (or any listed flag) and confirm the swap works.
- Try an excluded or invalid flag (such as `/use G`) and confirm it is rejected.
- Confirm `/use` is refused when not holding a Useless flag.

## Notes and limitations

- "Registered" means authenticated through global login. If your server build reports this differently, adjust `isRegistered()`.
- On maps with random flags (`+f`), the set of flag types in the world can change over time, so the offered list may vary slightly.
- The Useless flag is consumed by the swap. If a player drops it, `/use` stops working until they grab one again.
