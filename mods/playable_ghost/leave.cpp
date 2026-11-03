#include "leave.h"

#include <windows.h>

#include "Difficulty.h"
#include "GameController.h"
#include "Photon.h"
#include "Player.h"
#include "UnityAudio.h"
#include "body.h"
#include "game.h"
#include "log.h"
#include "runtime.h"

namespace pm::leave {
namespace {

using phasmo::LoadingController;
using phasmo::PauseMenuController;

// The vanilla leave (PauseMenuController's coroutine) disconnects once the loading screen is ready
// and a frame has passed; kLeaveTimeoutMs bounds the wait.
constexpr ULONGLONG kLeaveTimeoutMs = 15000;

ULONGLONG g_leaveStarted = 0;  // a leave is under way (local clock), until Photon is disconnected
bool g_loadingReady = false;    // the loading screen was ready last frame: disconnect this frame
bool g_leaving = false;        // from the leave until the next level

// The leave without the game's Leave, for a client whose investigator did not come back: the steps of
// its coroutine, which tick_background finishes.
void leave_without_investigator() {
  void* loading = LoadingController::instance.get();
  if (!loading) {
    PM_LOG("leave: no LoadingController; cannot leave the contract");
    return;
  }
  if (void* values = phasmo::LevelValues::instance.get()) phasmo::LevelValues::completed.set(values, false);
  if (void* network = phasmo::Network::get_Instance()) phasmo::Network::sceneChanging.set(network, true);
  phasmo::AudioListener::set_volume(0);
  LoadingController::LoadScene(loading, false, 0, api::make_string(""), true);
  g_leaveStarted = GetTickCount64();
  g_loadingReady = false;
  PM_LOG("leave: leaving the contract without an investigator");
}

void detour_leave(api::Original<PauseMenuController::Leave> original, void* menu) {
  if (!body::camera()) {
    original(menu);
    return;
  }
  g_leaving = true;
  body::teardown();  // the rig, camera and body come back for the main menu
  if (body::investigator_back()) {
    PM_LOG("leave: leaving the contract");
    original(menu);  // the game's own leave, which reads the investigator
    return;
  }
  leave_without_investigator();
}

void detour_quit(api::Original<PauseMenuController::Quit> original, void* menu) {
  if (!body::camera()) {
    original(menu);
    return;
  }
  PM_LOG("leave: quitting the game");
  body::teardown();
  if (body::investigator_back()) {
    original(menu);  // the game's own quit, which reads the investigator
    return;
  }
  PM_LOG("leave: no investigator came back; quitting without the game's own steps");
  phasmo::Application::Quit(0);  // Unity's Quit()
}

}  // namespace

bool create_hooks() {
  // The canonical bodies, as the raw hooks by address were.
  return api::hook<PauseMenuController::Leave>(&detour_leave, "PauseMenuController.Leave", api::Twins::CanonicalOnly) &&
         api::hook<PauseMenuController::Quit>(&detour_quit, "PauseMenuController.Quit", api::Twins::CanonicalOnly);
}

void left() { g_leaving = true; }

bool leaving() { return g_leaving; }

void tick_background() {
  // Here, a frame that always runs: the cover's own read of leaving() stops while there is no level.
  if (g_leaving && !game::level_controller()) g_leaving = false;
  if (!g_leaveStarted) return;
  void* loading = LoadingController::instance.get();
  const bool timedOut = GetTickCount64() - g_leaveStarted >= kLeaveTimeoutMs;
  if (!g_loadingReady && !timedOut) {
    g_loadingReady = game::alive(loading) && LoadingController::ready.get(loading);
    return;  // as the coroutine does: the frame after the loading screen is ready
  }
  g_leaveStarted = 0;
  try {
    phasmo::PhotonNetwork::Disconnect();
    PM_LOG("leave: disconnected after leaving%s", timedOut ? " (the loading screen never reported ready)" : "");
  } catch (...) {
    PM_LOG("leave: Photon's disconnect after leaving threw");
  }
}

}  // namespace pm::leave
