#ifndef CCLONESELECTIONCOMMAND_H
#define CCLONESELECTIONCOMMAND_H

#include "IUndoCommand.h"
#include "ObjReferences.h"
#include "Editor/WorldEditor/CWorldEditor.h"

class CCloneSelectionCommand : public IUndoCommand
{
    CWorldEditor *mpEditor;
    CNodePtrList mOriginalSelection;
    CNodePtrList mNodesToClone;
    CNodePtrList mClonedNodes;

public:
    CCloneSelectionCommand(INodeEditor *pEditor);
    void undo();
    void redo();
    bool AffectsCleanState() const { return true; }
};

#endif // CCLONESELECTIONCOMMAND_H
