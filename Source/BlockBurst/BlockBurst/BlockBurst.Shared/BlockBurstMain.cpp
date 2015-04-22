#include "pch.h"
#include "BlockBurstMain.h"
#include "Common\DirectXHelper.h"

using namespace BlockBurst;

using namespace DirectX;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// Loads and initializes application assets when the application is loaded.
BlockBurstMain::BlockBurstMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources),
	difficulty(1.0f),
	spawnTimeRemaining(1.0f)
{
	// Register to be notified if the Device is lost or recreated
	m_deviceResources->RegisterDeviceNotify(this);

	// TODO: Replace this with your app's content initialization.
	m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(m_deviceResources));

	m_fpsTextRenderer = std::unique_ptr<SampleFpsTextRenderer>(new SampleFpsTextRenderer(m_deviceResources));

	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/

	this->blocks = std::make_shared<std::vector<Block>>();
}

BlockBurstMain::~BlockBurstMain()
{
	// Deregister device notification
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Updates application state when the window size changes (e.g. device orientation change)
void BlockBurstMain::CreateWindowSizeDependentResources() 
{
	// TODO: Replace this with the size-dependent initialization of your app's content.
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

// Updates the application state once per frame.
void BlockBurstMain::Update() 
{
	if (!this->initialized)
	{
		if (this->m_sceneRenderer->IsInitialized())
		{
			this->CreateBlock(XMFLOAT3(-3.0f, 0.0f, 0.0f));
			this->CreateBlock(XMFLOAT3(3.0f, 0.0f, 0.0f));

			this->m_sceneRenderer->BuildGPUBuffers(this->blocks);

			this->initialized = true;
		}

		return;
	}

	// Update scene objects.
	m_timer.Tick([&]()
	{
		auto dt = m_timer.GetElapsedSeconds();

		// Convert degrees to radians, then convert seconds to rotation angle
		float radiansPerSecond = XMConvertToRadians(45);
		double totalRotation = m_timer.GetTotalSeconds() * radiansPerSecond;
		float radians = static_cast<float>(fmod(totalRotation, XM_2PI));

		for (auto it = this->blocks->begin(); it != this->blocks->end(); ++it)
		{
			Block& block = *it;

			// Rotate block.
			block.rotation = radians;

			// Translate block.
			block.position = XMFLOAT3(
				block.position.x + block.velocity.x * dt,
				block.position.y + block.velocity.y * dt,
				block.position.z + block.velocity.z * dt);
		}

		// Tick spawn timer.
		this->spawnTimeRemaining -= dt;

		if (this->spawnTimeRemaining <= 0)
		{
			int randomX = rand() % 10 - 5;
			this->CreateBlock(XMFLOAT3(randomX, 0.0f, 0.0f));
			this->m_sceneRenderer->BuildGPUBuffers(this->blocks);
			this->spawnTimeRemaining = this->difficulty;
		}

		// TODO: Replace this with your app's content update functions.
		m_sceneRenderer->Update(m_timer);
		m_fpsTextRenderer->Update(m_timer);
	});
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool BlockBurstMain::Render() 
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// Reset the viewport to target the whole screen.
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	// Reset render targets to the screen.
	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

	// Clear the back buffer and depth stencil view.
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::CornflowerBlue);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Render the scene objects.
	// TODO: Replace this with your app's content rendering functions.
	m_sceneRenderer->Render();
	m_fpsTextRenderer->Render();

	return true;
}

void BlockBurstMain::OnTap(float screenPositionX, float screenPositionY)
{
	if (this->blocks->empty())
	{
		return;
	}

	auto closestBlockIt = this->blocks->begin();

	// Find closest block.
	for (auto it = this->blocks->begin(); it != this->blocks->end(); ++it)
	{
		if ((*it).position.z < (*closestBlockIt).position.z)
		{
			closestBlockIt = it;
		}
	}

	// Get spawn position for new block.
	Block& closestBlock = *closestBlockIt;
	auto position = closestBlock.position;

	// Remove closest block.
	this->blocks->erase(closestBlockIt);
	
	// Add two new blocks.
	this->CreateBlock(XMFLOAT3(position.x - 1, position.y, position.z));
	this->CreateBlock(XMFLOAT3(position.x + 1, position.y, position.z));

	// Rebuild vertex and index buffers.
	this->m_sceneRenderer->BuildGPUBuffers(this->blocks);
}

// Notifies renderers that device resources need to be released.
void BlockBurstMain::OnDeviceLost()
{
	m_sceneRenderer->ReleaseDeviceDependentResources();
	m_fpsTextRenderer->ReleaseDeviceDependentResources();
}

// Notifies renderers that device resources may now be recreated.
void BlockBurstMain::OnDeviceRestored()
{
	m_sceneRenderer->CreateDeviceDependentResources();
	m_fpsTextRenderer->CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

void BlockBurstMain::CreateBlock(XMFLOAT3 position)
{
	auto block = Block();
	block.position = position;
	block.velocity = XMFLOAT3(0.0f, 0.0f, -this->difficulty);

	block.vertices[0] = VertexPositionColor{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f) };
	block.vertices[1] = VertexPositionColor{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) };
	block.vertices[2] = VertexPositionColor{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f) };
	block.vertices[3] = VertexPositionColor{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f) };
	block.vertices[4] = VertexPositionColor{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f) };
	block.vertices[5] = VertexPositionColor{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f) };
	block.vertices[6] = VertexPositionColor{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f) };
	block.vertices[7] = VertexPositionColor{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f) };

	this->blocks->push_back(block);
}