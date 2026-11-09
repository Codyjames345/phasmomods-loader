# Weather and lightning

What a mod author needs about a contract's weather and the storm's lightning. The entries are in `sdk/map/Weather.h`
and `sdk/map/Difficulty.h`; their `///` notes are the authority. Facts were read from decompiled code of Steam build
25811160 (`re/decomp.mjs <key>`).

## The weather types

`WeatherType` holds the game's weathers by their real value names: clear, foggy, rain, heavyRain, snow, wind, sunrise,
bloodMoon and dead. `dead` is the dead players' own profile (RandomWeather's profile for them), never a contract's, so a
dead player's client reads it while the living read the contract's. Read the values through the map: an unresolved value
reads 0, which is clear.

## How a contract's weather is chosen

- The difficulty's selected weather is a number. When it is 0 (random), the ghost's setup picks one with the game's random
  range over the weathers after clear, which never reaches bloodMoon: a random contract is never a Blood Moon.
- The result is the contract's actual weather, `Difficulty.actualWeather`, which the game syncs to every client. Read it
  there rather than from the sky.
- **Blood Moon makes hunts faster** (see [Ghost AI](ghost-ai.md), "Hunt speed").
- **The Monkey Paw's weather wish** changes the weather mid-contract through `RandomWeather`'s profile transition, which
  sets the weather at once; its visuals and sound are skipped on a client whose local player is dead. That method is not
  mapped yet.
- **Read the weather each time you need it.** The transition writes the wished profile into `LevelValues.weather` before
  it checks for a dead player, and the paw's weather handler has no master check, so the field changes on the host too.
  The game's storm reads it again at each strike, so a wish for heavy rain starts the storm. A mod that reads the weather
  once per contract misses the wish (the Tempest did, until 2026-11-05).

A mod that changes the weather mid-contract makes a change no vanilla ghost makes, which identifies it at once (the user's
reason for the Tempest never changing the weather).

## Lightning

- **The storm's strikes.** `LightningController`'s update on the master sends `LightningController.PlayLightningNetworked`
  to every client every 0 to 30 seconds while the weather is heavy rain, reading `LevelValues.weather` again each time its
  timer runs out. The strike plays from `RandomWeather.instance`'s
  `RandomWeather.effects`, whose `AzureEffectsController.thunderSettingsList` holds the strikes (a prefab, a clip and a
  position each).
- **Never send the storm's strike yourself.** A client whose weather is not heavy rain refuses it, and a refused RPC counts
  against its sender ([Photon](photon.md), "The game's checks on an RPC").
- **A strike on one client.** `LightningController.PlayLocalLightning` strikes on this client only, from the controller's own
  `LightningController.effects`. The game uses it only for the lobby's hoop. No contract scene sets those effects, so it
  throws in a contract until a mod gives the controller an effects controller first; the Tempest gives it
  `RandomWeather.effects`. An empty strike list throws too.
