#ifndef CRAYCOLLISIONHELPER_H
#define CRAYCOLLISIONHELPER_H

#include "SRayIntersection.h"
#include "Core/Render/SViewInfo.h"
#include "Core/Resource/Model/CBasicModel.h"
#include <Common/types.h>
#include <Math/CAABox.h>
#include <Math/CRay.h>
#include <Math/CVector3f.h>

#include <list>

class CSceneNode;

class CRayCollisionTester
{
    CRay mRay;
    std::list<SRayIntersection> mBoxIntersectList;

public:
    CRayCollisionTester(const CRay& rkRay);
    ~CRayCollisionTester();
    const CRay& Ray() const { return mRay; }

    void AddNode(CSceneNode *pNode, u32 AssetIndex, float Distance);
    void AddNodeModel(CSceneNode *pNode, CBasicModel *pModel);
    SRayIntersection TestNodes(const SViewInfo& rkViewInfo);
};

#endif // CRAYCOLLISIONHELPER_H
