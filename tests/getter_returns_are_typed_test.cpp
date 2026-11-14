// The map entries task 0b-2c gave a return type: each entry's `.type` and its wrapper's return
// agree, and the wrapper gives the handle the entry names. Every assertion is a static_assert, so
// compiling this test is the check; main only reports that the build reached it.
// Run: build\getter_returns_are_typed_test.exe
#include <type_traits>

#include "Animator.h"
#include "Camera.h"
#include "Difficulty.h"
#include "Doozy.h"
#include "FinalIK.h"
#include "GhostAI.h"
#include "GhostCollection.h"
#include "Input.h"
#include "LevelRoom.h"
#include "LevelStats.h"
#include "MainMenu.h"
#include "Photon.h"
#include "Player.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "UnityUI.h"
#include "UnityWorld.h"
#include "game_api.h"

namespace {

namespace map = pm::map;

// The return type of a Method wrapper (sdk/api/game_api.h): the R of Method<D, R(A...)>.
template <class M>
struct wrapper_return;
template <const pm::sdk::EntryDesc& D, class R, class... A>
struct wrapper_return<pm::api::Method<D, R(A...)>> {
  using type = R;
};
// A wrapper is a static constexpr member, so decltype gives it const: strip it.
template <class M>
using wrapper_return_t = typename wrapper_return<std::remove_const_t<M>>::type;

// One line per key 0b-2c typed, in map header order: the wrapper returns the handle its entry's
// `.type` names. The dump's declared return type is the evidence for each; task-0b2c-report.md
// lists it with the dump file and line.
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Animator::GetBoneTransform)>,
                             pm::api::Ref<map::Transform>>,
              "Animator.GetBoneTransform must return Ref<Transform>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::AudioClip::Create)>,
                             pm::api::Ref<map::AudioClip>>,
              "AudioClip.Create must return Ref<AudioClip>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Camera::get_targetTexture)>,
                             pm::api::Ref<map::RenderTexture>>,
              "Camera.get_targetTexture must return Ref<RenderTexture>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Canvas::get_worldCamera)>,
                             pm::api::Ref<map::Camera>>,
              "Canvas.get_worldCamera must return Ref<Camera>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Collider::get_attachedRigidbody)>,
                             pm::api::Ref<map::Rigidbody>>,
              "Collider.get_attachedRigidbody must return Ref<Rigidbody>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Component::get_gameObject)>,
                             pm::api::Ref<map::GameObject>>,
              "Component.get_gameObject must return Ref<GameObject>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Component::get_transform)>,
                             pm::api::Ref<map::Transform>>,
              "Component.get_transform must return Ref<Transform>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::ConnectToServer::get_Instance)>,
                             pm::api::Ref<map::ConnectToServer>>,
              "ConnectToServer.get_Instance must return Ref<ConnectToServer>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::EventSystem::get_current)>,
                             pm::api::Ref<map::EventSystem>>,
              "EventSystem.get_current must return Ref<EventSystem>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::EventSystem::get_currentSelectedGameObject)>,
                             pm::api::Ref<map::GameObject>>,
              "EventSystem.get_currentSelectedGameObject must return Ref<GameObject>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Gamepad::get_current)>,
                             pm::api::Ref<map::Gamepad>>,
              "Gamepad.get_current must return Ref<Gamepad>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Gamepad::get_leftShoulder)>,
                             pm::api::Ref<map::ButtonControl>>,
              "Gamepad.get_leftShoulder must return Ref<ButtonControl>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Gamepad::get_rightShoulder)>,
                             pm::api::Ref<map::ButtonControl>>,
              "Gamepad.get_rightShoulder must return Ref<ButtonControl>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::GameObject::get_transform)>,
                             pm::api::Ref<map::Transform>>,
              "GameObject.get_transform must return Ref<Transform>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::GhostActivity::GetDoorToOpen)>,
                             pm::api::Ref<map::Door>>,
              "GhostActivity.GetDoorToOpen must return Ref<Door>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::GhostActivity::GetPropToThrow)>,
                             pm::api::Ref<map::Prop>>,
              "GhostActivity.GetPropToThrow must return Ref<Prop>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::GhostCollection::GetGhostOfType)>,
                             pm::api::Ref<map::GhostData>>,
              "GhostCollection.GetGhostOfType must return Ref<GhostData>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Graphic::get_canvasRenderer)>,
                             pm::api::Ref<map::CanvasRenderer>>,
              "Graphic.get_canvasRenderer must return Ref<CanvasRenderer>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::IKSolverFullBodyBiped::get_leftHandEffector)>,
                             pm::api::Ref<map::IKEffector>>,
              "IKSolverFullBodyBiped.get_leftHandEffector must return Ref<IKEffector>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::IKSolverFullBodyBiped::get_rightHandEffector)>,
                             pm::api::Ref<map::IKEffector>>,
              "IKSolverFullBodyBiped.get_rightHandEffector must return Ref<IKEffector>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::InputActionReference::Create)>,
                             pm::api::Ref<map::InputActionReference>>,
              "InputActionReference.Create must return Ref<InputActionReference>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::InputActionReference::get_action)>,
                             pm::api::Ref<map::InputAction>>,
              "InputActionReference.get_action must return Ref<InputAction>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::InputActionSetupExtensions::AddAction)>,
                             pm::api::Ref<map::InputAction>>,
              "InputActionSetupExtensions.AddAction must return Ref<InputAction>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::LevelStats::get_Instance)>,
                             pm::api::Ref<map::LevelStats>>,
              "LevelStats.get_Instance must return Ref<LevelStats>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::LevelValues::get_Instance)>,
                             pm::api::Ref<map::LevelValues>>,
              "LevelValues.get_Instance must return Ref<LevelValues>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Material::GetTextureImpl)>,
                             pm::api::Ref<map::Texture>>,
              "Material.GetTextureImpl must return Ref<Texture>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::MonoBehaviourPun::get_photonView)>,
                             pm::api::Ref<map::PhotonView>>,
              "MonoBehaviourPun.get_photonView must return Ref<PhotonView>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Mouse::get_current)>,
                             pm::api::Ref<map::Mouse>>,
              "Mouse.get_current must return Ref<Mouse>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Mouse::get_leftButton)>,
                             pm::api::Ref<map::ButtonControl>>,
              "Mouse.get_leftButton must return Ref<ButtonControl>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Network::GetSpotPlayerFromPhotonPlayer)>,
                             pm::api::Ref<map::PlayerSpot>>,
              "Network.GetSpotPlayerFromPhotonPlayer must return Ref<PlayerSpot>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Network::get_Instance)>,
                             pm::api::Ref<map::Network>>,
              "Network.get_Instance must return Ref<Network>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::PCPropGrab::GetItemInCameraSlot)>,
                             pm::api::Ref<map::PhotonObjectInteract>>,
              "PCPropGrab.GetItemInCameraSlot must return Ref<PhotonObjectInteract>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::PhotonNetwork::get_CurrentRoom)>,
                             pm::api::Ref<map::PhotonRoom>>,
              "PhotonNetwork.get_CurrentRoom must return Ref<PhotonRoom>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::PhysicsCharacterController::get_CapsuleCollider)>,
                             pm::api::Ref<map::CapsuleCollider>>,
              "PhysicsCharacterController.get_CapsuleCollider must return Ref<CapsuleCollider>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::PhysicsCharacterController::get_Rigidbody)>,
                             pm::api::Ref<map::Rigidbody>>,
              "PhysicsCharacterController.get_Rigidbody must return Ref<Rigidbody>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::PlayerAudio::GetPlayerVoiceAudioSource)>,
                             pm::api::Ref<map::AudioSource>>,
              "PlayerAudio.GetPlayerVoiceAudioSource must return Ref<AudioSource>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::RaycastHit::get_transform)>,
                             pm::api::Ref<map::Transform>>,
              "RaycastHit.get_transform must return Ref<Transform>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::RenderTexture::GetTemporary)>,
                             pm::api::Ref<map::RenderTexture>>,
              "RenderTexture.GetTemporary must return Ref<RenderTexture>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::RenderTexture::get_active)>,
                             pm::api::Ref<map::RenderTexture>>,
              "RenderTexture.get_active must return Ref<RenderTexture>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Renderer::get_material)>,
                             pm::api::Ref<map::Material>>,
              "Renderer.get_material must return Ref<Material>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Renderer::get_sharedMaterial)>,
                             pm::api::Ref<map::Material>>,
              "Renderer.get_sharedMaterial must return Ref<Material>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Selectable::get_targetGraphic)>,
                             pm::api::Ref<map::Graphic>>,
              "Selectable.get_targetGraphic must return Ref<Graphic>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::TMP_Text::get_fontSharedMaterial)>,
                             pm::api::Ref<map::Material>>,
              "TMP_Text.get_fontSharedMaterial must return Ref<Material>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Transform::GetChild)>,
                             pm::api::Ref<map::Transform>>,
              "Transform.GetChild must return Ref<Transform>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::Transform::get_parent)>,
                             pm::api::Ref<map::Transform>>,
              "Transform.get_parent must return Ref<Transform>");
static_assert(std::is_same_v<wrapper_return_t<decltype(phasmo::UISelectable::SetState)>,
                             pm::api::Ref<map::UISelectable>>,
              "UISelectable.SetState must return Ref<UISelectable>");

}  // namespace

int main() { return 0; }
