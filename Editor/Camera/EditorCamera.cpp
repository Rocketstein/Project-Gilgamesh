#include "EditorCamera.h"

EditorCamera::EditorCamera()
{	
	currentState_ = pendingState_;
}

void EditorCamera::Update()
{
	if (isDirty_)
	{
		currentState_ = pendingState_;
		isDirty_ = false;
	}
}

