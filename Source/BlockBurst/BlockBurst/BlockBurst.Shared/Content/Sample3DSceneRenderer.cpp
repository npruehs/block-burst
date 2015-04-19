#include "pch.h"
#include "Sample3DSceneRenderer.h"

#include "..\Common\DirectXHelper.h"

using namespace BlockBurst;

using namespace DirectX;
using namespace Windows::Foundation;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_tracking(false),
	m_deviceResources(deviceResources)
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	// Note that the OrientationTransform3D matrix is post-multiplied here
	// in order to correctly orient the scene to match the display orientation.
	// This post-multiplication step is required for any draw calls that are
	// made to the swap chain render target. For draw calls to other targets,
	// this transform should not be applied.

	// This sample makes use of a right-handed coordinate system using row-major matrices.
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
		fovAngleY,
		aspectRatio,
		0.01f,
		100.0f
		);

	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();

	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(
		&m_constantBufferData.projection,
		XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
		);

	// Eye is at (0,0,-5), looking at point (0,0,0) with the up-vector along the y-axis.
	static const XMVECTORF32 eye = { 0.0f, 0.0f, -5.0f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void Sample3DSceneRenderer::Update(DX::StepTimer const& timer)
{
	if (!m_tracking)
	{
		// Convert degrees to radians, then convert seconds to rotation angle
		float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
		double totalRotation = timer.GetTotalSeconds() * radiansPerSecond;
		float radians = static_cast<float>(fmod(totalRotation, XM_2PI));

		Rotate(radians);
	}
}

// Rotate the 3D cube model a set amount of radians.
void Sample3DSceneRenderer::Rotate(float radians)
{
	for (auto it = this->blocks.begin(); it != this->blocks.end(); ++it)
	{
		Block& block = *it;
		block.rotation = radians;
	}
}

void Sample3DSceneRenderer::StartTracking()
{
	m_tracking = true;
}

// When tracking, the 3D cube can be rotated around its Y axis by tracking pointer position relative to the output screen width.
void Sample3DSceneRenderer::TrackingUpdate(float positionX)
{
	if (m_tracking)
	{
		float radians = XM_2PI * 2.0f * positionX / m_deviceResources->GetOutputSize().Width;
		Rotate(radians);
	}
}

void Sample3DSceneRenderer::StopTracking()
{
	m_tracking = false;
}

// Renders one frame using the vertex and pixel shaders.
void Sample3DSceneRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	for (auto it = this->blocks.begin(); it != this->blocks.end(); ++it)
	{
		Block& block = *it;

		// Prepare to pass the updated model matrix to the shader
		XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(
			XMMatrixRotationRollPitchYaw(0, block.rotation, 0) *
			XMMatrixTranslation(block.posX, block.posY, block.posZ)
			));

		// Prepare the constant buffer to send it to the graphics device.
		context->UpdateSubresource(
			m_constantBuffer.Get(),
			0,
			NULL,
			&m_constantBufferData,
			0,
			0
			);

		// Each vertex is one instance of the VertexPositionColor struct.
		UINT stride = sizeof(VertexPositionColor);
		UINT offset = 0;
		context->IASetVertexBuffers(
			0,
			1,
			m_vertexBuffer.GetAddressOf(),
			&stride,
			&offset
			);

		context->IASetIndexBuffer(
			m_indexBuffer.Get(),
			DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
			0
			);

		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->IASetInputLayout(m_inputLayout.Get());

		// Attach our vertex shader.
		context->VSSetShader(
			m_vertexShader.Get(),
			nullptr,
			0
			);

		// Send the constant buffer to the graphics device.
		context->VSSetConstantBuffers(
			0,
			1,
			m_constantBuffer.GetAddressOf()
			);

		// Attach our pixel shader.
		context->PSSetShader(
			m_pixelShader.Get(),
			nullptr,
			0
			);

		// Draw the objects.
		context->DrawIndexed(
			36,
			0,
			block.indexOffset
			);
	}
}

void Sample3DSceneRenderer::CreateDeviceDependentResources()
{
	// Load shaders asynchronously.
	auto loadVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso");

	// After the vertex shader file is loaded, create the shader and input layout.
	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateVertexShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_vertexShader
				)
			);

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc [] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateInputLayout(
				vertexDesc,
				ARRAYSIZE(vertexDesc),
				&fileData[0],
				fileData.size(),
				&m_inputLayout
				)
			);
	});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_pixelShader
				)
			);

		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer) , D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&constantBufferDesc,
				nullptr,
				&m_constantBuffer
				)
			);
	});

	// Once both shaders are loaded, create the mesh.
	auto createCubeTask = (createPSTask && createVSTask).then([this] () {
		CreateBlock(-3.0f, 0.0f, 0.0f);
		CreateBlock(3.0f, 0.0f, 0.0f);

		BuildGPUBuffers();
	});

	// Once the cube is loaded, the object is ready to be rendered.
	createCubeTask.then([this] () {
		m_loadingComplete = true;
	});
}

void Sample3DSceneRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_constantBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
}

void Sample3DSceneRenderer::CreateBlock(float posX, float posY, float posZ)
{
	auto block = Block();

	block.posX = posX;
	block.posY = posY;
	block.posZ = posZ;
	
	block.vertices[0] = VertexPositionColor{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f) };
	block.vertices[1] = VertexPositionColor{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) };
	block.vertices[2] = VertexPositionColor{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f) };
	block.vertices[3] = VertexPositionColor{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f) };
	block.vertices[4] = VertexPositionColor{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f) };
	block.vertices[5] = VertexPositionColor{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f) };
	block.vertices[6] = VertexPositionColor{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f) };
	block.vertices[7] = VertexPositionColor{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f) };

	this->blocks.push_back(block);
}

void Sample3DSceneRenderer::BuildGPUBuffers()
{
	// Add block vertices to scene.
	this->vertices.clear();
	this->indices.clear();

	for (auto blockIndex = 0; blockIndex < this->blocks.size(); ++blockIndex)
	{
		Block& block = this->blocks[blockIndex];

		for (auto i = 0; i < 8; ++i)
		{
			this->vertices.push_back(block.vertices[i]);

			auto indexOffset = blockIndex * 8;

			this->indices.push_back(blockIndex + 0);
			this->indices.push_back(blockIndex + 2);
			this->indices.push_back(blockIndex + 1);

			this->indices.push_back(blockIndex + 1);
			this->indices.push_back(blockIndex + 2);
			this->indices.push_back(blockIndex + 3);

			this->indices.push_back(blockIndex + 4);
			this->indices.push_back(blockIndex + 5);
			this->indices.push_back(blockIndex + 6);

			this->indices.push_back(blockIndex + 5);
			this->indices.push_back(blockIndex + 7);
			this->indices.push_back(blockIndex + 6);

			this->indices.push_back(blockIndex + 0);
			this->indices.push_back(blockIndex + 1);
			this->indices.push_back(blockIndex + 5);

			this->indices.push_back(blockIndex + 0);
			this->indices.push_back(blockIndex + 5);
			this->indices.push_back(blockIndex + 4);

			this->indices.push_back(blockIndex + 2);
			this->indices.push_back(blockIndex + 6);
			this->indices.push_back(blockIndex + 7);

			this->indices.push_back(blockIndex + 2);
			this->indices.push_back(blockIndex + 7);
			this->indices.push_back(blockIndex + 3);

			this->indices.push_back(blockIndex + 0);
			this->indices.push_back(blockIndex + 4);
			this->indices.push_back(blockIndex + 6);

			this->indices.push_back(blockIndex + 0);
			this->indices.push_back(blockIndex + 6);
			this->indices.push_back(blockIndex + 2);

			this->indices.push_back(blockIndex + 1);
			this->indices.push_back(blockIndex + 3);
			this->indices.push_back(blockIndex + 7);

			this->indices.push_back(blockIndex + 1);
			this->indices.push_back(blockIndex + 7);
			this->indices.push_back(blockIndex + 5);

			block.indexOffset = indexOffset;
		}
	}

	// Create vertex buffer.
	VertexPositionColor* vertexArray = &this->vertices[0];

	D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
	vertexBufferData.pSysMem = vertexArray;
	vertexBufferData.SysMemPitch = 0;
	vertexBufferData.SysMemSlicePitch = 0;
	CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColor) * this->vertices.size(), D3D11_BIND_VERTEX_BUFFER);
	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateBuffer(
		&vertexBufferDesc,
		&vertexBufferData,
		&m_vertexBuffer
		)
		);

	// Load mesh indices. Each trio of indices represents
	// a triangle to be rendered on the screen.
	// For example: 0,2,1 means that the vertices with indexes
	// 0, 2 and 1 from the vertex buffer compose the 
	// first triangle of this mesh.
	auto indexArray = &this->indices[0];

	m_indexCount = this->indices.size();

	D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
	indexBufferData.pSysMem = indexArray;
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;
	CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * m_indexCount, D3D11_BIND_INDEX_BUFFER);
	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateBuffer(
		&indexBufferDesc,
		&indexBufferData,
		&m_indexBuffer
		)
		);
}