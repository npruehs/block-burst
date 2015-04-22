#pragma once

using namespace DirectX;

namespace BlockBurst
{
	struct Block
	{
		XMFLOAT3 position;
		XMFLOAT3 velocity;

		float rotation;

		VertexPositionColor vertices[8];

		int indexOffset;
	};
}