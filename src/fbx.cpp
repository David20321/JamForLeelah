#include "fbx/fbx.h"
#include <fbxsdk.h>
#include <SDL.h>

// Most of this is copied from the FBX SDK ImportScene example

class FBXMemoryStream: public FbxStream {
public:
	EState state;
	int stream_pos;

	int file_size;
	const void* file_memory;
	int reader_id;


	FBXMemoryStream( FbxManager* pSdkManager) {
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

static void InitializeSdkObjects(FbxManager*& pManager, FbxScene*& pScene)
{
	//The first thing to do is to create the FBX Manager which is the object allocator for almost all the classes in the SDK
	pManager = FbxManager::Create();
	if( !pManager )
	{
		SDL_Log("Error: Unable to create FBX Manager!\n");
		exit(1);
	}
	else SDL_Log("Autodesk FBX SDK version %s\n", pManager->GetVersion());

	//Create an IOSettings object. This object holds all import/export settings.
	FbxIOSettings* ios = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(ios);

	//Load plugins from the executable directory (optional)
	FbxString lPath = FbxGetApplicationDirectory();
	pManager->LoadPluginsDirectory(lPath.Buffer());

	//Create an FBX scene. This object holds most objects imported/exported from/to files.
	pScene = FbxScene::Create(pManager, "My Scene");
	if( !pScene )
	{
		SDL_Log("Error: Unable to create FBX scene!\n");
		exit(1);
	}
}

static bool LoadScene(FbxManager* pManager, FbxDocument* pScene, const void* file_memory, int file_size)
{
	int lFileMajor, lFileMinor, lFileRevision;
	int lSDKMajor,  lSDKMinor,  lSDKRevision;
	//int lFileFormat = -1;
	int i, lAnimStackCount;
	bool lStatus;

	// Get the file version number generate by the FBX SDK.
	FbxManager::GetFileFormatVersion(lSDKMajor, lSDKMinor, lSDKRevision);

	// Create an importer.
	FbxImporter* lImporter = FbxImporter::Create(pManager,"");

	FBXMemoryStream fbx_mem_stream(pManager);
	fbx_mem_stream.file_memory = file_memory;
	fbx_mem_stream.file_size = file_size;
	fbx_mem_stream.stream_pos = 0;
	fbx_mem_stream.state = FbxStream::eClosed;
	
	const bool lImportStatus = 
		lImporter->Initialize(&fbx_mem_stream);
	lImporter->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

	if( !lImportStatus )
	{
		FbxString error = lImporter->GetStatus().GetErrorString();
		SDL_Log("Call to FbxImporter::Initialize() failed.\n");
		SDL_Log("Error returned: %s\n\n", error.Buffer());

		if (lImporter->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion)
		{
			SDL_Log("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);
			SDL_Log("FBX file format version for file is %d.%d.%d\n\n", lFileMajor, lFileMinor, lFileRevision);
		}

		return false;
	}

	SDL_Log("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);

	if (lImporter->IsFBX())
	{
		SDL_Log("FBX file format version for file is %d.%d.%d\n\n", lFileMajor, lFileMinor, lFileRevision);

		// From this point, it is possible to access animation stack information without
		// the expense of loading the entire file.

		SDL_Log("Animation Stack Information\n");

		lAnimStackCount = lImporter->GetAnimStackCount();

		SDL_Log("    Number of Animation Stacks: %d\n", lAnimStackCount);
		SDL_Log("    Current Animation Stack: \"%s\"\n", lImporter->GetActiveAnimStackName().Buffer());
		SDL_Log("\n");

		for(i = 0; i < lAnimStackCount; i++)
		{
			FbxTakeInfo* lTakeInfo = lImporter->GetTakeInfo(i);

			SDL_Log("    Animation Stack %d\n", i);
			SDL_Log("         Name: \"%s\"\n", lTakeInfo->mName.Buffer());
			SDL_Log("         Description: \"%s\"\n", lTakeInfo->mDescription.Buffer());

			// Change the value of the import name if the animation stack should be imported 
			// under a different name.
			SDL_Log("         Import Name: \"%s\"\n", lTakeInfo->mImportName.Buffer());

			// Set the value of the import state to false if the animation stack should be not
			// be imported. 
			SDL_Log("         Import State: %s\n", lTakeInfo->mSelect ? "true" : "false");
			SDL_Log("\n");
		}
	}

	// Import the scene.
	lStatus = lImporter->Import(pScene);

	// Destroy the importer.
	lImporter->Destroy();

	return lStatus;
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


void FBXStuff(void* file_memory, int file_size) {
	FbxManager* lSdkManager = NULL;
	FbxScene* lScene = NULL;

	// Prepare the FBX SDK.
	InitializeSdkObjects(lSdkManager, lScene);
	// Load the scene.
	LoadScene(lSdkManager, lScene, file_memory, file_size);
	DisplayMetaData(lScene);
	SDL_Log("\n\n---------\nHierarchy\n---------\n\n");
	DisplayHierarchy(lScene);
	SDL_Log("\n\n------------\nNode Content\n------------\n\n");
	DisplayContent(lScene);

	// Destroy all objects created by the FBX SDK.
	if( lSdkManager ) lSdkManager->Destroy();
	exit(0);
}