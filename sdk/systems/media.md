# Photos, video and screens

What a mod author needs about the game's cameras, the journal's media and the lobby's screens. Facts were read from
decompiled code and scene files of Steam build 25811160 while planning the archived Replay mod (`archive/replay`). Little of
it is mapped yet: each fact says so, and Task 17 maps what a mod could use.

## The video camera

- **A clip is one still.** The video camera's capture renders its camera once by hand into a texture and reads the pixels
  back: what the journal calls a video is a single picture. Its camera rig in the scene ("Render Texture Camcorder") has its
  Camera off, renders through URP with its own volume, and carries an infrared spot light. `CCTV` is the placed camera's
  class.
- **Night vision** swaps the rig's volume profile to the night vision one, switches its light on, and adds one layer to the
  camera's culling mask; turning it off reverses each. Not mapped yet.

## The truck's monitor

`CCTVController.instance` is the monitor. `CCTVController.viewers` lists who is actively viewing it, on every client: the
game sends each change to everyone, and takes a dead player out. `CCTVController.GetCCTVCamerasByCurrentCameraType` gives the
switched-on cameras of the kind it shows, which every client also knows. A camera joins the list that call reads when it is
switched on, and leaves it when it is switched off. Which page of the grid a viewer is on, and whether they have one camera
full screen, stay on that viewer's own client, so no other client can tell exactly what they see.

## The journal's media

`Journal_PhotoPage` holds the journal's `Journal_PhotoPage.photos`, `Journal_PhotoPage.sounds` and
`Journal_PhotoPage.videos`. Adding a video puts its one picture on every image of that video's entry.

## The lobby

- **The Photos board** in the lobby's truck shows the last contract's pictures. The game writes them as PNG files beside its
  own data, in the game folder, and the lobby reads them back. **A mod never writes there**: the loader's rule is that
  nothing goes in the game folder.
- **The truck's CCTV screen** in the lobby only ever shows its screensaver.
- The lobby is walkable, and its truck sits to the side.

## A camera of a mod's own, and screen effects

The game renders with the Universal Render Pipeline. A camera a mod makes renders as the player's does when it copies the
player's camera (`Camera.CopyFrom` with `LocalPlayer.camera`) and its URP settings
(`UniversalAdditionalCameraData.get_renderPostProcessing` and `get_volumeLayerMask`, set on the copy's own data). While
it draws, the player's camera can be switched off (`Behaviour.set_enabled`): its transform, its AudioListener and the
held item's slot stay where they are, and so do the game's raycasts from it. A HUD canvas drawn by the player's camera
hides with it; overlay canvases do not. `sdk/api/third_person.h` does this.

The local player's own body is not drawn as other players' are. The game's renderer has `RenderObjects` passes ("Render
Player HUD" and "Render Player HUD UI", each Front and Back) that draw the `PlayerView` layer (`Layer.PlayerView`)
with their own depth test and field of view, over the world: the first-person view. A camera that shows the body from
outside moves its renderers to `AlivePlayer` (`Layer.AlivePlayer`, the layer the PCPlayer prefab gives the body), so
it draws with depth as anyone else's, and moves them back after. The head is drawn as shadows only on the player's own
client ([players](players.md#the-body)).

A blur or vignette of a mod's own is a URP volume: a GameObject on a layer the camera's volume mask holds, with a
`Volume` (`Volume.m_IsGlobal`, `Volume.priority`, `Volume.weight`) whose `Volume.sharedProfile` is a new `VolumeProfile`
(`ScriptableObject.CreateInstance`) holding a `DepthOfField` in Gaussian mode and a `Vignette` (`VolumeProfile.Add`),
their values set by running each parameter's own constructor on it again (`ClampedFloatParameter.ctor` with its
`ClampedFloatParameter.min` and `max`, `MinFloatParameter.ctor`, `DepthOfFieldModeParameter.ctor`). The generic setter,
`VolumeParameter<T>.set_value`, has no code in this build for a float or a depth-of-field mode (the running game finds
none for either instance), so a mod that needed it would stay off.
`sdk/api/screen_effects.h` does this. The game has no blur of its own for its menus.

## Engine features the game does not use

- `VideoPlayer` and `AsyncGPUReadback` have no managed methods left in this build (they were stripped). The engine
  still registers their calls: setting a URL, a render target and looping; prepare, play, pause and stop; asking for a
  texture's data and reading it back. A mod reaches them through icall entries, which the map supports. The vanilla
  maps' TVs use a `VideoPlayer`.
- Steam's timeline markers are not available: the game's Steam library has no `ISteamTimeline`.
