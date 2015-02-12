/****************************************************************************************

   Copyright (C) 2014 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk.h>
#include <SDL.h>

void DisplayPivotsAndLimits(FbxNode* pNode)
{
    FbxVector4 lTmpVector;

    //
    // Pivots
    //
    SDL_Log("    Pivot Information\n");

    FbxNode::EPivotState lPivotState;
    pNode->GetPivotState(FbxNode::eSourcePivot, lPivotState);
    SDL_Log("        Pivot State: %s\n", lPivotState == FbxNode::ePivotActive ? "Active" : "Reference");

    lTmpVector = pNode->GetPreRotation(FbxNode::eSourcePivot);
    SDL_Log("        Pre-Rotation: %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);

    lTmpVector = pNode->GetPostRotation(FbxNode::eSourcePivot);
    SDL_Log("        Post-Rotation: %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);

    lTmpVector = pNode->GetRotationPivot(FbxNode::eSourcePivot);
    SDL_Log("        Rotation Pivot: %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);

    lTmpVector = pNode->GetRotationOffset(FbxNode::eSourcePivot);
    SDL_Log("        Rotation Offset: %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);

    lTmpVector = pNode->GetScalingPivot(FbxNode::eSourcePivot);
    SDL_Log("        Scaling Pivot: %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);

    lTmpVector = pNode->GetScalingOffset(FbxNode::eSourcePivot);
    SDL_Log("        Scaling Offset: %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);

    //
    // Limits
    //
    bool        lIsActive, lMinXActive, lMinYActive, lMinZActive;
    bool        lMaxXActive, lMaxYActive, lMaxZActive;
    FbxDouble3    lMinValues, lMaxValues;

    SDL_Log("    Limits Information\n");

    lIsActive = pNode->TranslationActive;
    lMinXActive = pNode->TranslationMinX;
    lMinYActive = pNode->TranslationMinY;
    lMinZActive = pNode->TranslationMinZ;
    lMaxXActive = pNode->TranslationMaxX;
    lMaxYActive = pNode->TranslationMaxY;
    lMaxZActive = pNode->TranslationMaxZ;
    lMinValues = pNode->TranslationMin;
    lMaxValues = pNode->TranslationMax;

    SDL_Log("        Translation limits: %s\n", lIsActive ? "Active" : "Inactive");
    SDL_Log("            X\n");
    SDL_Log("                Min Limit: %s\n", lMinXActive ? "Active" : "Inactive");
    SDL_Log("                Min Limit Value: %f\n", lMinValues[0]);
    SDL_Log("                Max Limit: %s\n", lMaxXActive ? "Active" : "Inactive");
    SDL_Log("                Max Limit Value: %f\n", lMaxValues[0]);
    SDL_Log("            Y\n");
    SDL_Log("                Min Limit: %s\n", lMinYActive ? "Active" : "Inactive");
    SDL_Log("                Min Limit Value: %f\n", lMinValues[1]);
    SDL_Log("                Max Limit: %s\n", lMaxYActive ? "Active" : "Inactive");
    SDL_Log("                Max Limit Value: %f\n", lMaxValues[1]);
    SDL_Log("            Z\n");
    SDL_Log("                Min Limit: %s\n", lMinZActive ? "Active" : "Inactive");
    SDL_Log("                Min Limit Value: %f\n", lMinValues[2]);
    SDL_Log("                Max Limit: %s\n", lMaxZActive ? "Active" : "Inactive");
    SDL_Log("                Max Limit Value: %f\n", lMaxValues[2]);

    lIsActive = pNode->RotationActive;
    lMinXActive = pNode->RotationMinX;
    lMinYActive = pNode->RotationMinY;
    lMinZActive = pNode->RotationMinZ;
    lMaxXActive = pNode->RotationMaxX;
    lMaxYActive = pNode->RotationMaxY;
    lMaxZActive = pNode->RotationMaxZ;
    lMinValues = pNode->RotationMin;
    lMaxValues = pNode->RotationMax;

    SDL_Log("        Rotation limits: %s\n", lIsActive ? "Active" : "Inactive");
    SDL_Log("            X\n");
    SDL_Log("                Min Limit: %s\n", lMinXActive ? "Active" : "Inactive");
    SDL_Log("                Min Limit Value: %f\n", lMinValues[0]);
    SDL_Log("                Max Limit: %s\n", lMaxXActive ? "Active" : "Inactive");
    SDL_Log("                Max Limit Value: %f\n", lMaxValues[0]);
    SDL_Log("            Y\n");
    SDL_Log("                Min Limit: %s\n", lMinYActive ? "Active" : "Inactive");
    SDL_Log("                Min Limit Value: %f\n", lMinValues[1]);
    SDL_Log("                Max Limit: %s\n", lMaxYActive ? "Active" : "Inactive");
    SDL_Log("                Max Limit Value: %f\n", lMaxValues[1]);
    SDL_Log("            Z\n");
    SDL_Log("                Min Limit: %s\n", lMinZActive ? "Active" : "Inactive");
    SDL_Log("                Min Limit Value: %f\n", lMinValues[2]);
    SDL_Log("                Max Limit: %s\n", lMaxZActive ? "Active" : "Inactive");
    SDL_Log("                Max Limit Value: %f\n", lMaxValues[2]);

    lIsActive = pNode->ScalingActive;
    lMinXActive = pNode->ScalingMinX;
    lMinYActive = pNode->ScalingMinY;
    lMinZActive = pNode->ScalingMinZ;
    lMaxXActive = pNode->ScalingMaxX;
    lMaxYActive = pNode->ScalingMaxY;
    lMaxZActive = pNode->ScalingMaxZ;
    lMinValues = pNode->ScalingMin;
    lMaxValues = pNode->ScalingMax;

    SDL_Log("        Scaling limits: %s\n", lIsActive ? "Active" : "Inactive");
    SDL_Log("            X\n");
    SDL_Log("                Min Limit: %s\n", lMinXActive ? "Active" : "Inactive");
    SDL_Log("                Min Limit Value: %f\n", lMinValues[0]);
    SDL_Log("                Max Limit: %s\n", lMaxXActive ? "Active" : "Inactive");
    SDL_Log("                Max Limit Value: %f\n", lMaxValues[0]);
    SDL_Log("            Y\n");
    SDL_Log("                Min Limit: %s\n", lMinYActive ? "Active" : "Inactive");
    SDL_Log("                Min Limit Value: %f\n", lMinValues[1]);
    SDL_Log("                Max Limit: %s\n", lMaxYActive ? "Active" : "Inactive");
    SDL_Log("                Max Limit Value: %f\n", lMaxValues[1]);
    SDL_Log("            Z\n");
    SDL_Log("                Min Limit: %s\n", lMinZActive ? "Active" : "Inactive");
    SDL_Log("                Min Limit Value: %f\n", lMinValues[2]);
    SDL_Log("                Max Limit: %s\n", lMaxZActive ? "Active" : "Inactive");
    SDL_Log("                Max Limit Value: %f\n", lMaxValues[2]);
}

