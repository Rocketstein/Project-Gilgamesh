#pragma once

// Selects whether a buffer is immutable or CPU-updatable after creation.
enum BufferUsage
{
	BUFFER_IMMUTABLE,
	BUFFER_DYNAMIC,
};
