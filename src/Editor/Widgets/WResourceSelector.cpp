#include "WResourceSelector.h"
#include "WTexturePreviewPanel.h"
#include "Editor/UICommon.h"
#include <Core/GameProject/CResourceStore.h>

#include <QApplication>
#include <QCompleter>
#include <QDesktopWidget>
#include <QDirModel>
#include <QEvent>
#include <QFileDialog>

WResourceSelector::WResourceSelector(QWidget *parent)
    : QWidget(parent)
    // Selector Members
    , mShowEditButton(false)
    , mShowExportButton(false)
    // Preview Panel Members
    , mpPreviewPanel(nullptr)
    , mEnablePreviewPanel(true)
    , mPreviewPanelValid(false)
    , mShowingPreviewPanel(false)
    , mAdjustPreviewToParent(false)
    // Resource Members
    , mResourceValid(false)
{
    // Create Widgets
    mUI.LineEdit = new QLineEdit(this);
    mUI.BrowseButton = new QPushButton(this);
    mUI.EditButton = new QPushButton("Edit", this);
    mUI.ExportButton = new QPushButton("Export", this);

    // Create Layout
    mUI.Layout = new QHBoxLayout(this);
    setLayout(mUI.Layout);
    mUI.Layout->addWidget(mUI.LineEdit);
    mUI.Layout->addWidget(mUI.BrowseButton);
    mUI.Layout->addWidget(mUI.EditButton);
    mUI.Layout->addWidget(mUI.ExportButton);
    mUI.Layout->setContentsMargins(0,0,0,0);
    mUI.Layout->setSpacing(1);

    // Set Up Widgets
    mUI.LineEdit->installEventFilter(this);
    mUI.LineEdit->setMouseTracking(true);
    mUI.LineEdit->setMaximumHeight(23);
    mUI.LineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mUI.BrowseButton->installEventFilter(this);
    mUI.BrowseButton->setMouseTracking(true);
    mUI.BrowseButton->setText("...");
    mUI.BrowseButton->setMaximumSize(25, 23);
    mUI.BrowseButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    mUI.EditButton->installEventFilter(this);
    mUI.EditButton->setMouseTracking(true);
    mUI.EditButton->setMaximumSize(50, 23);
    mUI.EditButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    mUI.EditButton->hide();
    mUI.ExportButton->installEventFilter(this);
    mUI.ExportButton->setMouseTracking(true);
    mUI.ExportButton->setMaximumSize(50, 23);
    mUI.ExportButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    mUI.ExportButton->hide();

    QCompleter *pCompleter = new QCompleter(this);
    pCompleter->setModel(new QDirModel(pCompleter));
    mUI.LineEdit->setCompleter(pCompleter);

    connect(mUI.LineEdit, SIGNAL(editingFinished()), this, SLOT(OnLineEditTextEdited()));
    connect(mUI.BrowseButton, SIGNAL(clicked()), this, SLOT(OnBrowseButtonClicked()));
}

WResourceSelector::~WResourceSelector()
{
    delete mpPreviewPanel;
}

bool WResourceSelector::event(QEvent *pEvent)
{
    if ((pEvent->type() == QEvent::Leave) || (pEvent->type() == QEvent::WindowDeactivate))
        HidePreviewPanel();

    return false;
}

bool WResourceSelector::eventFilter(QObject* /*pObj*/, QEvent *pEvent)
{
    if (pEvent->type() == QEvent::MouseMove)
        if (mEnablePreviewPanel)
            ShowPreviewPanel();

    return false;
}

bool WResourceSelector::IsSupportedExtension(const QString& rkExtension)
{
    foreach(const QString& str, mSupportedExtensions)
        if (str == rkExtension) return true;

    return false;
}

bool WResourceSelector::HasSupportedExtension(const CResourceInfo& rkRes)
{
    return IsSupportedExtension(TO_QSTRING(rkRes.Type().ToString()));
}

void WResourceSelector::UpdateFrameColor()
{
    bool RedFrame = false;

    // Red frame should only display if an incorrect resource path is entered. It shouldn't display on Invalid Asset ID.
    if (!mResourceValid)
    {
        TString Name = mResource.ToString().GetFileName(false);

        if (!Name.IsEmpty())
        {
            if (!Name.IsHexString() || (Name.Size() != 8 && Name.Size() != 16) || mResource.ID().IsValid())
                RedFrame = true;
        }
    }
    mUI.LineEdit->setStyleSheet(RedFrame ? "border: 1px solid red" : "");
    mUI.LineEdit->setFont(font());
}

// ************ GETTERS ************
CResourceInfo WResourceSelector::GetResourceInfo()
{
    return mResource;
}

CResource* WResourceSelector::GetResource()
{
    return mResource.Load();
}

QString WResourceSelector::GetText()
{
    return mUI.LineEdit->text();
}

bool WResourceSelector::IsEditButtonEnabled()
{
    return mShowEditButton;
}

bool WResourceSelector::IsExportButtonEnabled()
{
    return mShowExportButton;
}

bool WResourceSelector::IsPreviewPanelEnabled()
{
    return mEnablePreviewPanel;
}


// ************ SETTERS ************
void WResourceSelector::SetResource(CResource *pRes)
{
    if (pRes)
        SetResource(CResourceInfo(pRes->FullSource()));
    else
        SetResource(CResourceInfo());
}

void WResourceSelector::SetResource(const QString& rkRes)
{
    TString Res = TO_TSTRING(rkRes);
    TString Name = Res.GetFileName(false);
    TString Dir = Res.GetFileDirectory();
    TString Ext = Res.GetFileExtension();

    if (Dir.IsEmpty() && Name.IsHexString() && (Name.Size() == 8 || Name.Size() == 16) && Ext.Size() == 4)
        SetResource(CResourceInfo(Name.Size() == 8 ? Name.ToInt32() : Name.ToInt64(), Ext));
    else
        SetResource(CResourceInfo(Res));
}

void WResourceSelector::SetResource(const CResourceInfo& rkRes)
{
    if (mResource != rkRes)
    {
        mResource = rkRes;

        if (mResource.IsValid())
            mResourceValid = HasSupportedExtension(rkRes);
        else
            mResourceValid = false;

        TString ResStr = mResource.ToString();
        if (ResStr.Contains("FFFFFFFF", false)) mUI.LineEdit->clear();
        else mUI.LineEdit->setText(TO_QSTRING(ResStr));

        UpdateFrameColor();
        CreatePreviewPanel();
        SetButtonsBasedOnResType();
        emit ResourceChanged(TO_QSTRING(mResource.ToString()));
    }
}

void WResourceSelector::SetAllowedExtensions(const QString& rkExtension)
{
    TStringList list = TString(rkExtension.toStdString()).Split(",");
    SetAllowedExtensions(list);
}

void WResourceSelector::SetAllowedExtensions(const TStringList& rkExtensions)
{
    mSupportedExtensions.clear();
    for (auto it = rkExtensions.begin(); it != rkExtensions.end(); it++)
        mSupportedExtensions << TO_QSTRING(*it);
}

void WResourceSelector::SetText(const QString& rkResPath)
{
    mUI.LineEdit->setText(rkResPath);
    SetResource(rkResPath);
}

void WResourceSelector::SetEditButtonEnabled(bool Enabled)
{
    mShowEditButton = Enabled;
    if (Enabled) mUI.EditButton->show();
    else mUI.EditButton->hide();
}

void WResourceSelector::SetExportButtonEnabled(bool Enabled)
{
    mShowExportButton = Enabled;
    if (Enabled) mUI.ExportButton->show();
    else mUI.ExportButton->hide();
}

void WResourceSelector::SetPreviewPanelEnabled(bool Enabled)
{
    mEnablePreviewPanel = Enabled;
    if (!mPreviewPanelValid) CreatePreviewPanel();
}

void WResourceSelector::AdjustPreviewToParent(bool Adjust)
{
    mAdjustPreviewToParent = Adjust;
}

// ************ SLOTS ************
void WResourceSelector::OnLineEditTextEdited()
{
    SetResource(mUI.LineEdit->text());
}

void WResourceSelector::OnBrowseButtonClicked()
{
    // Construct filter string
    QString filter;

    if (mSupportedExtensions.size() > 1)
    {
        QString all = "All allowed extensions (";

        for (int iExt = 0; iExt < mSupportedExtensions.size(); iExt++)
        {
            if (iExt > 0) all += " ";
            all += "*." + mSupportedExtensions[iExt];
        }
        all += ")";
        filter += all + ";;";
    }

    for (int iExt = 0; iExt < mSupportedExtensions.size(); iExt++)
    {
        if (iExt > 0) filter += ";;";
        filter += UICommon::ExtensionFilterString(mSupportedExtensions[iExt]);
    }

    QString NewRes = QFileDialog::getOpenFileName(this, "Select resource", "", filter);

    if (!NewRes.isEmpty())
    {
        mUI.LineEdit->setText(NewRes);
        SetResource(NewRes);
    }
}

void WResourceSelector::OnEditButtonClicked()
{
    Edit();
}

void WResourceSelector::OnExportButtonClicked()
{
    Export();
}

// ************ PRIVATE ************
// Should the resource selector handle edit/export itself
// or delegate it entirely to the signals?
void WResourceSelector::Edit()
{
    emit EditResource(mResource);
}

void WResourceSelector::Export()
{
    emit ExportResource(mResource);
}

void WResourceSelector::CreatePreviewPanel()
{
    delete mpPreviewPanel;
    mpPreviewPanel = nullptr;

    if (mResourceValid)
        mpPreviewPanel = IPreviewPanel::CreatePanel(CResource::ResTypeForExtension(mResource.Type()), this);

    if (!mpPreviewPanel) mPreviewPanelValid = false;

    else
    {
        mPreviewPanelValid = true;
        mpPreviewPanel->setWindowFlags(Qt::ToolTip);
        if (mResourceValid) mpPreviewPanel->SetResource(mResource.Load());
    }
}

void WResourceSelector::ShowPreviewPanel()
{
    if (mPreviewPanelValid)
    {
        // Preferred panel point is lower-right, but can move if there's not enough room
        QPoint Position = parentWidget()->mapToGlobal(pos());
        QRect ScreenResolution = QApplication::desktop()->screenGeometry();
        QSize PanelSize = mpPreviewPanel->size();
        QPoint PanelPoint = Position;

        // Calculate parent adjustment with 9 pixels of buffer
        int ParentAdjustLeft = (mAdjustPreviewToParent ? pos().x() + 9 : 0);
        int ParentAdjustRight = (mAdjustPreviewToParent ? parentWidget()->width() - pos().x() + 9 : 0);

        // Is there enough space on the right?
        if (Position.x() + width() + PanelSize.width() + ParentAdjustRight >= ScreenResolution.width())
            PanelPoint.rx() -= PanelSize.width() + ParentAdjustLeft;
        else
            PanelPoint.rx() += width() + ParentAdjustRight;

        // Is there enough space on the bottom?
        if (Position.y() + PanelSize.height() >= ScreenResolution.height() - 30)
        {
            int Difference = Position.y() + PanelSize.height() - ScreenResolution.height() + 30;
            PanelPoint.ry() -= Difference;
        }

        mpPreviewPanel->move(PanelPoint);
        mpPreviewPanel->show();
        mShowingPreviewPanel = true;
    }
}

void WResourceSelector::HidePreviewPanel()
{
    if (mPreviewPanelValid && mShowingPreviewPanel)
    {
        mpPreviewPanel->hide();
        mShowingPreviewPanel = false;
    }
}

void WResourceSelector::SetButtonsBasedOnResType()
{
    // Basically this function sets whether the "Export" and "Edit"
    // buttons are present based on the resource type.
    if (!mResource.IsValid())
    {
        SetEditButtonEnabled(false);
        SetExportButtonEnabled(false);
    }

    else switch (CResource::ResTypeForExtension(mResource.Type()))
    {
    // Export button should be enabled here because CTexture already has a DDS export function
    // However, need to figure out what sort of interface to create to do it. Disabling until then.
    case eTexture:
        SetEditButtonEnabled(false);
        SetExportButtonEnabled(false);
        break;
    default:
        SetEditButtonEnabled(false);
        SetExportButtonEnabled(false);
        break;
    }
}
