#pragma once
#ifndef FBX_HPP
#define FBX_HPP

struct Mesh {
	int num_verts;
	float* vert_coords; // 3 per vert
	int num_tris;
	unsigned* tri_indices; // 3 per tri
	float* tri_uvs; // 6 per tri
	float* tri_normals; // 9 per tri

	void Dispose();
	~Mesh();
};

struct FBXParseScene {
	int num_mesh;
	Mesh* meshes;

	void Dispose();
	~FBXParseScene();
};

void ParseFBXFromRAM(FBXParseScene* scene, void* file_memory, int file_size);
void PrintFBXInfoFromRAM(void* file_memory, int file_size);

#endif