// Copyright draftDesk.

#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "DdNavToolset.h"

// Editor-only module hosting draftDesk's MCP toolsets. Toolsets are NOT auto-discovered — each must be
// explicitly registered with the ToolsetRegistry in StartupModule (mirrors EditorToolsetModule).
class FDraftDeskEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UDdNavToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		UToolsetRegistry::UnregisterToolsetClass(UDdNavToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FDraftDeskEditorModule, DraftDeskEditor);
