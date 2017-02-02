#ifndef CRESOURCESELECTOR
#define CRESOURCESELECTOR

#include <Core/GameProject/CResourceEntry.h>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class CResourceSelector : public QWidget
{
    Q_OBJECT

    CResourceEntry *mpResEntry;

    // UI
    QHBoxLayout *mpLayout;
    QLabel *mpResNameLabel;
    QPushButton *mpSetButton;
    QPushButton *mpFindButton;
    QPushButton *mpClearButton;

    // Context Menu
    QAction *mpEditAssetAction;
    QAction *mpCopyNameAction;
    QAction *mpCopyPathAction;

public:
    explicit CResourceSelector(QWidget *pParent = 0);
    void SetAllowedExtensions(const QString& rkExtension);
    void SetAllowedExtensions(const TStringList& rkExtensions);
    void SetResource(const CAssetID& rkID);
    void SetResource(CResourceEntry *pEntry);
    void SetResource(CResource *pRes);

    // Accessors
    inline CResourceEntry* Entry() const    { return mpResEntry; }

public slots:
    void CreateContextMenu(const QPoint& rkPoint);
    void Set();
    void Find();
    void Clear();
    void EditAsset();
    void CopyName();
    void CopyPath();
    void OnResourceChanged();
    void UpdateUI();

signals:
    void ResourceChanged(CResourceEntry *pNewRes);
};

#endif // CRESOURCESELECTOR
