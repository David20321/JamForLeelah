#include "fbx/fbx.h"
#include <fbxsdk.h>
#include "fbx/ImportScene/DisplayCommon.h"
#include "fbx/ImportScene/DisplayHierarchy.h"
#include "fbx/ImportScene/DisplayAnimation.h"
#include "fbx/ImportScene/DisplayMarker.h"
#include "fbx/ImportScene/DisplaySkeleton.h"
#include "fbx/ImportScene/DisplayMesh.h"
#include "fbx/ImportScene/DisplayNurb.h"
#include "fbx/ImportScene/DisplayPatch.h"
#include "fbx/ImportScene/DisplayLodGroup.h"
#include "fbx/ImportScene/DisplayCamera.h"
#include "fbx/ImportScene/DisplayLight.h"
#include "fbx/ImportScene/DisplayGlobalSettings.h"
#include "fbx/ImportScene/DisplayPose.h"
#include "fbx/ImportScene/DisplayPivotsAndLimits.h"
#include "fbx/ImportScene/DisplayUserProperties.h"
#include "fbx/ImportScene/DisplayGenericInfo.h"
#include <SDL.h>
#include "platform_sdl/error.h"
#include "internal/common.h"
#include <cstdlib>
#include <stdint.h>
#include "glm/glm.hpp"

// Local function prototypes.
void DisplayContent(FbxScene* pScene);
void DisplayContent(FbxNode* pNode);
void DisplayTarget(FbxNode* pNode);
void DisplayTransformPropagation(FbxNode* pNode);
void DisplayGeometricTransform(FbxNode* pNode);
void DisplayMetaData(FbxScene* pScene);

// Most of this is copied from the FBX SDK ImportScene example

class FBXMemoryStream: public FbxStream {
private:
    EState state;
    int stream_pos;
    int file_size;
    const void* file_memory;
    int reader_id;

public:
    FBXMemoryStream( FbxManager* pSdkManager, const void* p_file_memory, int p_file_size)
        :file_memory(p_file_memory), file_size(p_file_size), 
         stream_pos(0), state(FbxStream::eClosed)
    {
        const char* format = "FBX (*.fbx)";
        reader_id = pSdkManager->GetIOPluginRegistry()->FindReaderIDByDescription( format );
    }

    virtual EState GetState() {
        return state;
    }

    virtual bool Open(void* pStreamData) {
        stream_pos = 0;
        state = eOpen;
        return true;
    }

    virtual bool Close() {
        state = eClosed;
        stream_pos = 0;
        return true;
    }

    virtual bool Flush() {
        return true;
    }

    /** Writes a memory block.
    * \param pData Pointer to the memory block to write.
    * \param pSize Size (in bytes) of the memory block to write.
    * \return The number of bytes written in the stream. */
    virtual int Write(const void* /*pData*/, int /*pSize*/) {
        return 0;
    }

    /** Read bytes from the stream and store them in the memory block.
    * \param pData Pointer to the memory block where the read bytes are stored.
    * \param pSize Number of bytes read from the stream.
    * \return The actual number of bytes successfully read from the stream. */
    virtual int Read(void* pData, int pSize) const {
        int size = pSize;
        if(stream_pos + size > file_size){
            size = file_size - stream_pos;
        }
        memcpy(pData, (void*)((int)file_memory+stream_pos), size);
        int* stream_pos_ptr = (int*)&stream_pos;
        *stream_pos_ptr += size;
        return size;
    }

    /** If not specified by KFbxImporter::Initialize(), the importer will ask
    * the stream to select an appropriate reader ID to associate with the stream.
    * FbxIOPluginRegistry can be used to locate id by extension or description.
    * Return -1 to allow FBX to select an appropriate default. */
    virtual int GetReaderID() const {
        return reader_id;
    }

    /** If not specified by KFbxExporter::Initialize(), the exporter will ask
    * the stream to select an appropriate writer ID to associate with the stream.
    * KFbxIOPluginRegistry can be used to locate id by extension or description.
    * Return -1 to allow FBX to select an appropriate default. */
    virtual int GetWriterID() const {
        return -1;
    }

    /** Adjust the current stream position.
    * \param pSeekPos Pre-defined position where offset is added (FbxFile::eBegin, FbxFile::eCurrent:, FbxFile::eEnd)
    * \param pOffset Number of bytes to offset from pSeekPos. */
    virtual void Seek(const FbxInt64& pOffset, const FbxFile::ESeekPos& pSeekPos) {
        switch(pSeekPos){
        case FbxFile::eBegin:
            stream_pos = (int)pOffset;
            break;
        case FbxFile::eCurrent:
            stream_pos += (int)pOffset;
            break;
        case FbxFile::eEnd:
            stream_pos = file_size + (int)pOffset;
            break;
        }
    }

    /** Get the current stream position.
    * \return Current number of bytes from the beginning of the stream. */
    virtual long GetPosition() const {
        return stream_pos;
    }

    /** Set the current stream position.
    * \param pPosition Number of bytes from the beginning of the stream to seek to. */
    virtual void SetPosition(long pPosition) {
        stream_pos = pPosition;
    }

    /** Return 0 if no errors occurred. Otherwise, return 1 to indicate
    * an error. This method will be invoked whenever FBX needs to verify
    * that the last operation succeeded. */
    virtual int GetError() const {
        return 0;
    }

    /** Clear current error condition by setting the current error value to 0. */
    virtual void ClearError() {
    }
};

void DisplayContent(FbxScene* pScene)
{
    int i;
    FbxNode* lNode = pScene->GetRootNode();

    if(lNode)
    {
        for(i = 0; i < lNode->GetChildCount(); i++)
        {
            DisplayContent(lNode->GetChild(i));
        }
    }

    DisplayAnimation(pScene);
}

void DisplayContent(FbxNode* pNode)
{
    FbxNodeAttribute::EType lAttributeType;
    int i;

    if(pNode->GetNodeAttribute() == NULL)
    {
        SDL_Log("NULL Node Attribute\n\n");
    }
    else
    {
        lAttributeType = (pNode->GetNodeAttribute()->GetAttributeType());

        switch (lAttributeType)
        {
        default:
            break;
        case FbxNodeAttribute::eMarker:  
            DisplayMarker(pNode);
            break;

        case FbxNodeAttribute::eSkeleton:  
            DisplaySkeleton(pNode);
            break;

        case FbxNodeAttribute::eMesh:      
            DisplayMesh(pNode);
            break;

        case FbxNodeAttribute::eNurbs:      
            DisplayNurb(pNode);
            break;

        case FbxNodeAttribute::ePatch:     
            DisplayPatch(pNode);
            break;

        case FbxNodeAttribute::eCamera:    
            DisplayCamera(pNode);
            break;

        case FbxNodeAttribute::eLight:     
            DisplayLight(pNode);
            break;

        case FbxNodeAttribute::eLODGroup:
            DisplayLodGroup(pNode);
            break;
        }   
    }

    DisplayUserProperties(pNode);
    DisplayTarget(pNode);
    DisplayPivotsAndLimits(pNode);
    DisplayTransformPropagation(pNode);
    DisplayGeometricTransform(pNode);

    for(i = 0; i < pNode->GetChildCount(); i++)
    {
        DisplayContent(pNode->GetChild(i));
    }
}


void DisplayTarget(FbxNode* pNode)
{
    if(pNode->GetTarget() != NULL)
    {
        DisplayString("    Target Name: ", (char *) pNode->GetTarget()->GetName());
    }
}

void DisplayTransformPropagation(FbxNode* pNode)
{
    SDL_Log("    Transformation Propagation\n");

    // 
    // Rotation Space
    //
    EFbxRotationOrder lRotationOrder;
    pNode->GetRotationOrder(FbxNode::eSourcePivot, lRotationOrder);

    SDL_Log("        Rotation Space: ");

    switch (lRotationOrder)
    {
    case eEulerXYZ: 
        SDL_Log("Euler XYZ\n");
        break;
    case eEulerXZY:
        SDL_Log("Euler XZY\n");
        break;
    case eEulerYZX:
        SDL_Log("Euler YZX\n");
        break;
    case eEulerYXZ:
        SDL_Log("Euler YXZ\n");
        break;
    case eEulerZXY:
        SDL_Log("Euler ZXY\n");
        break;
    case eEulerZYX:
        SDL_Log("Euler ZYX\n");
        break;
    case eSphericXYZ:
        SDL_Log("Spheric XYZ\n");
        break;
    }

    //
    // Use the Rotation space only for the limits
    // (keep using eEulerXYZ for the rest)
    //
    SDL_Log("        Use the Rotation Space for Limit specification only: %s\n",
        pNode->GetUseRotationSpaceForLimitOnly(FbxNode::eSourcePivot) ? "Yes" : "No");


    //
    // Inherit Type
    //
    FbxTransform::EInheritType lInheritType;
    pNode->GetTransformationInheritType(lInheritType);

    SDL_Log("        Transformation Inheritance: ");

    switch (lInheritType)
    {
    case FbxTransform::eInheritRrSs:
        SDL_Log("RrSs\n");
        break;
    case FbxTransform::eInheritRSrs:
        SDL_Log("RSrs\n");
        break;
    case FbxTransform::eInheritRrs:
        SDL_Log("Rrs\n");
        break;
    }
}

void DisplayGeometricTransform(FbxNode* pNode)
{
    FbxVector4 lTmpVector;

    SDL_Log("    Geometric Transformations\n");

    //
    // Translation
    //
    lTmpVector = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
    SDL_Log("        Translation: %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);

    //
    // Rotation
    //
    lTmpVector = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
    SDL_Log("        Rotation:    %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);

    //
    // Scaling
    //
    lTmpVector = pNode->GetGeometricScaling(FbxNode::eSourcePivot);
    SDL_Log("        Scaling:     %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);
}


void DisplayMetaData(FbxScene* pScene)
{
    FbxDocumentInfo* sceneInfo = pScene->GetSceneInfo();
    if (sceneInfo)
    {
        SDL_Log("\n\n--------------------\nMeta-Data\n--------------------\n\n");
        SDL_Log("    Title: %s\n", sceneInfo->mTitle.Buffer());
        SDL_Log("    Subject: %s\n", sceneInfo->mSubject.Buffer());
        SDL_Log("    Author: %s\n", sceneInfo->mAuthor.Buffer());
        SDL_Log("    Keywords: %s\n", sceneInfo->mKeywords.Buffer());
        SDL_Log("    Revision: %s\n", sceneInfo->mRevision.Buffer());
        SDL_Log("    Comment: %s\n", sceneInfo->mComment.Buffer());

        FbxThumbnail* thumbnail = sceneInfo->GetSceneThumbnail();
        if (thumbnail)
        {
            SDL_Log("    Thumbnail:\n");

            switch (thumbnail->GetDataFormat())
            {
            case FbxThumbnail::eRGB_24:
                SDL_Log("        Format: RGB\n");
                break;
            case FbxThumbnail::eRGBA_32:
                SDL_Log("        Format: RGBA\n");
                break;
            }

            switch (thumbnail->GetSize())
            {
            default:
                break;
            case FbxThumbnail::eNotSet:
                SDL_Log("        Size: no dimensions specified (%ld bytes)\n", thumbnail->GetSizeInBytes());
                break;
            case FbxThumbnail::e64x64:
                SDL_Log("        Size: 64 x 64 pixels (%ld bytes)\n", thumbnail->GetSizeInBytes());
                break;
            case FbxThumbnail::e128x128:
                SDL_Log("        Size: 128 x 128 pixels (%ld bytes)\n", thumbnail->GetSizeInBytes());
            }
        }
    }
}

void DisplayFBXInfo(FbxScene* scene) {
    DisplayMetaData(scene);
    SDL_Log("\n\n---------\nHierarchy\n---------\n\n");
    DisplayHierarchy(scene);
    SDL_Log("\n\n------------\nNode Content\n------------\n\n");
    DisplayContent(scene);
}

enum FBXParsePass {
    kCount, kStore
};

bool GetNormal(FbxMesh* fbx_mesh, int tri_index, int tri_vert, int layer, FbxVector4& normal) {
    FbxGeometryElementNormal* normal_el = fbx_mesh->GetElementNormal(layer);
    bool ret = false;
    int vert_index = fbx_mesh->GetPolygonVertex(tri_index, tri_vert);
    if(normal_el->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
        switch (normal_el->GetReferenceMode()) {
        case FbxGeometryElement::eDirect:
            normal = normal_el->GetDirectArray().GetAt(vert_index);
            ret = true;
            break;
        case FbxGeometryElement::eIndexToDirect: {
                int id = normal_el->GetIndexArray().GetAt(vert_index);
                normal = normal_el->GetDirectArray().GetAt(id);
                ret = true;
            } break;
        default:
            break;
        }
    }
    return ret;
}

bool GetUV(FbxMesh* fbx_mesh, int tri_index, int tri_vert, int layer, FbxVector2& uv) {
    bool ret = false;
    FbxGeometryElementUV* uv_el = fbx_mesh->GetElementUV(layer);
    switch (uv_el->GetMappingMode()) {
    case FbxGeometryElement::eByControlPoint: {
        int vert_index = fbx_mesh->GetPolygonVertex(tri_index, tri_vert);
        switch (uv_el->GetReferenceMode()) {
        case FbxGeometryElement::eDirect:
            uv = uv_el->GetDirectArray().GetAt(vert_index);
            ret = true;
            break;
        case FbxGeometryElement::eIndexToDirect: {
            int id = uv_el->GetIndexArray().GetAt(vert_index);
            uv = uv_el->GetDirectArray().GetAt(id);
            ret = true;
            } break;
        default:
            break; // other reference modes not shown here!
        }
        } break;
    case FbxGeometryElement::eByPolygonVertex: {
        int tex_uv_index = fbx_mesh->GetTextureUVIndex(tri_index, tri_vert);
        switch (uv_el->GetReferenceMode()) {
        case FbxGeometryElement::eDirect:
        case FbxGeometryElement::eIndexToDirect: {
            uv = uv_el->GetDirectArray().GetAt(tex_uv_index);
            ret = true;
            } break;
        default:
            break; // other reference modes not shown here!
        }
        } break;
    }
    return ret;
}

void ParseSkeleton(Skeleton* skeleton, FbxNode* node, FBXParsePass pass, int parent) {
    const char* name = node->GetName();
    if(name[0] == 'D' && name[1] == 'E' && name[2] == 'F'){
        int bone_id = skeleton->num_bones++;
        if(pass == kStore){
            FbxSkeleton* skeleton_node = (FbxSkeleton*)node->GetNodeAttribute();
            const char* types[] = { "Root", "Limb", "Limb Node", "Effector" };
            FbxSkeleton::EType e_type = skeleton_node->GetSkeletonType();
            const char* type = types[e_type];
            float size = 0.0f;
            switch(e_type){
            case FbxSkeleton::eLimb:{
                size = (float)skeleton_node->LimbLength.Get();
                } break;
            case FbxSkeleton::eLimbNode:{
                size = (float)skeleton_node->Size.Get();
                } break;
            case FbxSkeleton::eRoot: {
                size = (float)skeleton_node->Size.Get();
                } break;
            }
            const FbxAMatrix& transform = node->EvaluateGlobalTransform(FBXSDK_TIME_ZERO);
            Bone& bone = skeleton->bones[bone_id];
            FormatString(bone.name, Bone::kMaxBoneNameSize,
                         "%s", name);
            for(int i=0; i<16; ++i){
                bone.transform[i] = (float)transform[i/4][i%4];
            }
            bone.size = size;
            bone.parent = parent;
            bone.bone_id = node->GetUniqueID();
        }
        parent = bone_id;
    }
    for(int i=0, len=node->GetChildCount(); i<len; ++i) {
        ParseSkeleton(skeleton, node->GetChild(i), pass, parent);
    }
}


void SetBoneWeights(int bone_id, int num_bone_verts, int* bone_verts, double* bone_vert_weights, int* vert_bone_indices, float* vert_bone_weights) {
    static const float kWeightThreshold = 0.01f; // Don't bother storing vertex weights that are below this threshold
    int first_free_weight = 0;
    for(int i=0; i<num_bone_verts; ++i){
        float weight = (float)bone_vert_weights[i];
        if(weight > kWeightThreshold) {
            int bone_vert = bone_verts[i];
            int vert_bone_index = bone_vert * Mesh::kMaxWeightsPerVert;
            int first_free_slot = -1;
            for(int j=0; j<Mesh::kMaxWeightsPerVert; ++j){
                if(vert_bone_indices[vert_bone_index+j] == -1){
                    first_free_slot = j;
                    break;
                }
            }
            if(first_free_slot != -1){
                vert_bone_indices[vert_bone_index+first_free_slot] = bone_id;
                vert_bone_weights[vert_bone_index+first_free_slot] = weight;
            }
        }
    }
}

void CheckMeshDeformer(FbxMesh* fbx_mesh, int* vert_bone_indices, 
                       float* vert_bone_weights, uint64_t** bone_ids, 
                       int* num_bones_ptr, float** bind_matrices) 
{
    int num_bones;
    for(int pass=0; pass<2; ++pass){
        if(pass == 1){
            *num_bones_ptr = num_bones;
            *bone_ids = (uint64_t*)malloc(num_bones*sizeof(uint64_t));
            *bind_matrices = (float*)malloc(num_bones*sizeof(float)*16);
        }
        num_bones = 0;
        int num_skins = fbx_mesh->GetDeformerCount(FbxDeformer::eSkin);
        for(int i=0; i<num_skins; ++i)    {
            FbxSkin* skin = (FbxSkin *) fbx_mesh->GetDeformer(i, FbxDeformer::eSkin);
            int num_clusters = skin->GetClusterCount();
            for (int j=0; j<num_clusters; ++j) {
                FbxCluster* cluster = skin->GetCluster(j);
                int num_indices = cluster->GetControlPointIndicesCount();
                if(num_indices){
                    switch(pass) {
                    case 0: {
                        int* indices = cluster->GetControlPointIndices();
                        double* weights = cluster->GetControlPointWeights();
                        SetBoneWeights(num_bones, num_indices, indices, weights,
                                       vert_bone_indices, vert_bone_weights);
                        ++num_bones; 
                        } break;
                    case 1: 
                        (*bone_ids)[num_bones] = cluster->GetLink()->GetUniqueID();
                        FbxAMatrix transform_link_matrix;
                        transform_link_matrix = 
                            cluster->GetTransformLinkMatrix(transform_link_matrix);
                        for(int matrix_element=0, matrix_index=num_bones*16; matrix_element<16; ++matrix_element){
                            (*bind_matrices)[matrix_index++] = (float)transform_link_matrix[matrix_element/4][matrix_element%4];
                        }
                        ++num_bones; 
                        break;
                    }
                }
            }
        }
    }
}

void ParseNode(FBXParseScene* scene, FbxNode* node, FBXParsePass pass, int depth) {
    FbxNodeAttribute* node_attribute = node->GetNodeAttribute();
    bool check_children = true;
    if(node_attribute) {
        FbxNodeAttribute::EType type = node_attribute->GetAttributeType();
        switch (type) {
        case FbxNodeAttribute::eMesh: {
            switch(pass){
            case kStore: {
                FbxAMatrix matrix = node->EvaluateGlobalTransform(FBXSDK_TIME_INFINITE);
                FbxMesh* fbx_mesh = (FbxMesh*)node_attribute;
                Mesh* mesh = &scene->meshes[scene->num_mesh];
                ++scene->num_mesh;
                mesh->num_verts = fbx_mesh->GetControlPointsCount();
                FbxVector4* fbx_verts = fbx_mesh->GetControlPoints();
                mesh->vert_coords = (float*)malloc(sizeof(float) * mesh->num_verts * 3);
                for(int i=0, index=0; i<mesh->num_verts; ++i, index+=3){
                    FbxVector4 vert = matrix.MultT(fbx_verts[i]);
                    for(int j=0; j<3; ++j){
                        mesh->vert_coords[index+j] = (float)vert[j];
                    }
                }
                mesh->num_tris = fbx_mesh->GetPolygonCount();
                mesh->tri_indices = (unsigned*)malloc(sizeof(unsigned) * mesh->num_tris * 3);
                mesh->tri_uvs = (float*)malloc(sizeof(float) * mesh->num_tris * 6);
                mesh->tri_normals = (float*)malloc(sizeof(float) * mesh->num_tris * 9);
                for(int i=0, index=0, uv_index=0, normal_index=0; i<mesh->num_tris; ++i){
                    SDL_assert(fbx_mesh->GetPolygonSize(i) == 3); // Should have been triangulated earlier
                    for(int j=0; j<3; ++j){
                        mesh->tri_indices[index++] = fbx_mesh->GetPolygonVertex(i,j);
                        FbxVector2 uv;
                        bool ret = GetUV(fbx_mesh, i, j, 0, uv);
                        SDL_assert(ret);
                        for(int k=0; k<2; ++k){
                            mesh->tri_uvs[uv_index++] = (float)(uv[k]);
                        }
                        FbxVector4 normal;
                        ret = GetNormal(fbx_mesh, i, j, 0, normal);
                        SDL_assert(ret);
                        for(int k=0; k<3; ++k){
                            mesh->tri_normals[normal_index++] = (float)(normal[k]);
                        }
                    }
                }
                const int total_weights = Mesh::kMaxWeightsPerVert*mesh->num_verts;
                mesh->vert_bone_indices = (int*)malloc(sizeof(int)*total_weights);
                for(int i=0; i<total_weights; ++i){
                    mesh->vert_bone_indices[i] = -1;
                }
                mesh->vert_bone_weights = (float*)malloc(sizeof(float)*total_weights);
#ifdef _DEBUG
                for(int i=0; i<total_weights; ++i){
                    mesh->vert_bone_weights[i] = 0.0f;
                }
#endif
                CheckMeshDeformer(fbx_mesh, mesh->vert_bone_indices, mesh->vert_bone_weights, &mesh->bone_ids, &mesh->num_bones, &mesh->bind_matrices);
                } break;
            case kCount:
                ++scene->num_mesh;
                break;
            }
            } break;
        case FbxNodeAttribute::eSkeleton: {
            check_children = false;
            switch(pass){
            case kStore: {
                Skeleton* skeleton = &scene->skeletons[scene->num_skeleton];
                skeleton->num_animations = 0;
                skeleton->animations = NULL;
                skeleton->num_bones = 0;
                ++scene->num_skeleton;
                ParseSkeleton(skeleton, node, kCount, -1);
                skeleton->bones = (Bone*)malloc(sizeof(Bone)*skeleton->num_bones);
                skeleton->num_bones = 0;
                ParseSkeleton(skeleton, node, kStore, -1);
                } break;
            case kCount: {
                ++scene->num_skeleton;
                } break;
            }
            } break;
        default:
            SDL_Log("Unhandled node type\n");
            break;
        }   
    } 
    if(check_children){
        for(int i=0, len=node->GetChildCount(); i<len; ++i) {
            ParseNode(scene, node->GetChild(i), pass, depth+1);
        }
    }
}

bool MatchName(const char* name, const char** match_names, int num_names) {
    for(int i=0; i<num_names; ++i){
        if(strcmp(name, match_names[i]) == 0){
            return true;
        }
    }
    return (num_names == 0);
}

void ParseScene(FbxScene* scene, FBXParseScene* parse_scene, const char** specific_names, int num_names) {
    FbxNode* node = scene->GetRootNode();
    parse_scene->num_mesh = 0;
    parse_scene->num_skeleton = 0;
    if(node) {
        for(int i=0, len=node->GetChildCount(); i<len; ++i) {
            FbxNode* child = node->GetChild(i);
            if(MatchName(child->GetName(), specific_names, num_names)) {
                ParseNode(parse_scene, child, kCount, 0);
            }
        }
        parse_scene->meshes = (Mesh*)malloc(sizeof(Mesh)*parse_scene->num_mesh);
        parse_scene->num_mesh = 0;
        parse_scene->skeletons = (Skeleton*)malloc(sizeof(Skeleton)*parse_scene->num_skeleton);
        parse_scene->num_skeleton = 0;
        for(int i=0, len=node->GetChildCount(); i<len; ++i) {
            FbxNode* child = node->GetChild(i);
            if(MatchName(child->GetName(), specific_names, num_names)) {
                ParseNode(parse_scene, child, kStore, 0);
            }
        }
    }
}

void ParseFBXFromRAM(FBXParseScene* parse_scene, void* file_memory, int file_size, const char** specific_names, int num_names) {
    // Create manager and scene
    FbxManager* fbx_manager = FbxManager::Create();
    if( !fbx_manager ) {
        FormattedError("FBX error", "Unable to create FBX Manager.\n");
        exit(1);
    }
    fbx_manager->SetIOSettings(FbxIOSettings::Create(fbx_manager, IOSROOT));
    FbxScene* scene = FbxScene::Create(fbx_manager, "My Scene");
    if( !scene ) {
        FormattedError("FBX error", "Unable to create FBX scene.\n");
        exit(1);
    }

    { // Import data into scene
        FbxImporter* fbx_importer = FbxImporter::Create(fbx_manager,"");
        FBXMemoryStream fbx_mem_stream(fbx_manager, file_memory, file_size);
        if(!fbx_importer->Initialize(&fbx_mem_stream)) {
            FormattedError("FBX Importer Error", "Failed to initialize. %s", fbx_importer->GetStatus().GetErrorString());
            exit(1);
        }
        if(!fbx_importer->Import(scene)){
            FormattedError("FBX Importer Error", "Failed to import. %s", fbx_importer->GetStatus().GetErrorString());
            exit(1);
        }
        fbx_importer->Destroy();
    }
    {
        FbxGeometryConverter converter(fbx_manager);
        converter.Triangulate(scene, true, false);
    }

    for (int stack_index = 0, len=scene->GetSrcObjectCount<FbxAnimStack>(); 
         stack_index < len; 
         ++stack_index) 
    {
        FbxAnimStack* anim_stack = scene->GetSrcObject<FbxAnimStack>(stack_index);
        FbxTimeSpan time_span = anim_stack->GetLocalTimeSpan();
        FbxTime duration = time_span.GetDuration();
        double seconds = duration.GetSecondDouble();
        SDL_Log("Skeleton anim_stack: %s is %f seconds long", 
            anim_stack->GetName(), (float)seconds);
        if(stack_index==1){
            scene->SetCurrentAnimationStack(anim_stack);
        }
    }

    ParseScene(scene, parse_scene, specific_names, num_names);
    static const bool print_description = false;
    if(print_description){
        DisplayContent(scene);
    }

    // Dispose of manager and scene
    fbx_manager->Destroy();
}

Mesh::~Mesh() {
    SDL_assert(vert_coords == NULL);
    SDL_assert(tri_indices == NULL);
    SDL_assert(tri_uvs == NULL);
    SDL_assert(tri_normals == NULL);
    SDL_assert(vert_bone_weights == NULL);
    SDL_assert(vert_bone_indices == NULL);
    SDL_assert(bone_ids == NULL);
    SDL_assert(bind_matrices == NULL);
}

static void FreeAndNull(void** mem){
    free(*mem);
    *mem = NULL;
}

void Mesh::Dispose() {
    FreeAndNull((void**)&vert_coords);
    FreeAndNull((void**)&tri_indices);
    FreeAndNull((void**)&tri_uvs);
    FreeAndNull((void**)&tri_normals);
    FreeAndNull((void**)&vert_bone_weights);
    FreeAndNull((void**)&vert_bone_indices);
    FreeAndNull((void**)&bone_ids);
    FreeAndNull((void**)&bind_matrices);
}

void Skeleton::Dispose() {
    FreeAndNull((void**)&bones);
    for(int i=0; i<num_animations; ++i){
        animations[i].Dispose();
    }
    if(num_animations > 0){
        FreeAndNull((void**)&animations);
    }
}

Skeleton::~Skeleton() {
    SDL_assert(bones == NULL);
    SDL_assert(animations == NULL);
}

void FBXParseScene::Dispose() {
    for(int i=0; i<num_mesh; ++i){
        meshes[i].Dispose();
    }
    FreeAndNull((void**)&meshes);
    for(int i=0; i<num_skeleton; ++i){
        skeletons[i].Dispose();
    }
    FreeAndNull((void**)&skeletons);
}

FBXParseScene::~FBXParseScene() {
    SDL_assert(meshes == NULL);
    SDL_assert(skeletons == NULL);
}

struct BoneIDSort {
    uint64_t unique_id;
    int num;
};

static int BoneIDSortCompare(const void* a_ptr, const void* b_ptr){
    const BoneIDSort* a = (const BoneIDSort*)a_ptr;
    const BoneIDSort* b = (const BoneIDSort*)b_ptr;
    if(a->unique_id > b->unique_id) {
        return 1;
    } else if(a->unique_id == b->unique_id){
        return 0;
    } else {
        return -1;
    }
} 

void AttachMeshToSkeleton(Mesh* mesh, Skeleton* skeleton) {
    // rearrange mesh bone indices so they correspond to skeleton bones
    BoneIDSort* mesh_bone_ids = (BoneIDSort*)malloc(sizeof(BoneIDSort) * mesh->num_bones);
    for(int i=0; i<mesh->num_bones; ++i){
        mesh_bone_ids[i].num = i;
        mesh_bone_ids[i].unique_id = mesh->bone_ids[i];
    }
    qsort(mesh_bone_ids, mesh->num_bones, sizeof(BoneIDSort), BoneIDSortCompare);
    BoneIDSort* skeleton_bone_ids = (BoneIDSort*)malloc(sizeof(BoneIDSort) * skeleton->num_bones);
    for(int i=0; i<skeleton->num_bones; ++i){
        skeleton_bone_ids[i].num = i;
        skeleton_bone_ids[i].unique_id = skeleton->bones[i].bone_id;
    }
    qsort(skeleton_bone_ids, skeleton->num_bones, sizeof(BoneIDSort), BoneIDSortCompare);
    int* new_bone_ids = (int*)malloc(sizeof(int) * mesh->num_bones);
    for(int i=0; i<mesh->num_bones; ++i){
        new_bone_ids[i] = -1;
    }
    for(int mesh_index=0, skeleton_index=0; 
        mesh_index < mesh->num_bones && skeleton_index < skeleton->num_bones;)
    {
        BoneIDSort& skel = skeleton_bone_ids[skeleton_index];
        BoneIDSort& mesh = mesh_bone_ids[mesh_index];
        if(skel.unique_id == mesh.unique_id){
            new_bone_ids[mesh.num] = skel.num;
            ++mesh_index;
            ++skeleton_index;
        } else if(skel.unique_id > mesh.unique_id) {
            ++mesh_index;
        } else {
            ++skeleton_index;
        }
    }
    for(int i=0, index=0; i<mesh->num_verts*4; ++i){
        int& vert_bone_index = mesh->vert_bone_indices[index++];
        if(vert_bone_index != -1){
            vert_bone_index = new_bone_ids[vert_bone_index];
        }
    }

    // What to do with mesh->bind_matrices
    float* bind_matrices = (float*)malloc(16 * sizeof(float) * skeleton->num_bones);
    for(int i=0, index=0; i<skeleton->num_bones; ++i){
        for(int j=0; j<16; ++j){
            bind_matrices[index++] = (j/4 == j%4)?1.0f:0.0f;
        }
    }
    for(int i=0; i<mesh->num_bones; ++i){
        if(new_bone_ids[i] != -1){
            void *dst = &bind_matrices[new_bone_ids[i]*16];
            void *src = &mesh->bind_matrices[i*16];
            memcpy(dst, src, sizeof(float) * 16);
        }
    }
    free(mesh->bind_matrices);
    mesh->bind_matrices = bind_matrices;

    free(new_bone_ids);
    free(skeleton_bone_ids);
    free(mesh_bone_ids);
}

void GetBoundingBox(const Mesh* mesh, glm::vec3* bounding_box) {
    SDL_assert(mesh);
    SDL_assert(mesh->num_verts > 0);
    bounding_box[0] = glm::vec3(FLT_MAX);
    bounding_box[1] = glm::vec3(-FLT_MAX);
    for(int i=0, vert_index=0; i<mesh->num_verts; ++i){
        for(int k=0; k<3; ++k){
            bounding_box[0][k] = min(mesh->vert_coords[vert_index], bounding_box[0][k]);
            bounding_box[1][k] = max(mesh->vert_coords[vert_index], bounding_box[1][k]);
            ++vert_index;
        }
    }
}

void Animation::Dispose() {
    FreeAndNull((void**)&transforms);
}

Animation::~Animation() {
    SDL_assert(transforms == NULL);
}
