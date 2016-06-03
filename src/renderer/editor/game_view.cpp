#include "game_view.h"
#include "editor/studio_app.h"
#include "engine/crc32.h"
#include "engine/input_system.h"
#include "engine/profiler.h"
#include "engine/resource_manager.h"
#include "engine/engine.h"
#include "engine/plugin_manager.h"
#include "imgui/imgui.h"
#include "renderer/frame_buffer.h"
#include "renderer/pipeline.h"
#include "renderer/render_scene.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include <SDL.h>


GameView::GameView(StudioApp& app)
	: m_studio_app(app)
	, m_is_opened(true)
	, m_pipeline(nullptr)
	, m_is_mouse_captured(false)
	, m_editor(nullptr)
	, m_is_mouse_hovering_window(false)
	, m_time_multiplier(1.0f)
	, m_paused(false)
{
	m_is_opengl = false;
}


GameView::~GameView()
{
}


void GameView::onUniverseCreated()
{
	auto* scene = m_editor->getScene(Lumix::crc32("renderer"));
	m_pipeline->setScene(static_cast<Lumix::RenderScene*>(scene));
}


void GameView::onUniverseDestroyed()
{
	m_pipeline->setScene(nullptr);
}


void GameView::init(Lumix::WorldEditor& editor)
{
	m_editor = &editor;
	auto& engine = editor.getEngine();
	auto* renderer = static_cast<Lumix::Renderer*>(engine.getPluginManager().getPlugin("renderer"));
	m_is_opengl = renderer->isOpenGL();
	Lumix::Path path("pipelines/game_view.lua");
	m_pipeline = Lumix::Pipeline::create(*renderer, path, engine.getAllocator());
	m_pipeline->load();

	editor.universeCreated().bind<GameView, &GameView::onUniverseCreated>(this);
	editor.universeDestroyed().bind<GameView, &GameView::onUniverseDestroyed>(this);
	onUniverseCreated();
}


void GameView::shutdown()
{
	m_editor->universeCreated().unbind<GameView, &GameView::onUniverseCreated>(this);
	m_editor->universeDestroyed().unbind<GameView, &GameView::onUniverseDestroyed>(this);
	Lumix::Pipeline::destroy(m_pipeline);
	m_pipeline = nullptr;
}


void GameView::setScene(Lumix::RenderScene* scene)
{
	m_pipeline->setScene(scene);
}


void GameView::captureMouse(bool capture)
{
	m_is_mouse_captured = capture;
	m_editor->getEngine().getInputSystem().enable(m_is_mouse_captured);
	SDL_ShowCursor(m_is_mouse_captured ? 0 : 1);
	SDL_SetRelativeMouseMode(capture ? SDL_TRUE : SDL_FALSE);
}


void GameView::onGui()
{
	PROFILE_FUNCTION();
	if (!m_pipeline->isReady()) return;

	auto& io = ImGui::GetIO();

	bool is_focus = (SDL_GetWindowFlags(m_studio_app.getWindow()) & SDL_WINDOW_INPUT_FOCUS) != 0;
	if (m_is_mouse_captured &&
		(io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_Escape)] || !m_editor->isGameMode() || !is_focus))
	{
		captureMouse(false);
	}

	const char* window_name = "Game View###game_view";
	if (m_is_mouse_captured) window_name = "Game View (mouse captured)###game_view";
	if (ImGui::BeginDock(window_name, &m_is_opened))
	{
		m_is_mouse_hovering_window = ImGui::IsMouseHoveringWindow();

		auto content_min = ImGui::GetCursorScreenPos();
		auto size = ImGui::GetContentRegionAvail();
		size.y -= ImGui::GetTextLineHeightWithSpacing();
		ImVec2 content_max(content_min.x + size.x, content_min.y + size.y);
		if (size.x > 0 && size.y > 0)
		{
			auto pos = ImGui::GetWindowPos();
			auto cp = ImGui::GetCursorPos();
			m_pipeline->setViewport(0, 0, int(size.x), int(size.y));

			auto* fb = m_pipeline->getFramebuffer("default");
			m_texture_handle = fb->getRenderbufferHandle(0);
			if (m_is_opengl)
			{
				ImGui::Image(&m_texture_handle, size, ImVec2(0, 1), ImVec2(1, 0));
			}
			else
			{
				ImGui::Image(&m_texture_handle, size);
			}
			if (ImGui::Checkbox("Pause", &m_paused))
			{
				m_editor->getEngine().pause(m_paused);
			}
			if (m_paused)
			{
				ImGui::SameLine();
				if (ImGui::Button("Next frame"))
				{
					m_editor->getEngine().nextFrame();
				}
			}
			ImGui::SameLine();
			if (ImGui::DragFloat("Time multiplier", &m_time_multiplier, 0.01f, 0.01f, 30.0f))
			{
				m_editor->getEngine().setTimeMultiplier(m_time_multiplier);
			}
			m_pipeline->render();
		}

		if (m_is_mouse_captured)
		{
			if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_Escape)] || !m_editor->isGameMode())
			{
				captureMouse(false);
			}
		}

		if (ImGui::IsMouseHoveringRect(content_min, content_max) && m_is_mouse_hovering_window &&
			ImGui::IsMouseClicked(0) && m_editor->isGameMode())
		{
			captureMouse(true);
		}
	}
	ImGui::EndDock();
}
