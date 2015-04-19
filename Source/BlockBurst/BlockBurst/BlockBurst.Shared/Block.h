#pragma once

namespace BlockBurst
{
	struct Block
	{
		float posX;
		float posY;
		float posZ;

		float rotation;

		VertexPositionColor vertices[8];

		int indexOffset;
	};
}