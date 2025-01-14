#include "CPoiMapSidebar.h"
#include "ui_CPoiMapSidebar.h"
#include "CWorldEditor.h"
#include "Editor/UICommon.h"

#include <Core/Resource/Cooker/CPoiToWorldCooker.h>
#include <Core/Resource/Scan/CScan.h>
#include <Core/Resource/Script/CGameTemplate.h>
#include <Core/Resource/Script/NGameList.h>
#include <Core/ScriptExtra/CPointOfInterestExtra.h>

#include <QMouseEvent>
#include <QMessageBox>

const CColor CPoiMapSidebar::skNormalColor(0.137255f, 0.184314f, 0.776471f, 0.5f);
const CColor CPoiMapSidebar::skImportantColor(0.721569f, 0.066667f, 0.066667f, 0.5f);
const CColor CPoiMapSidebar::skHoverColor(0.047059f, 0.2f, 0.003922f, 0.5f);

CPoiMapSidebar::CPoiMapSidebar(CWorldEditor *pEditor)
    : CWorldEditorSidebar(pEditor)
    , ui(new Ui::CPoiMapSidebar)
    , mSourceModel(pEditor, this)
    , mHighlightMode(EHighlightMode::HighlightSelected)
    , mPickType(EPickType::NotPicking)
    , mpHoverModel(nullptr)
{
    mModel.setSourceModel(&mSourceModel);
    mModel.sort(0);

    ui->setupUi(this);
    ui->ListView->setModel(&mModel);
    ui->ListView->selectionModel()->select(mModel.index(0,0), QItemSelectionModel::Select | QItemSelectionModel::Current);
    SetHighlightSelected();

    connect(ui->HighlightSelectedButton, SIGNAL(pressed()), this, SLOT(SetHighlightSelected()));
    connect(ui->HighlightAllButton, SIGNAL(pressed()), this, SLOT(SetHighlightAll()));
    connect(ui->HighlightNoneButton, SIGNAL(pressed()), this, SLOT(SetHighlightNone()));
    connect(ui->ListView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(OnSelectionChanged(QItemSelection,QItemSelection)));
    connect(ui->ListView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(OnItemDoubleClick(QModelIndex)));
    connect(ui->MapMeshesButton, SIGNAL(clicked()), this, SLOT(OnPickButtonClicked()));
    connect(ui->UnmapMeshesButton, SIGNAL(clicked()), this, SLOT(OnPickButtonClicked()));
    connect(ui->UnmapAllButton, SIGNAL(clicked()), this, SLOT(OnUnmapAllPressed()));
    connect(ui->AddPoiFromViewportButton, SIGNAL(clicked()), this, SLOT(OnPickButtonClicked()));
    connect(ui->AddPoiFromInstanceListButton, SIGNAL(clicked()), this, SLOT(OnInstanceListButtonClicked()));
    connect(ui->RemovePoiButton, SIGNAL(clicked()), this, SLOT(OnRemovePoiButtonClicked()));
}

CPoiMapSidebar::~CPoiMapSidebar()
{
    delete ui;
}

void CPoiMapSidebar::SidebarOpen()
{
    Editor()->SetRenderingMergedWorld(false);
    UpdateModelHighlights();
}

void CPoiMapSidebar::SidebarClose()
{
    // Clear model highlights
    if (mHighlightMode != EHighlightMode::HighlightNone)
    {
        EHighlightMode OldHighlightMode = mHighlightMode;
        SetHighlightNone();
        mHighlightMode = OldHighlightMode;
    }

    // Stop picking
    if (mPickType != EPickType::NotPicking)
        StopPicking();

    // Disable unmerged world rendering
    Editor()->SetRenderingMergedWorld(true);
}

void CPoiMapSidebar::HighlightPoiModels(const QModelIndex& rkIndex)
{
    // Get POI and models
    QModelIndex SourceIndex = mModel.mapToSource(rkIndex);
    const QList<CModelNode*>& rkModels = mSourceModel.GetPoiMeshList(SourceIndex);
    bool Important = IsImportant(SourceIndex);

    // Highlight the meshes
    for (int iMdl = 0; iMdl < rkModels.size(); iMdl++)
    {
        rkModels[iMdl]->SetScanOverlayEnabled(true);
        rkModels[iMdl]->SetScanOverlayColor(Important ? skImportantColor : skNormalColor);
    }
}

void CPoiMapSidebar::UnhighlightPoiModels(const QModelIndex& rkIndex)
{
    QModelIndex SourceIndex = mModel.mapToSource(rkIndex);
    const QList<CModelNode*>& rkModels = mSourceModel.GetPoiMeshList(SourceIndex);

    for (int iMdl = 0; iMdl < rkModels.size(); iMdl++)
        RevertModelOverlay(rkModels[iMdl]);
}

void CPoiMapSidebar::HighlightModel(const QModelIndex& rkIndex, CModelNode *pNode)
{
    bool Important = IsImportant(rkIndex);
    pNode->SetScanOverlayEnabled(true);
    pNode->SetScanOverlayColor(Important ? skImportantColor : skNormalColor);
}

void CPoiMapSidebar::UnhighlightModel(CModelNode *pNode)
{
    pNode->SetScanOverlayEnabled(false);
}

void CPoiMapSidebar::RevertModelOverlay(CModelNode *pModel)
{
    if (pModel)
    {
        if (mHighlightMode == EHighlightMode::HighlightAll)
        {
            // Prioritize the selected POI over others.
            QModelIndex Selected = GetSelectedRow();

            if (mSourceModel.IsModelMapped(Selected, pModel))
                HighlightModel(Selected, pModel);

            // If it's not mapped to the selected POI, then check whether it's mapped to any others.
            else
            {
                for (int iRow = 0; iRow < mSourceModel.rowCount(QModelIndex()); iRow++)
                {
                    QModelIndex Index = mSourceModel.index(iRow, 0);

                    if (mSourceModel.IsModelMapped(Index, pModel))
                    {
                        HighlightModel(Index, pModel);
                        return;
                    }
                }

                UnhighlightModel(pModel);
            }
        }

        else if (mHighlightMode == EHighlightMode::HighlightSelected)
        {
            QModelIndex Index = GetSelectedRow();

            if (mSourceModel.IsModelMapped(Index, pModel))
                HighlightModel(Index, pModel);
            else
                UnhighlightModel(pModel);
        }

        else
            UnhighlightModel(pModel);
    }
}

CPoiMapSidebar::EPickType CPoiMapSidebar::GetRealPickType(bool AltPressed) const
{
    if (!AltPressed) return mPickType;
    if (mPickType == EPickType::AddMeshes) return EPickType::RemoveMeshes;
    return EPickType::AddMeshes;
}

bool CPoiMapSidebar::IsImportant(const QModelIndex& rkIndex)
{
    CScriptNode *pPOI = mSourceModel.PoiNodePointer(rkIndex);

    bool Important = false;
    TResPtr<CScan> pScan = static_cast<CPointOfInterestExtra*>(pPOI->Extra())->GetScan();

    if (pScan)
        Important = pScan->IsCriticalPropertyRef();

    return Important;
}

QModelIndex CPoiMapSidebar::GetSelectedRow() const
{
    QModelIndexList Indices = ui->ListView->selectionModel()->selectedRows();
    return ( Indices.isEmpty() ? QModelIndex() : mModel.mapToSource(Indices.front()) );
}

void CPoiMapSidebar::UpdateModelHighlights()
{
    const QItemSelection kSelection = ui->ListView->selectionModel()->selection();
    QList<QModelIndex> SelectedIndices;
    QList<QModelIndex> UnselectedIndices;

    for (int iRow = 0; iRow < mModel.rowCount(QModelIndex()); iRow++)
    {
        QModelIndex Index = mModel.index(iRow, 0);

        switch (mHighlightMode)
        {
        case EHighlightMode::HighlightSelected:
            if (kSelection.contains(Index))
                SelectedIndices << Index;
            else
                UnselectedIndices << Index;
            break;

        case EHighlightMode::HighlightAll:
            SelectedIndices << Index;
            break;

        case EHighlightMode::HighlightNone:
            UnselectedIndices << Index;
            break;
        }
    }

    foreach (const QModelIndex& rkIndex, UnselectedIndices)
        UnhighlightPoiModels(rkIndex);

    foreach (const QModelIndex& rkIndex, SelectedIndices)
        HighlightPoiModels(rkIndex);
}

void CPoiMapSidebar::SetHighlightSelected()
{
    mHighlightMode = EHighlightMode::HighlightSelected;
    UpdateModelHighlights();
}

void CPoiMapSidebar::SetHighlightAll()
{
    mHighlightMode = EHighlightMode::HighlightAll;
    UpdateModelHighlights();

    // Call HighlightPoiModels again on the selected index to prioritize it over the non-selected POIs.
    if (ui->ListView->selectionModel()->hasSelection())
        HighlightPoiModels(ui->ListView->selectionModel()->selectedRows().front());
}

void CPoiMapSidebar::SetHighlightNone()
{
    mHighlightMode = EHighlightMode::HighlightNone;
    UpdateModelHighlights();
}

void CPoiMapSidebar::OnSelectionChanged(const QItemSelection& rkSelected, const QItemSelection& rkDeselected)
{
    if (mHighlightMode == EHighlightMode::HighlightSelected)
    {
        // Clear highlight on deselected models
        QModelIndexList DeselectedIndices = rkDeselected.indexes();

        for (int iIdx = 0; iIdx < DeselectedIndices.size(); iIdx++)
            UnhighlightPoiModels(DeselectedIndices[iIdx]);

        // Highlight newly selected models
        QModelIndexList SelectedIndices = rkSelected.indexes();

        for (int iIdx = 0; iIdx < SelectedIndices.size(); iIdx++)
            HighlightPoiModels(SelectedIndices[iIdx]);
    }
}

void CPoiMapSidebar::OnItemDoubleClick(QModelIndex Index)
{
    QModelIndex SourceIndex = mModel.mapToSource(Index);
    CScriptNode *pPOI = mSourceModel.PoiNodePointer(SourceIndex);
    Editor()->ClearAndSelectNode(pPOI);
}

void CPoiMapSidebar::OnUnmapAllPressed()
{
    QModelIndex Index = GetSelectedRow();
    QList<CModelNode*> ModelList = mSourceModel.GetPoiMeshList(Index);

    foreach (CModelNode *pModel, ModelList)
    {
        mSourceModel.RemoveMapping(Index, pModel);
        RevertModelOverlay(pModel);
    }
}

void CPoiMapSidebar::OnPickButtonClicked()
{
    QPushButton *pButton = qobject_cast<QPushButton*>(sender());

    if (pButton == ui->AddPoiFromViewportButton)
    {
        Editor()->EnterPickMode(ENodeType::Script, true, false, false);
        connect(Editor(), SIGNAL(PickModeExited()), this, SLOT(StopPicking()));
        connect(Editor(), SIGNAL(PickModeClick(SRayIntersection,QMouseEvent*)), this, SLOT(OnPoiPicked(SRayIntersection,QMouseEvent*)));

        pButton->setChecked(true);
        ui->MapMeshesButton->setChecked(false);
        ui->UnmapMeshesButton->setChecked(false);

        mPickType = EPickType::AddPOIs;
    }

    else
    {
        if (!pButton->isChecked())
            Editor()->ExitPickMode();

        else
        {
            Editor()->EnterPickMode(ENodeType::Model, false, false, true);
            connect(Editor(), SIGNAL(PickModeExited()), this, SLOT(StopPicking()));
            connect(Editor(), SIGNAL(PickModeHoverChanged(SRayIntersection,QMouseEvent*)), this, SLOT(OnModelHover(SRayIntersection,QMouseEvent*)));
            pButton->setChecked(true);

            if (pButton == ui->MapMeshesButton)
            {
                mPickType = EPickType::AddMeshes;
                ui->UnmapMeshesButton->setChecked(false);
            }

            else if (pButton == ui->UnmapMeshesButton)
            {
                mPickType = EPickType::RemoveMeshes;
                ui->MapMeshesButton->setChecked(false);
            }
        }
    }
}

void CPoiMapSidebar::StopPicking()
{
    ui->MapMeshesButton->setChecked(false);
    ui->UnmapMeshesButton->setChecked(false);
    ui->AddPoiFromViewportButton->setChecked(false);
    mPickType = EPickType::NotPicking;

    RevertModelOverlay(mpHoverModel);
    mpHoverModel = nullptr;

    Editor()->ExitPickMode();
    disconnect(Editor(), SIGNAL(PickModeExited()), this, 0);
    disconnect(Editor(), SIGNAL(PickModeHoverChanged(SRayIntersection,QMouseEvent*)), this, 0);
    disconnect(Editor(), SIGNAL(PickModeClick(SRayIntersection,QMouseEvent*)), this, 0);
}

void CPoiMapSidebar::OnInstanceListButtonClicked()
{
    EGame Game = Editor()->CurrentGame();
    CScriptTemplate *pPoiTemplate = NGameList::GetGameTemplate(Game)->TemplateByID("POIN");

    CPoiListDialog Dialog(pPoiTemplate, &mSourceModel, Editor()->Scene(), this);
    Dialog.exec();

    const QList<CScriptNode*>& rkSelection = Dialog.Selection();

    if (!rkSelection.empty())
    {
        foreach (CScriptNode *pNode, rkSelection)
            mSourceModel.AddPOI(pNode);

        mModel.sort(0);
    }
}

void CPoiMapSidebar::OnRemovePoiButtonClicked()
{
    if (ui->ListView->selectionModel()->hasSelection())
    {
        QModelIndex Index = ui->ListView->selectionModel()->selectedRows().front();
        UnhighlightPoiModels(Index);
        Index = mModel.mapToSource(Index);
        mSourceModel.RemovePOI(Index);
    }
}

void CPoiMapSidebar::OnPoiPicked(const SRayIntersection& rkIntersect, QMouseEvent *pEvent)
{
    CScriptNode *pPOI = static_cast<CScriptNode*>(rkIntersect.pNode);
    if (pPOI->Instance()->ObjectTypeID() != CFourCC("POIN").ToLong()) return;

    mSourceModel.AddPOI(pPOI);
    mModel.sort(0);

    // Exit pick mode unless the user is holding the Ctrl key
    if (!(pEvent->modifiers() & Qt::ControlModifier))
        Editor()->ExitPickMode();
}

void CPoiMapSidebar::OnModelPicked(const SRayIntersection& rkRayIntersect, QMouseEvent* pEvent)
{
    if (!rkRayIntersect.pNode) return;

    // Check for valid selection
    QModelIndexList Indices = ui->ListView->selectionModel()->selectedRows();
    if (Indices.isEmpty()) return;

    // Map selection to source model
    QModelIndexList SourceIndices;
    for (auto it = Indices.begin(); it != Indices.end(); it++)
        SourceIndices << mModel.mapToSource(*it);

    // If alt is pressed, invert the pick mode
    CModelNode *pModel = static_cast<CModelNode*>(rkRayIntersect.pNode);

    bool AltPressed = (pEvent->modifiers() & Qt::AltModifier) != 0;
    EPickType PickType = GetRealPickType(AltPressed);

    // Add meshes
    if (PickType == EPickType::AddMeshes)
    {
        for (auto it = SourceIndices.begin(); it != SourceIndices.end(); it++)
            mSourceModel.AddMapping(*it, pModel);

        if (mHighlightMode != EHighlightMode::HighlightNone)
            HighlightModel(SourceIndices.front(), pModel);
    }

    // Remove meshes
    else if (PickType == EPickType::RemoveMeshes)
    {
        for (auto it = SourceIndices.begin(); it != SourceIndices.end(); it++)
            mSourceModel.RemoveMapping(*it, pModel);

        if (mHighlightMode != EHighlightMode::HighlightNone)
            RevertModelOverlay(mpHoverModel);
        else
            UnhighlightModel(pModel);
    }
}

void CPoiMapSidebar::OnModelHover(const SRayIntersection& rkIntersect, QMouseEvent *pEvent)
{
    // Restore old hover model to correct overlay color, and set new hover model
    if (mpHoverModel)
        RevertModelOverlay(mpHoverModel);

    mpHoverModel = static_cast<CModelNode*>(rkIntersect.pNode);

    // If the left mouse button is pressed, treat this as a click.
    if (pEvent->buttons() & Qt::LeftButton)
        OnModelPicked(rkIntersect, pEvent);

    // Otherwise, process as a mouseover
    else
    {
        QModelIndex Index = GetSelectedRow();

        // Process new hover model
        if (mpHoverModel)
        {
            bool AltPressed = (pEvent->modifiers() & Qt::AltModifier) != 0;
            EPickType PickType = GetRealPickType(AltPressed);

            if ( ((PickType == EPickType::AddMeshes) && !mSourceModel.IsModelMapped(Index, mpHoverModel)) ||
                 ((PickType == EPickType::RemoveMeshes) && mSourceModel.IsModelMapped(Index, mpHoverModel)) )
            {
                mpHoverModel->SetScanOverlayEnabled(true);
                mpHoverModel->SetScanOverlayColor(skHoverColor);
            }
        }
    }
}
