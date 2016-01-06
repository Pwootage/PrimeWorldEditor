#ifndef CSCENE_H
#define CSCENE_H

#include "CSceneNode.h"
#include "CRootNode.h"
#include "CLightNode.h"
#include "CModelNode.h"
#include "CScriptNode.h"
#include "CStaticNode.h"
#include "CCollisionNode.h"
#include "Core/Render/CRenderer.h"
#include "Core/Render/SViewInfo.h"
#include "Core/Resource/CGameArea.h"
#include "Core/Resource/CWorld.h"
#include "Core/CAreaAttributes.h"
#include "Core/SRayIntersection.h"
#include <Common/types.h>

#include <unordered_map>
#include <vector>

class CScene
{
    bool mSplitTerrain;

    u32 mNodeCount;
    std::vector<CModelNode*> mModelNodes;
    std::vector<CStaticNode*> mStaticNodes;
    std::vector<CCollisionNode*> mCollisionNodes;
    std::vector<CScriptNode*> mScriptNodes;
    std::vector<CLightNode*> mLightNodes;
    CRootNode *mpSceneRootNode;

    TResPtr<CGameArea> mpArea;
    TResPtr<CWorld> mpWorld;
    CRootNode *mpAreaRootNode;

    // Environment
    std::vector<CAreaAttributes> mAreaAttributesObjects;
    CAreaAttributes *mpActiveAreaAttributes;

    // Objects
    std::unordered_map<u32, CScriptNode*> mScriptNodeMap;

public:
    CScene();
    ~CScene();

    // Scene Management
    CModelNode* AddModel(CModel *mdl);
    CStaticNode* AddStaticModel(CStaticModel *mdl);
    CCollisionNode* AddCollision(CCollisionMeshGroup *mesh);
    CScriptNode* AddScriptObject(CScriptObject *obj);
    CLightNode* AddLight(CLight *Light);
    void SetActiveArea(CGameArea *_area);
    void SetActiveWorld(CWorld *_world);
    void ClearScene();
    void AddSceneToRenderer(CRenderer *pRenderer, const SViewInfo& ViewInfo);
    SRayIntersection SceneRayCast(const CRay& Ray, const SViewInfo& ViewInfo);
    void PickEnvironmentObjects();
    CScriptNode* ScriptNodeByID(u32 InstanceID);
    CScriptNode* NodeForObject(CScriptObject *pObj);
    CLightNode* NodeForLight(CLight *pLight);

    // Setters/Getters
    CModel* GetActiveSkybox();
    CGameArea* GetActiveArea();
};

#endif // CSCENE_H