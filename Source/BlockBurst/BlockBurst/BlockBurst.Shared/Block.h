#pragma once

using namespace DirectX;

namespace BlockBurst
{
	enum BlockType
	{
		Good,
		Bad,
		Dead
	};

	struct Block
	{
		XMFLOAT3 position;
		XMFLOAT3 velocity;

		float rotation;

		VertexPositionColor vertices[8];

		int indexOffset;

		BlockType blockType;

		float size;
	};
}