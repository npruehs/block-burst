#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\Sample3DSceneRenderer.h"
#include "Content\SampleFpsTextRenderer.h"

#include "Block.h"

// Renders Direct2D and 3D content on the screen.
namespace BlockBurst
{
	class BlockBurstMain : public DX::IDeviceNotify
	{
	public:
		BlockBurstMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~BlockBurstMain();
		void CreateWindowSizeDependentResources();
		void Update();
		bool Render();

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// TODO: Replace with your own content renderers.
		std::unique_ptr<Sample3DSceneRenderer> m_sceneRenderer;
		std::unique_ptr<SampleFpsTextRenderer> m_fpsTextRenderer;

		// Rendering loop timer.
		DX::StepTimer m_timer;

		bool initialized;

		// Blocks in the scene.
		std::shared_ptr<std::vector<Block>> blocks;

		// Creates a new block at the specified position and adds it to the scene to be rendered.
		void CreateBlock(float posX, float posY, float posZ);
	};
}