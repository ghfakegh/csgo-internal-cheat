#include "hooks.h"

// include minhook for epic hookage
#include "../../ext/minhook/minhook.h"
#include "../../ext/x86retspoof/x86RetSpoof.h"

#include <intrin.h>

#include "../hacks/misc.h"

void hooks::Setup() noexcept
{
	MH_Initialize();

	// AllocKeyValuesMemory hook
	MH_CreateHook(
		memory::Get(interfaces::keyValuesSystem, 1),
		&AllocKeyValuesMemory,
		reinterpret_cast<void**>(&AllocKeyValuesMemoryOriginal)
	);

	// CreateMove hook
	MH_CreateHook(
		memory::Get(interfaces::clientMode, 24),
		&CreateMove,
		reinterpret_cast<void**>(&CreateMoveOriginal)
	);

	// PaintTraverse hook
	MH_CreateHook(
		memory::Get(interfaces::panel, 41),
			&PaintTraverse,
			reinterpret_cast<void**>(&PaintTraverseOriginal)
		);

	MH_EnableHook(MH_ALL_HOOKS);
}

void hooks::Destroy() noexcept
{
	// restore hooks
	MH_DisableHook(MH_ALL_HOOKS);
	MH_RemoveHook(MH_ALL_HOOKS);

	// uninit minhook
	MH_Uninitialize();
}

void* __stdcall hooks::AllocKeyValuesMemory(const std::int32_t size) noexcept
{
	// if function is returning to speficied addresses, return nullptr to "bypass"
	if (const std::uint32_t address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
		address == reinterpret_cast<std::uint32_t>(memory::allocKeyValuesEngine) ||
		address == reinterpret_cast<std::uint32_t>(memory::allocKeyValuesClient)) 
		return nullptr;

	// return original
	return AllocKeyValuesMemoryOriginal(interfaces::keyValuesSystem, size);
}

bool __stdcall hooks::CreateMove(float frameTime, CUserCmd* cmd) noexcept
{
	static const auto sequence = reinterpret_cast<std::uintptr_t>(memory::PatternScan("client.dll", "FF 23"));
	const auto result = x86RetSpoof::invokeStdcall<bool>((uintptr_t)hooks::CreateMoveOriginal, sequence, frameTime, cmd);

	// make sure this function is being called from CInput::CreateMove
	if (!cmd || !cmd->commandNumber)
		return result;

	// this would be done anyway by returning true
	if (CreateMoveOriginal(interfaces::clientMode, frameTime, cmd))
		interfaces::engine->SetViewAngles(cmd->viewAngles);

	// get our local player here
	globals::UpdateLocalPlayer();

	if (globals::localPlayer && globals::localPlayer->IsAlive())
	{
		// example bhop
		hacks::RunBunnyHop(cmd);
	}

	return false;
}

// our hooks
void __stdcall hooks::PaintTraverse(std::uintptr_t vguiPanel, bool forceRepaint, bool allowForce) noexcept
{
	// make sure we have the correct panel
	if (vguiPanel == interfaces::engineVGui->GetPanel(PANEL_TOOLS))
	{
		// make sure local player is valid && we are in-game
		if (globals::localPlayer && interfaces::engine->IsInGame())
		{
			// loop through the player list
			for (int i = 1; i < interfaces::globals->maxClients; ++i)
			{
				// get the player pointer
				CEntity* player = interfaces::entityList->GetEntityFromIndex(i);

				// make sure player is not nullptr
				if (!player)
				{
					continue;
				}

				// make sure player is not dormant && is alive
				if (player->IsDormant() || !player->IsAlive())
				{
					continue;
				}

				// no ESP on team
				if (player->GetTeam() == globals::localPlayer->GetTeam())
				{
					continue;
				}

				// dont do ESP on player we are spectating
				if (!globals::localPlayer->IsAlive())
				{
					if (globals::localPlayer->GetObserverTarget() == player)
					{
						continue;
					}
				}
				// players bone matrix
				CMatrix3x4 bones[128];
				if (!player->SetupBones(bones, 128, 0x7FF00, interfaces::globals->currentTime))
				{
					continue;
				}

				// screen position of head
				CVector top;
				if (interfaces::debugOverlay->ScreenPosition(bones[8].Origin() + CVector{ 0.f, 0.f, 11.f }, top))
				{
					continue;
				}

				// screen position of feet
				CVector bottom;
				if (interfaces::debugOverlay->ScreenPosition(player->GetAbsOrigin() - CVector{ 0.f,0.f,9.f }, bottom))
				{
					continue;
				}

				// height of box
				const float h = bottom.y - top.y;

				// use the height to determine a width
				const float w = h * 0.30f;

				const auto left = static_cast<int>(top.x - w);
				const auto right = static_cast<int>(top.x + w);

				//set drawing color to white
				interfaces::surface->DrawSetColor(255, 255, 255, 255);

				// draw normal box
				interfaces::surface->DrawOutlinedRect(left, top.y, right, bottom.y);
			}
		}
	}

	// call the original function
	PaintTraverseOriginal(interfaces::panel, vguiPanel, forceRepaint, allowForce);
}