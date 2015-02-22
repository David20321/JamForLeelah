#include "platform_sdl/blender_file_io.h"
#include "platform_sdl/error.h"
#include "glm/glm.hpp"
#include "SDL.h"
#include <cstring>
#include "internal/common.h"

using namespace glm;

void LoadFileToRAM(const char *path, void** mem, int* size) {
    SDL_RWops* file = SDL_RWFromFile(path, "r");
    if(!file){
        FormattedError("Error", "Could not open file \"%s\" for reading", path);
        exit(1);
    }
    *size = (int)SDL_RWseek(file, 0, RW_SEEK_END);
    if(*size == -1){
        FormattedError("Error", "Could not get size of file \"%s\"", path);
        exit(1);
    }
    SDL_RWseek(file, 0, RW_SEEK_SET);
    *mem = malloc(*size);
    size_t read = SDL_RWread(file, *mem, *size, 1);
    if(read != 1){
        FormattedError("Error", "Failed to read data from file \"%s\", %s", path, SDL_GetError());
        exit(1);
    }
    SDL_RWclose(file);
}

void FileParseErr(const char* path, int line, const char* detail) {
    FormattedError("Error", "Line %d of file \"%s\"\n%s", line, path, detail);
    exit(1);
}

void ReadFloatArray(char* line, float* elements, int num_elements, int start, int end, const char* path, int line_num) {
    static const int kMaxElements = 32;
    int coord_read_pos[kMaxElements];
    SDL_assert(num_elements < kMaxElements);
    coord_read_pos[0] = start;
    int comma_pos = -1;
    int coord_read_index = 1;
    for(int comma_find=start; comma_find<end; ++comma_find){
        if(line[comma_find] == ','){
            line[comma_find] = '\0';
            coord_read_pos[coord_read_index++] = comma_find+1;
            if(coord_read_index > num_elements) {
                FileParseErr(path, line_num, "Too many commas");
            }
        }
        if(line[comma_find] == ')'){
            line[comma_find] = '\0';
        }
    }
    if(coord_read_index != num_elements) {
        FileParseErr(path, line_num, "Too few commas");
    }
    for(int i=0; i<num_elements; ++i){
        elements[i] = (float)atof(&line[coord_read_pos[i]]);
    }
}

enum ParsePass {
    kCount,
    kStore
};

class StringHashStore {
public:
    enum Err {
        kStringTooLong = -1,
        kTooManyStrings = -2,
        kStringCollision = -3
    };
    static const int kMaxStrings = 1024;
    static const int kMaxStringLength = 256;
    // Return Err on failure, otherwise index into array
    int StringIndex(const char* str);
    // These could be protected better
    int num_strings;
    char strings[kMaxStrings][kMaxStringLength];
private:
    int string_hash[kMaxStrings];
};

int StringHashStore::StringIndex(const char* str) {
    if(strlen(str) > kMaxStringLength){
        return kStringTooLong;
    }
    int hash_val = djb2_hash((unsigned char*)str);
    int index = -1;
    //TODO: This could be a binary search rather than linear if it becomes a bottleneck 
    for(int i=0; i<num_strings; ++i){
        if(hash_val == string_hash[i]){
            index = i;
            if(strcmp(str, strings[i]) != 0){
                return kStringCollision;
            }
            break;
        }
    }
    if(index == -1) {
        if(num_strings >= kMaxStrings) {
            return kTooManyStrings;
        }
        index = num_strings++;
        string_hash[index] = hash_val;
        strcpy(strings[index], str);
    }
    return index;
}

struct ParseMeshStraight {
    struct Vert {
        int index;
        vec3 coord;
        vec3 normal;
        int num_vert_groups;
        int vert_group_start_index;
    };
    struct Polygon {
        int num_verts;
        int polygon_vert_index;
    };
    struct PolygonVert {
        int vert;
        vec2 uv;
    };
    struct Action {
        Uint32 name_hash;
        int num_frames;
        int frame_index;
        int first_frame;
    };
    struct Bone {
        Uint32 name_hash;
        Uint32 parent_name_hash;
        mat4 rest_mat; // world space
    };
    struct Frame {
        int num_bones;
        int start_index;
    };
    struct FrameTransform {
        Uint32 name_hash; // of bone
        mat4 mat; // world space
    };
    struct VertGroup {
        Uint32 name_hash;
        float weight;
    };

    int num_verts;
    Vert* verts;
    int num_polygons;
    Polygon* polygons;
    int num_polygon_verts;
    PolygonVert* polygon_verts;
    int num_bones;
    Bone* bones;
    int num_frames;
    Frame* frames;
    int num_actions;
    Action* actions;
    int num_frame_transforms;
    FrameTransform* frame_transforms;
    int num_vert_groups;
    VertGroup* vert_groups;

    StringHashStore strings;

    int AllocSpace(void* mem);
};

int ParseMeshStraight::AllocSpace(void* mem) {
    // TODO: this seems pretty error-prone, put these all into arrays and just iterate
    int vert_space = num_verts * sizeof(Vert);
    int polygon_space = num_polygons * sizeof(Polygon);
    int polygon_vert_space = num_polygon_verts * sizeof(PolygonVert);
    int bone_space = num_bones * sizeof(Bone);
    int frame_space = num_frames * sizeof(Frame);
    int action_space = num_actions * sizeof(Action);
    int frame_transform_space = num_frame_transforms * sizeof(FrameTransform);
    int vert_group_space = num_vert_groups * sizeof(VertGroup);
    if(mem){
        intptr_t mem_index = (intptr_t)mem;
        verts      = (Vert*)mem_index;
        mem_index += vert_space;
        vert_groups = (VertGroup*)mem_index;
        mem_index += vert_group_space;
        polygons   = (Polygon*)mem_index;
        mem_index += polygon_space;
        polygon_verts = (PolygonVert*)mem_index;
        mem_index += polygon_vert_space;
        bones      = (Bone*)mem_index;
        mem_index += bone_space;
        actions    = (Action*)mem_index;
        mem_index += action_space;
        frames     = (Frame*)mem_index;
        mem_index += frame_space;
        frame_transforms = (FrameTransform*)mem_index;
        mem_index += frame_transform_space;
    }
    return vert_space + polygon_space + bone_space + frame_space + 
        action_space + frame_transform_space + polygon_vert_space +
        vert_group_space;
}

void ParseTestFileFromRam(const char* path, ParsePass pass, ParseMeshStraight* mesh, const char* const_file_str, int size) {
    char* file_str = (char*)malloc(size);
    memcpy(file_str, const_file_str, size);

    mesh->num_verts = 0;
    mesh->num_vert_groups = 0;
    mesh->num_polygons = 0;
    mesh->num_polygon_verts = 0;
    mesh->num_bones = 0;
    mesh->num_actions = 0;
    mesh->num_frames = 0;
    mesh->num_frame_transforms = 0;
    mesh->strings.num_strings = 0;

    static const int curr_version = 1;
    enum ParseState {
        kHeader,
        kVersion,
        kBegin,
        kMesh,
        kMeshVert,
        kMeshVertCoord,
        kMeshVertNormal,
        kMeshVertGroups,
        kMeshVertGroupSingle,
        kMeshPolygon,
        kMeshPolygonVertex,
        kMeshPolygonUV,
        kSkeleton,
        kSkeletonBone,
        kSkeletonBoneMatrix,
        kSkeletonBoneParent,
        kAction,
        kActionFrame,
        kActionFrameBone,
        kActionFrameBoneMatrix,
        kEnd
    };
    ParseState parse_state = kHeader;
    int line_start = 0;
    int line_num = 0;
    static const char* polygon_index_header = "  Polygon index: ";
    static const int polygon_index_header_len = strlen(polygon_index_header);
    static const char* skeleton_header = "Skeleton";
    static const int skeleton_header_len = strlen(skeleton_header);
    static const char* skeleton_bone_header = "  Bone: ";
    static const int skeleton_bone_header_len = strlen(skeleton_bone_header);
    static const char* skeleton_bone_matrix_header = "    Matrix: (";
    static const int skeleton_bone_matrix_header_len = strlen(skeleton_bone_matrix_header);
    static const char* skeleton_bone_parent_header = "    Parent: \"";
    static const int skeleton_bone_parent_header_len = strlen(skeleton_bone_parent_header);
    static const char* action_header = "Action: ";
    static const int action_header_len = strlen(action_header);
    static const char* action_frame_header = "  Frame: ";
    static const int action_frame_header_len = strlen(action_frame_header);
    static const char* action_frame_bone_header = "    Bone: ";
    static const int action_frame_bone_header_len = strlen(action_frame_bone_header);
    static const char* action_frame_bone_matrix_header = "      Matrix: (";
    static const int action_frame_bone_matrix_header_len = strlen(action_frame_bone_matrix_header);
    static const char* end_header = "--END--";
    static const int end_header_len = strlen(end_header);
    for(int i=0; i<size, parse_state!=kEnd; ++i){
        if(file_str[i] == '\n'){
            int length = i-line_start;
            if(length > 1){
                // End line before newline
                file_str[i] = '\0';
                if(i!=0 && file_str[i-1] == '\r') {
                    file_str[i-1] = '\0';
                }
                char* line = &file_str[line_start];
                //SDL_Log("Processing line \"%s\"", &file_str[line_start]);
                // Parse line
                bool repeat;
                do {
                    repeat = false;
                    switch(parse_state) {
                    case kHeader:
                        if(strcmp(line, "Wolfire JamForLeelah Format") != 0){
                            FileParseErr(path, line_num, "Invalid header");
                        }
                        parse_state = kVersion;
                        break;
                    case kVersion: {
                        static const char* version_str = "Version ";
                        static const int version_len = strlen(version_str);
                        if(strncmp(line, version_str, version_len) != 0){
                            FileParseErr(path, line_num, "Invalid version text");
                        }
                        int version_num = atoi(&line[version_len]);
                        if(version_num != curr_version) {
                            FileParseErr(path, line_num, "Invalid version number");
                        }
                        parse_state = kBegin;
                                   } break;
                    case kBegin: 
                        if(strcmp(line, "--BEGIN--") != 0){
                            FileParseErr(path, line_num, "Invalid BEGIN header");
                        }
                        parse_state = kMesh;
                        break;
                    case kMesh: 
                        if(strcmp(line, "Mesh") != 0){
                            FileParseErr(path, line_num, "Invalid Mesh header");
                        }
                        parse_state = kMeshVert;
                        break;
                    case kMeshVert: {
                        static const char* header = "  Vert ";
                        static const int header_len = strlen(header);
                        if(strncmp(line, header, header_len) != 0){
                            FileParseErr(path, line_num, "Invalid MeshVert header");
                        }
                        int vert_id = atoi(&line[header_len]);
                        ++mesh->num_verts;
                        if(pass == kStore){
                            mesh->verts[mesh->num_verts-1].index = vert_id;
                            mesh->verts[mesh->num_verts-1].num_vert_groups = 0;
                            mesh->verts[mesh->num_verts-1].vert_group_start_index = 
                                mesh->num_vert_groups;
                        }
                        parse_state = kMeshVertCoord;
                                    } break;
                    case kMeshVertCoord: {
                        static const char* header = "    Coords: (";
                        static const int header_len = strlen(header);
                        if(strncmp(line, header, header_len) != 0){
                            FileParseErr(path, line_num, "Invalid MeshVertCoords header");
                        }
                        float coords[3];
                        ReadFloatArray(line, coords, 3, header_len, length, path, line_num);
                        if(pass == kStore){
                            vec3& vec = mesh->verts[mesh->num_verts-1].coord;
                            for(int element=0; element<3; ++element){
                                vec[element] = coords[element];
                            }
                        }
                        parse_state = kMeshVertNormal;
                                         } break;
                    case kMeshVertNormal: {
                        static const char* header = "    Normals: (";
                        static const int header_len = strlen(header);
                        if(strncmp(line, header, header_len) != 0){
                            FileParseErr(path, line_num, "Invalid MeshVertNormals header");
                        }
                        float coords[3];
                        ReadFloatArray(line, coords, 3, header_len, length, path, line_num);
                        if(pass == kStore){
                            vec3& vec = mesh->verts[mesh->num_verts-1].normal;
                            for(int element=0; element<3; ++element){
                                vec[element] = coords[element];
                            }
                        }
                        parse_state = kMeshVertGroups;
                                          } break;
                    case kMeshVertGroups: 
                        if(strcmp(line, "    Vertex Groups:") != 0){
                            FileParseErr(path, line_num, "Invalid vertex groups header");
                        }
                        parse_state = kMeshVertGroupSingle;
                        break;
                    case kMeshVertGroupSingle: {
                        static const char* other_header = "  Vert ";
                        static const int other_header_len = strlen(other_header);
                        static const char* header=  "      \"";
                        static const int header_len = strlen(header);
                        if(strncmp(line, other_header, other_header_len) == 0){
                            parse_state = kMeshVert;
                            repeat = true;
                        } else if(strncmp(line, polygon_index_header, polygon_index_header_len) == 0){
                            parse_state = kMeshPolygon;
                            repeat = true;
                        } else if(strncmp(line, header, header_len) != 0){
                            FileParseErr(path, line_num, "Invalid single vertex group header");
                        } else {
                            // This is a vertex group header
                            int weight_start = -1;
                            for(int find_quote=header_len; find_quote<length; ++find_quote){
                                if(line[find_quote] == '"'){
                                    line[find_quote] = '\0';
                                    weight_start = find_quote+2;
                                    break;
                                }
                            }
                            const char* vert_group_bone = &line[header_len];
                            int hash_index = mesh->strings.StringIndex(vert_group_bone);
                            if(hash_index < 0){
                                FileParseErr(path, line_num, "Hash string problem");
                            }
                            float vert_group_weight = (float)atof(&line[weight_start]);
                            if(pass == kStore){
                                mesh->vert_groups[mesh->num_vert_groups].name_hash = hash_index;
                                mesh->vert_groups[mesh->num_vert_groups].weight = vert_group_weight;
                                ++mesh->verts[mesh->num_verts-1].num_vert_groups;
                            }
                            ++mesh->num_vert_groups;
                        }
                                               } break;
                    case kMeshPolygon: {
                        if(strncmp(line, polygon_index_header, polygon_index_header_len) != 0){
                            FileParseErr(path, line_num, "Invalid polygon header");
                        }
                        static const char* length_header = " length: ";
                        static const int length_header_len = strlen(length_header);
                        int comma_pos = -1;
                        for(int comma_find=polygon_index_header_len; comma_find<length; ++comma_find){
                            if(line[comma_find] == ','){
                                line[comma_find] = '\0';
                                comma_pos = comma_find+1;
                                break;
                            }
                        }
                        if(strncmp(&line[comma_pos], length_header, length_header_len) != 0){
                            FileParseErr(path, line_num, "Invalid polygon length header");
                        }
                        int polygon_index = atoi(&line[polygon_index_header_len]);
                        int polygon_length = atoi(&line[comma_pos+length_header_len]);
                        if(polygon_length<3) {
                            FileParseErr(path, line_num, "Polygons must have at least three sides");                            
                        }
                        if(pass == kStore){
                            mesh->polygons[mesh->num_polygons].num_verts = polygon_length;
                            mesh->polygons[mesh->num_polygons].polygon_vert_index = 
                                mesh->num_polygon_verts;
                        }
                        ++mesh->num_polygons;
                        parse_state = kMeshPolygonVertex;
                                       } break;
                    case kMeshPolygonVertex: {
                        static const char* header = "    Vertex: ";
                        static const int header_len = strlen(header);
                        if(strncmp(line, polygon_index_header, polygon_index_header_len) == 0){
                            repeat = true;
                            parse_state = kMeshPolygon;
                        } else if(strncmp(line, skeleton_header, skeleton_header_len) == 0){
                            repeat = true;
                            parse_state = kSkeleton;
                        } else if(strncmp(line, end_header, end_header_len) == 0){
                            parse_state = kEnd;
                        } else {
                            if(strncmp(line, header, header_len) != 0){
                                FileParseErr(path, line_num, "Invalid polygon vertex header");
                            }
                            int vert_index = atoi(&line[header_len]);
                            if(pass == kStore){
                                mesh->polygon_verts[mesh->num_polygon_verts].vert = vert_index;
                            }
                            ++mesh->num_polygon_verts;
                            parse_state = kMeshPolygonUV;
                        }
                                             } break;
                    case kMeshPolygonUV: {
                        static const char* header = "    UV: (";
                        static const int header_len = strlen(header);
                        if(strncmp(line, header, header_len) != 0){
                            FileParseErr(path, line_num, "Invalid polygon uv header");
                        }
                        int comma_pos = -1;
                        for(int comma_find=header_len; comma_find<length; ++comma_find){
                            if(line[comma_find] == ','){
                                line[comma_find] = '\0';
                                comma_pos = comma_find+1;
                            } else if(line[comma_find] == ')'){
                                line[comma_find] = '\0';
                                break;
                            }
                        }
                        vec2 uv;
                        uv[0] = (float)atof(&line[header_len]);
                        uv[1] = (float)atof(&line[comma_pos]);
                        if(pass == kStore){
                            mesh->polygon_verts[mesh->num_polygon_verts-1].uv = uv;
                        }
                        parse_state = kMeshPolygonVertex;
                                         } break;
                    case kSkeleton: 
                        if(strcmp(line, skeleton_header) != 0){
                            FileParseErr(path, line_num, "Invalid skeleton header");
                        }
                        parse_state = kSkeletonBone;
                        break;
                    case kSkeletonBone: {
                        if(strncmp(line, action_header, action_header_len) == 0){
                            parse_state = kAction;
                            repeat = true;
                        } else {
                            if(strncmp(line, skeleton_bone_header, skeleton_bone_header_len) != 0){
                                FileParseErr(path, line_num, "Invalid skeleton bone header");
                            }
                            const char* bone_name = &line[skeleton_bone_header_len];
                            int hash_index = mesh->strings.StringIndex(bone_name);
                            if(hash_index < 0){
                                FileParseErr(path, line_num, "Hash string problem");
                            }
                            if(pass == kStore){
                                mesh->bones[mesh->num_bones].name_hash = hash_index;
                            }
                            ++mesh->num_bones;
                            parse_state = kSkeletonBoneMatrix;
                        }
                                        } break;
                    case kSkeletonBoneMatrix: {
                        if(strncmp(line, skeleton_bone_matrix_header, skeleton_bone_matrix_header_len) != 0){
                            FileParseErr(path, line_num, "Invalid skeleton bone matrix header");
                        }
                        float coords[16];
                        ReadFloatArray(line, coords, 16, skeleton_bone_matrix_header_len, length, path, line_num);
                        parse_state = kSkeletonBoneParent;
                        if(pass == kStore){
                            for(int el=0; el<16; ++el){
                                mesh->bones[mesh->num_bones-1].rest_mat[el%4][el/4] = 
                                    coords[el];
                            }
                        }
                                              } break;
                    case kSkeletonBoneParent: {
                        if(strncmp(line, skeleton_bone_parent_header, skeleton_bone_parent_header_len) != 0){
                            FileParseErr(path, line_num, "Invalid skeleton bone parent header");
                        }
                        for(int quote_find=skeleton_bone_parent_header_len;
                            quote_find<length;
                            ++quote_find)
                        {
                            if(line[quote_find] == '\"'){
                                line[quote_find] = '\0';
                                break;
                            }
                        }
                        const char* parent_name = &line[skeleton_bone_parent_header_len];
                        int hash_index = mesh->strings.StringIndex(parent_name);
                        if(hash_index < 0){
                            FileParseErr(path, line_num, "Hash string problem");
                        }
                        if(pass == kStore){
                            mesh->bones[mesh->num_bones-1].parent_name_hash = hash_index;
                        }
                        parse_state = kSkeletonBone;
                                              } break;
                    case kAction: {
                        if(strncmp(line, action_header, action_header_len) != 0){
                            FileParseErr(path, line_num, "Invalid action header");
                        }
                        const char* action_name = &line[action_header_len];
                        int hash_index = mesh->strings.StringIndex(action_name);
                        if(hash_index < 0){
                            FileParseErr(path, line_num, "Hash string problem");
                        }
                        if(pass == kStore){
                            mesh->actions[mesh->num_actions].name_hash = hash_index;
                            mesh->actions[mesh->num_actions].num_frames = 0;
                            mesh->actions[mesh->num_actions].frame_index = mesh->num_frames;
                            mesh->actions[mesh->num_actions].first_frame = -1;
                        }
                        ++mesh->num_actions;
                        parse_state = kActionFrame;
                                  } break;
                    case kActionFrame: {
                        if(strncmp(line, action_frame_header, action_frame_header_len) != 0){
                            FileParseErr(path, line_num, "Invalid action frame header");
                        }
                        int frame = atoi(&line[action_frame_header_len]);
                        if(pass == kStore){
                            ++mesh->actions[mesh->num_actions-1].num_frames;
                            if(mesh->actions[mesh->num_actions-1].first_frame == -1){
                                mesh->actions[mesh->num_actions-1].first_frame = frame;
                            }
                            mesh->frames[mesh->num_frames].num_bones = 0;
                            mesh->frames[mesh->num_frames].start_index = mesh->num_frame_transforms;
                        }
                        ++mesh->num_frames;
                        parse_state = kActionFrameBone;
                                       } break;
                    case kActionFrameBone: {
                        if(strncmp(line, action_frame_header, action_frame_header_len) == 0){
                            parse_state = kActionFrame;
                            repeat = true;
                        } else if(strncmp(line, action_header, action_header_len) == 0){
                            parse_state = kAction;
                            repeat = true;
                        } else if(strncmp(line, end_header, end_header_len) == 0){
                            parse_state = kEnd;
                        } else {
                            if(strncmp(line, action_frame_bone_header, action_frame_bone_header_len) != 0){
                                FileParseErr(path, line_num, "Invalid action frame bone header");
                            }
                            const char* bone_name = &line[action_frame_bone_header_len];
                            int hash_index = mesh->strings.StringIndex(bone_name);
                            if(hash_index < 0){
                                FileParseErr(path, line_num, "Hash string problem");
                            }
                            if(pass == kStore){
                                ++mesh->frames[mesh->num_frames-1].num_bones;
                                mesh->frame_transforms[mesh->num_frame_transforms].name_hash = 
                                    hash_index;
                            }
                            parse_state = kActionFrameBoneMatrix;
                        }
                                           } break;
                    case kActionFrameBoneMatrix: {
                        if(strncmp(line, action_frame_bone_matrix_header, action_frame_bone_matrix_header_len) != 0){
                            FileParseErr(path, line_num, "Invalid action frame bone matrix header");
                        }
                        float coords[16];
                        ReadFloatArray(line, coords, 16, action_frame_bone_matrix_header_len, length, path, line_num);
                        parse_state = kActionFrameBone;
                        if(pass == kStore){
                            for(int el=0; el<16; ++el){
                                mesh->frame_transforms[mesh->num_frame_transforms].mat[el%4][el/4] = 
                                    coords[el];
                            }
                        }
                        ++mesh->num_frame_transforms;
                                                 } break;
                    default:
                        SDL_assert(false);
                    }
                } while(repeat);
                line_start = i+1;
                ++line_num;
            }
        }
    }
    free(file_str);
}

void ParseMesh::Dispose() {
    free(vert); vert = NULL;
    free(indices); indices = NULL;
    free(rest_mats); rest_mats = NULL;
    free(inverse_rest_mats); inverse_rest_mats = NULL;
    free(bone_parents); bone_parents = NULL;
    free(animations); animations = NULL;
    free(anim_transforms); anim_transforms = NULL;
}

ParseMesh::~ParseMesh()
{
    SDL_assert(vert == NULL);
    SDL_assert(indices == NULL);
    SDL_assert(rest_mats == NULL);
    SDL_assert(inverse_rest_mats == NULL);
    SDL_assert(bone_parents == NULL);
    SDL_assert(animations == NULL);
    SDL_assert(anim_transforms == NULL);
}

struct SortBone {
    int index;
    float weight;
};

int SortBonesByWeight(const void* a_ptr, const void* b_ptr) {
    SortBone* a = (SortBone*)a_ptr;
    SortBone* b = (SortBone*)b_ptr;
    if(a->weight < b->weight){
        return 1;
    } else if(a==b) {
        return 0;
    } else {
        return -1;
    }
};

mat4 BlenderMatToGame(const mat4 &blender_mat){
    mat4 mat;
    for(int row=0; row<4; ++row){
        for(int column=0; column<4; ++column){
            int src_column = column;
            if(column == 2){
                src_column = 1;
            }
            if(column == 1){
                src_column = 2;
            }
            mat[row][column] = blender_mat[row][src_column];
            if(column == 2){
                mat[row][column] *= -1.0f;
            }
        }
    }
    return mat;
}

void FinalMeshFromStraight(ParseMesh* mesh_final, ParseMeshStraight* mesh_straight) {
    bool skinned = (mesh_straight->num_bones != 0);
    
    // Prepare structure for easy lookup of the bone ID of a hash string
    int bone_id_from_hash[StringHashStore::kMaxStrings];
    for(int i=0; i<StringHashStore::kMaxStrings; ++i){
        bone_id_from_hash[i] = -1;
    }
    for(int i=0; i<mesh_straight->num_bones; ++i){
        bone_id_from_hash[mesh_straight->bones[i].name_hash] = i;
    }

    const int kFloatsPerVert = skinned?ParseMesh::kFloatsPerVert_Skinned:ParseMesh::kFloatsPerVert_Unskinned;
    float* vert_data;
    vert_data = (float*)malloc(sizeof(float)*kFloatsPerVert*mesh_straight->num_verts);
    int vert_data_index = 0;
    for(int i=0; i<mesh_straight->num_verts; ++i) {
        // Copy over vertex and normal data (switching order from Blender to game)
        ParseMeshStraight::Vert* vert = &mesh_straight->verts[i];
        vert_data[vert_data_index++] = vert->coord[0];
        vert_data[vert_data_index++] = vert->coord[2];
        vert_data[vert_data_index++] = vert->coord[1] * -1.0f;
        for(int el=0; el<2; ++el){
            vert_data[vert_data_index++] = 0.0f; //UV unknown at this point
        }
        vert_data[vert_data_index++] = vert->normal[0];
        vert_data[vert_data_index++] = vert->normal[2];
        vert_data[vert_data_index++] = vert->normal[1] * -1.0f;
        if(skinned){
            // The rest of this scope is about getting the 4 bone indices
            static const int kMaxIndicesToProcess = 32;
            SortBone sort_bone[kMaxIndicesToProcess];
            int num_bones = 0;
            // Get all deform bones affecting vert
            for(int j=0, vert_group_index = vert->vert_group_start_index; 
                j<vert->num_vert_groups; 
                ++j, ++vert_group_index) 
            {
                ParseMeshStraight::VertGroup* vert_group = 
                    &mesh_straight->vert_groups[vert_group_index];
                int bone_id = bone_id_from_hash[vert_group->name_hash];
                if(bone_id != -1){
                    sort_bone[num_bones].index = bone_id;
                    sort_bone[num_bones].weight = vert_group->weight;
                    ++num_bones;
                    SDL_assert(num_bones < kMaxIndicesToProcess);
                }
            }
            // Sort bones by weight
            if(num_bones > 1){
                qsort(sort_bone, num_bones, sizeof(SortBone), SortBonesByWeight);
            }
            // Clip down to 4 bones
            num_bones = min(4,num_bones);
            // Normalize to add up to 1
            float total_weight = 0.0f;
            for(int i=0; i<num_bones; ++i){
                total_weight += sort_bone[i].weight;
            }
            if(total_weight != 0.0f){
                for(int i=0; i<num_bones; ++i){
                    sort_bone[i].weight /= total_weight;
                }
            }
            for(int i=num_bones; i<4; ++i){
                sort_bone[i].weight = 0.0f;
                sort_bone[i].index = 0;
            }
            for(int i=0; i<4; ++i){
                vert_data[vert_data_index++] = (float)sort_bone[i].index;       
            }
            for(int i=0; i<4; ++i){
                vert_data[vert_data_index++] = sort_bone[i].weight;       
            }
        }
    }

    // Triangulate polygons
    int num_tris=0;
    for(int poly_index=0; 
        poly_index < mesh_straight->num_polygons;
        ++poly_index)
    {
        num_tris += mesh_straight->polygons[poly_index].num_verts-2;
    }

    int* tri_verts = (int*)malloc(sizeof(int) * 3 * num_tris);

    int tri_vert_index = 0;
    for(int poly_index=0; 
        poly_index < mesh_straight->num_polygons;
        ++poly_index)
    {
        ParseMeshStraight::Polygon& polygon = 
            mesh_straight->polygons[poly_index]; 
        // Simple triangle fan triangulation
        for(int tri=0, len=polygon.num_verts-2; tri<len; ++tri) {
            tri_verts[tri_vert_index++] = polygon.polygon_vert_index+0;
            tri_verts[tri_vert_index++] = polygon.polygon_vert_index+(tri+1);
            tri_verts[tri_vert_index++] = polygon.polygon_vert_index+(tri+2);
        }
    }

    float* vert_data_expanded;
    vert_data_expanded = (float*)malloc(sizeof(float)*kFloatsPerVert*num_tris*3);
    Uint32* indices = (Uint32*)malloc(sizeof(Uint32)*num_tris*3);

    int vert_data_expanded_index = 0;
    for(int i=0, len=num_tris*3; i<len; ++i){
        ParseMeshStraight::PolygonVert* polygon_vert = 
            &mesh_straight->polygon_verts[tri_verts[i]];
        memcpy(&vert_data_expanded[vert_data_expanded_index],
            &vert_data[polygon_vert->vert * kFloatsPerVert],
            sizeof(float) * kFloatsPerVert);
        vert_data_expanded[vert_data_expanded_index+3] = polygon_vert->uv[0];
        vert_data_expanded[vert_data_expanded_index+4] = polygon_vert->uv[1];
        indices[i] = i;
        vert_data_expanded_index += kFloatsPerVert;
    }

    mesh_final->num_vert = num_tris*3;
    mesh_final->vert = vert_data_expanded;
    mesh_final->num_index = num_tris*3;
    mesh_final->indices = indices;
    free(tri_verts);
    free(vert_data);

    if(skinned){
        // Process bones
        mesh_final->num_bones = mesh_straight->num_bones;
        mesh_final->rest_mats = (mat4*)malloc(sizeof(mat4)*mesh_straight->num_bones);
        mesh_final->inverse_rest_mats = (mat4*)malloc(sizeof(mat4)*mesh_straight->num_bones);
        mesh_final->bone_parents = (int*)malloc(sizeof(int)*mesh_straight->num_bones);
        for(int i=0; i<mesh_straight->num_bones; ++i){
            mesh_final->rest_mats[i] = BlenderMatToGame(mesh_straight->bones[i].rest_mat);
            mesh_final->inverse_rest_mats[i] = inverse(mesh_final->rest_mats[i]);
            mesh_final->bone_parents[i] = bone_id_from_hash[mesh_straight->bones[i].parent_name_hash];
        }

        // Process animations
        mesh_final->num_animations = mesh_straight->num_actions;
        mesh_final->animations = (ParseMesh::Animation*)malloc(sizeof(ParseMesh::Animation)*mesh_straight->num_actions);
        int num_anim_frames = 0;
        for(int i=0; i<mesh_final->num_animations; ++i){
            mesh_final->animations[i].num_frames = mesh_straight->actions[i].num_frames;
            mesh_final->animations[i].first_frame = mesh_straight->actions[i].first_frame;
            num_anim_frames += mesh_final->animations[i].num_frames;
        }
        int num_anim_transforms = num_anim_frames * mesh_final->num_bones;
        mesh_final->anim_transforms = (mat4*)malloc(sizeof(mat4)*num_anim_transforms);
        int anim_transform_index = 0;
        for(int i=0; i<mesh_final->num_animations; ++i){
            mesh_final->animations[i].anim_transform_start = anim_transform_index;
            for(int j=0; j<mesh_final->animations[i].num_frames; ++j){
                int frame_transform_index = mesh_straight->frames[mesh_straight->actions[i].frame_index+j].start_index;
                for(int k=0; k<mesh_final->num_bones; ++k){
                    int bone_id = bone_id_from_hash[mesh_straight->frame_transforms[frame_transform_index].name_hash];
                    SDL_assert(bone_id >= 0 && bone_id < mesh_final->num_bones);
                    mesh_final->anim_transforms[anim_transform_index+bone_id] = 
                        BlenderMatToGame(mesh_straight->frame_transforms[frame_transform_index].mat);
                    ++frame_transform_index;
                }
                anim_transform_index += mesh_final->num_bones;
            }
        }
    } else {
        mesh_final->rest_mats = NULL;
        mesh_final->inverse_rest_mats = NULL;
        mesh_final->bone_parents = NULL;
        mesh_final->animations = NULL;
        mesh_final->anim_transforms = NULL;
    }
}

void ParseTestFile(const char* path, ParseMesh* mesh_final){
    char* file_str;
    int size;
    LoadFileToRAM(path, (void**)&file_str, &size);
    ParseMeshStraight mesh_straight;
    ParseTestFileFromRam(path, kCount, &mesh_straight, file_str, size);
    int space_needed = mesh_straight.AllocSpace(NULL);
    void* space = malloc(space_needed);
    mesh_straight.AllocSpace(space);
    ParseTestFileFromRam(path, kStore, &mesh_straight, file_str, size);
    FinalMeshFromStraight(mesh_final, &mesh_straight);
    free(space);
}