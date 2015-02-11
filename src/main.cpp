#include "SDL.h"
#include "GL/glew.h"
#define GLM_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/gtx/quaternion.hpp"
#include <cstdio>
#include "fbx/fbx.h"
#include "platform_sdl/error.h"
#include "platform_sdl/audio.h"
#include "platform_sdl/graphics.h"
#include "platform_sdl/file_io.h"
#include "platform_sdl/debug_draw.h"
#include "internal/common.h"
#include "internal/memory.h"
#include <cstring>
#include <sys/stat.h>

#ifdef WIN32
#define ASSET_PATH "../assets/"
#else
#define ASSET_PATH "assets/"
#endif

using namespace glm;

enum VBO_Setup {
	kSimple_4V,
	kInterleave_3V2T3N
};

struct Drawable {
	int texture_id;
	int vert_vbo;
	int index_vbo;
	int num_indices;
	int shader_id;
	VBO_Setup vbo_layout;
	mat4 transform;
};

struct SeparableTransform {
	quat rotation;
	vec3 scale;
	vec3 translation;
	mat4 GetCombination();
};

struct GameState {
	SeparableTransform camera;
};

mat4 SeparableTransform::GetCombination() {
	mat4 mat;
	for(int i=0; i<3; ++i){
		mat[i][i] = scale[i];
	}
	for(int i=0; i<3; ++i){
		mat[i] = rotation * mat[i];
	}
	for(int i=0; i<3; ++i){
		mat[3][i] += translation[i];
	}
	return mat;
}

struct DrawScene {
	static const int kMaxDrawables = 100;
	Drawable drawables[kMaxDrawables];
	int num_drawables;
	DebugDrawLines lines;
};

void Update(GameState* game_state, const vec2& mouse_rel, float time_step) {
	float speed = 10.0f;
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_SPACE]) {
        speed *= 0.1f;
    }
	if (state[SDL_SCANCODE_W]) {
		game_state->camera.translation -= game_state->camera.rotation * vec3(0,0,1) * speed * time_step;
	}
	if (state[SDL_SCANCODE_S]) {
		game_state->camera.translation += game_state->camera.rotation * vec3(0,0,1) * speed * time_step;
	}
	if (state[SDL_SCANCODE_A]) {
		game_state->camera.translation -= game_state->camera.rotation * vec3(1,0,0) * speed * time_step;
	}
	if (state[SDL_SCANCODE_D]) {
		game_state->camera.translation += game_state->camera.rotation * vec3(1,0,0) * speed * time_step;
	}
	const float kMouseSensitivity = 0.003f;
    Uint32 mouse_button_bitmask = SDL_GetMouseState(NULL, NULL);
    static float cam_x = 0.0f;
    static float cam_y = 0.0f;
    if(mouse_button_bitmask & SDL_BUTTON_LEFT){
        cam_x -= mouse_rel.y * kMouseSensitivity;
        cam_y -= mouse_rel.x * kMouseSensitivity;
        quat xRot = angleAxis(cam_x,vec3(1,0,0));
        quat yRot = angleAxis(cam_y,vec3(0,1,0));
        game_state->camera.rotation = yRot * xRot;
    }
    game_state->camera.scale = vec3(1.0f);
}

void DrawCoordinateGrid(DrawScene* draw_scene){
	static const float opac = 0.25f;
	static const vec4 basic_grid_color(1.0f, 1.0f, 1.0f, opac);
	static const vec4 x_axis_color(1.0f, 0.0f, 0.0f, opac);
	static const vec4 y_axis_color(0.0f, 1.0f, 0.0f, opac);
	static const vec4 z_axis_color(0.0f, 0.0f, 1.0f, opac);
    for(int i=-10; i<11; ++i){
        draw_scene->lines.Add(vec3(-10.0f, 0.0f, i), vec3(10.0f, 0.0f, i),
							  i==0?x_axis_color:basic_grid_color, kDraw, 1);
        draw_scene->lines.Add(vec3(i, 0.0f, -10.0f), vec3(i, 0.0f, 10.0f),
							  i==0?z_axis_color:basic_grid_color, kDraw, 1);
    }
    draw_scene->lines.Add(vec3(0.0f, -10.0f, 0.0f), vec3(0.0f, 10.0f,  0.0f),
						  y_axis_color, kDraw, 1);
}

void DrawDrawable(const mat4 &proj_view_mat, Drawable* drawable) {
	glUseProgram(drawable->shader_id);

	GLuint modelview_matrix_uniform = glGetUniformLocation(drawable->shader_id, "mv_mat");
	GLuint normal_matrix_uniform = glGetUniformLocation(drawable->shader_id, "norm_mat");

	mat4 model_mat = drawable->transform;
	mat4 mat = proj_view_mat * model_mat;
	mat3 normal_mat = mat3(model_mat);
	glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&mat);
	glUniformMatrix3fv(normal_matrix_uniform, 1, false, (GLfloat*)&normal_mat);

	GLuint texture_uniform = glGetUniformLocation(drawable->shader_id, "texture_id");
	glUniform1i(texture_uniform, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, drawable->texture_id);

	glBindBuffer(GL_ARRAY_BUFFER, drawable->vert_vbo);
	switch(drawable->vbo_layout){
	case kSimple_4V:
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable->index_vbo);
		glDrawElements(GL_TRIANGLES, drawable->num_indices, GL_UNSIGNED_INT, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDisableVertexAttribArray(0);
		break;
	case kInterleave_3V2T3N:
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), (void*)(3*sizeof(GLfloat)));
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), (void*)(5*sizeof(GLfloat)));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable->index_vbo);
		glDrawElements(GL_TRIANGLES, drawable->num_indices, GL_UNSIGNED_INT, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDisableVertexAttribArray(2);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(0);
		break;
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
	CHECK_GL_ERROR();
}

void Draw(GraphicsContext* context, GameState* game_state, DrawScene* draw_scene, int ticks) {
	glViewport(0, 0, context->screen_dims[0], context->screen_dims[1]);
	glClearColor(0.5,0.5,0.5,1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

    float aspect_ratio = context->screen_dims[0] / (float)context->screen_dims[1];
    mat4 proj_mat = glm::perspective(45.0f, aspect_ratio, 0.1f, 100.0f);
	mat4 view_mat = inverse(game_state->camera.GetCombination());
	mat4 proj_view_mat = proj_mat * view_mat;

	for(int i=0; i<draw_scene->num_drawables; ++i){
		Drawable* drawable = &draw_scene->drawables[i];
		DrawDrawable(proj_view_mat, drawable);
	}

    DrawCoordinateGrid(draw_scene);
	draw_scene->lines.Draw(proj_view_mat);
}

void LoadFBX(FBXParseScene* parse_scene, const char* path, FileLoadData* file_load_data, const char* specific_name) {
	int path_len = strlen(path);
	if(path_len > FileRequest::kMaxFileRequestPathLen){
		FormattedError("File path too long", "Path is %d characters, %d allowed", path_len, FileRequest::kMaxFileRequestPathLen);
		exit(1);
	}
	int texture = -1;
	if (SDL_LockMutex(file_load_data->mutex) == 0) {
		FileRequest* request = file_load_data->queue.AddNewRequest();
		for(int i=0; i<path_len + 1; ++i){
			request->path[i] = path[i];
		}
		request->condition = SDL_CreateCond();
		SDL_CondWait(request->condition, file_load_data->mutex);
        if(file_load_data->err){
            FormattedError(file_load_data->err_title, file_load_data->err_msg);
            exit(1);
        }
		ParseFBXFromRAM(parse_scene, file_load_data->memory, file_load_data->memory_len, specific_name);
		SDL_UnlockMutex(file_load_data->mutex);
	} else {
		FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
		exit(1);
	}
}

int CreateProgramFromFile(FileLoadData* file_load_data, const char* path){
	int path_len = strlen(path)+5;
	if(path_len > FileRequest::kMaxFileRequestPathLen){
		FormattedError("File path too long", "Path is %d characters, %d allowed", path_len, FileRequest::kMaxFileRequestPathLen);
		exit(1);
	}

	char shader_path[FileRequest::kMaxFileRequestPathLen];
	FormatString(shader_path, FileRequest::kMaxFileRequestPathLen, "%s.vert", path);
	static const int kNumShaders = 2;
	int shaders[kNumShaders];

	if (SDL_LockMutex(file_load_data->mutex) == 0) {
		FileRequest* request = file_load_data->queue.AddNewRequest();
		for(int i=0; i<path_len + 1; ++i){
			request->path[i] = shader_path[i];
		}
		request->condition = SDL_CreateCond();
        SDL_CondWait(request->condition, file_load_data->mutex);
        if(file_load_data->err){
            FormattedError(file_load_data->err_title, file_load_data->err_msg);
            exit(1);
        }
		char* mem_text = (char*)file_load_data->memory;
		mem_text[file_load_data->memory_len] = '\0';
		shaders[0] = CreateShader(GL_VERTEX_SHADER, mem_text);
		SDL_UnlockMutex(file_load_data->mutex);
	} else {
		FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
		exit(1);
	}

	FormatString(shader_path, FileRequest::kMaxFileRequestPathLen, "%s.frag", path);

	if (SDL_LockMutex(file_load_data->mutex) == 0) {
		FileRequest* request = file_load_data->queue.AddNewRequest();
		for(int i=0; i<path_len + 1; ++i){
			request->path[i] = shader_path[i];
		}
		request->condition = SDL_CreateCond();
        SDL_CondWait(request->condition, file_load_data->mutex);
        if(file_load_data->err){
            FormattedError(file_load_data->err_title, file_load_data->err_msg);
            exit(1);
        }
		char* mem_text = (char*)file_load_data->memory;
		mem_text[file_load_data->memory_len] = '\0';
		shaders[1] = CreateShader(GL_FRAGMENT_SHADER, mem_text);
		SDL_UnlockMutex(file_load_data->mutex);
	} else {
		FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
		exit(1);
	}

	int shader_program = CreateProgram(shaders, kNumShaders);
	for(int i=0; i<kNumShaders; ++i){
		glDeleteShader(shaders[i]);
	}
	return shader_program;
}

void VBOFromMesh(Mesh* mesh, int* vert_vbo, int* index_vbo) {
	int interleaved_size = sizeof(float)*mesh->num_tris*3*8;
	float* interleaved = (float*)malloc(interleaved_size);
	int consecutive_size = sizeof(unsigned)*mesh->num_tris*3;
	unsigned* consecutive = (unsigned*)malloc(consecutive_size);
	for(int i=0, index=0, len=mesh->num_tris*3; i<len; ++i){
		for(int j=0; j<3; ++j){
			interleaved[index++] = mesh->vert_coords[mesh->tri_indices[i]*3+j];
		}
		for(int j=0; j<2; ++j){
			interleaved[index++] = mesh->tri_uvs[i*2+j];
		}
		for(int j=0; j<3; ++j){
			interleaved[index++] = mesh->tri_normals[i*3+j];
		}
		consecutive[i] = i;
	}	
	*vert_vbo = CreateVBO(kArrayVBO, kStaticVBO, interleaved, interleaved_size);
	*index_vbo = CreateVBO(kElementVBO, kStaticVBO, consecutive, consecutive_size);
}

void GetBoundingBox(const Mesh* mesh, vec3* bounding_box) {
    SDL_assert(mesh);
    SDL_assert(mesh->num_verts > 0);
    bounding_box[0] = vec3(FLT_MAX);
    bounding_box[1] = vec3(-FLT_MAX);
    for(int i=0, vert_index=0; i<mesh->num_verts; ++i){
        for(int k=0; k<3; ++k){
            bounding_box[0][k] = min(mesh->vert_coords[vert_index], bounding_box[0][k]);
            bounding_box[1][k] = max(mesh->vert_coords[vert_index], bounding_box[1][k]);
            ++vert_index;
        }
    }
}

static const int s_pos_x = 1 << 0, s_pos_y = 1 << 1, s_pos_z = 1 << 2;
static const int e_pos_x = 1 << 3, e_pos_y = 1 << 4, e_pos_z = 1 << 5;

static void AddBBLine(DebugDrawLines* lines, const mat4& mat, vec3 bb[], int flags) {
    vec3 points[2];
    points[0][0] = (flags & s_pos_x)?bb[1][0]:bb[0][0];
    points[0][1] = (flags & s_pos_y)?bb[1][1]:bb[0][1];
    points[0][2] = (flags & s_pos_z)?bb[1][2]:bb[0][2];
    points[1][0] = (flags & e_pos_x)?bb[1][0]:bb[0][0];
    points[1][1] = (flags & e_pos_y)?bb[1][1]:bb[0][1];
    points[1][2] = (flags & e_pos_z)?bb[1][2]:bb[0][2];
    lines->Add(vec3(mat*vec4(points[0],1.0f)), 
		       vec3(mat*vec4(points[1],1.0f)), 
		       vec4(1.0f), kPersistent, 1);
}

static void DrawBoundingBox(DebugDrawLines* lines, const mat4& mat, vec3 bb[]) {
    static const int s_neg_x = 0, s_neg_y = 0, s_neg_z = 0;
    static const int e_neg_x = 0, e_neg_y = 0, e_neg_z = 0;
    // Neg Y square
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_neg_z | e_pos_x | e_neg_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_neg_z | e_pos_x | e_neg_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_pos_z | e_neg_x | e_neg_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_pos_z | e_neg_x | e_neg_y | e_neg_z);
    // Pos Y square
    AddBBLine(lines, mat, bb, s_neg_x | s_pos_y | s_neg_z | e_pos_x | e_pos_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_pos_y | s_neg_z | e_pos_x | e_pos_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_pos_y | s_pos_z | e_neg_x | e_pos_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_neg_x | s_pos_y | s_pos_z | e_neg_x | e_pos_y | e_neg_z);
    // Neg Y to Pos Y
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_neg_z | e_neg_x | e_pos_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_neg_z | e_pos_x | e_pos_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_pos_z | e_pos_x | e_pos_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_pos_z | e_neg_x | e_pos_y | e_pos_z);
}

int main(int argc, char* argv[]) {
	{
		FBXParseScene parse_scene;
		void* mem = malloc(1024*1024*64);
		int len;
		char err_title[FileLoadData::kMaxErrMsgLen];
		char err_msg[FileLoadData::kMaxErrMsgLen];
		if(!FileLoadData::LoadFile(ASSET_PATH "main_character_rig.fbx", 
			mem,  &len, err_title, err_msg))
		{
			FormattedError(err_title, err_msg);
			exit(1);
		}
		ParseFBXFromRAM(&parse_scene, mem, len, "RiggedMesh");
		exit(1);
	}

	// Allocate game memory block
	static const int kGameMemSize = 1024*1024*64;
	StackMemoryBlock stack_memory_block(malloc(kGameMemSize), kGameMemSize);
	if(!stack_memory_block.mem){
		FormattedError("Malloc failed", "Could not allocate enough memory");
		exit(1);
	}

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
		FormattedError("SDL_Init failed", "Could not initialize SDL: %s", SDL_GetError());
		return 1;
    }
    
    { // Check for assets folder
        struct stat st;
        if(stat(ASSET_PATH "under_glass_game_assets_folder.txt", &st) == -1){
            char *basePath = SDL_GetBasePath();
            ChangeWorkingDirectory(basePath);
            SDL_free(basePath);
            if(stat(ASSET_PATH "under_glass_game_assets_folder.txt", &st) == -1){
                FormattedError("Assets?", "Could not find assets directory, possibly running from inside archive");
            }
        }
    }

	// Set up file loader 
	FileLoadData file_load_data;
	file_load_data.wants_to_quit = false;
	file_load_data.memory_len = 0;
	file_load_data.memory = stack_memory_block.Alloc(FileLoadData::kMaxFileLoadSize);
	if(!file_load_data.memory){
		FormattedError("Alloc failed", "Could not allocate memory for FileLoadData");
		return 1;
	}
	file_load_data.mutex = SDL_CreateMutex();
	if (!file_load_data.mutex) {
		FormattedError("SDL_CreateMutex failed", "Could not create file load mutex: %s", SDL_GetError());
		return 1;
	}
	SDL_Thread* file_thread = SDL_CreateThread(FileLoadAsync, "FileLoaderThread", &file_load_data);
	if(!file_thread){
		FormattedError("SDL_CreateThread failed", "Could not create file loader thread: %s", SDL_GetError());
		return 1;
	}

	// Init graphics
	GraphicsContext graphics_context;
	InitGraphicsContext(&graphics_context);

	int texture = LoadImage(ASSET_PATH "cobbles_c.tga", &file_load_data);
	FBXParseScene parse_scene;

	int street_lamp_vert_vbo, street_lamp_index_vbo;
	LoadFBX(&parse_scene, ASSET_PATH "street_lamp.fbx", &file_load_data, NULL);
	int num_street_lamp_indices = parse_scene.meshes[0].num_tris*3;
    vec3 lamp_bb[2];
    GetBoundingBox(&parse_scene.meshes[0], lamp_bb);
	VBOFromMesh(&parse_scene.meshes[0], &street_lamp_vert_vbo, &street_lamp_index_vbo);
	parse_scene.Dispose();

	int cobble_floor_vert_vbo, cobble_floor_index_vbo;
	LoadFBX(&parse_scene, ASSET_PATH "cobble_floor.fbx", &file_load_data, NULL);	
    int num_cobble_floor_indices = parse_scene.meshes[0].num_tris*3;
    vec3 floor_bb[2];
    GetBoundingBox(&parse_scene.meshes[0], floor_bb);
	VBOFromMesh(&parse_scene.meshes[0], &cobble_floor_vert_vbo, &cobble_floor_index_vbo);
	parse_scene.Dispose();

	int cobbles_texture = LoadImage(ASSET_PATH "cobbles_c.tga", &file_load_data);
	int lamp_texture = LoadImage(ASSET_PATH "lamp_c.tga", &file_load_data);
	
	int shader_3d_model = CreateProgramFromFile(&file_load_data, ASSET_PATH "shaders/3D_model");

	DrawScene draw_scene;
    draw_scene.lines.num_lines = 0;
    draw_scene.num_drawables = 0;
	draw_scene.drawables[0].vert_vbo = street_lamp_vert_vbo;
	draw_scene.drawables[0].index_vbo = street_lamp_index_vbo;
	draw_scene.drawables[0].num_indices = num_street_lamp_indices;
	draw_scene.drawables[0].vbo_layout = kInterleave_3V2T3N;
    SeparableTransform sep_transform;
    sep_transform.rotation = angleAxis(-glm::half_pi<float>(), vec3(1.0f,0.0f,0.0f));
    sep_transform.translation = vec3(0.0f, -lamp_bb[0][2], 2.0f);
    sep_transform.scale = vec3(1.0f);
	draw_scene.drawables[0].transform = sep_transform.GetCombination();
	draw_scene.drawables[0].texture_id = lamp_texture;
    draw_scene.drawables[0].shader_id = shader_3d_model;
    ++draw_scene.num_drawables;

	draw_scene.lines.shader = CreateProgramFromFile(&file_load_data, ASSET_PATH "shaders/debug_draw");
	draw_scene.lines.vbo = CreateVBO(kArrayVBO, kStreamVBO, NULL, 0);

    DrawBoundingBox(&draw_scene.lines, sep_transform.GetCombination(), lamp_bb);

    {
        FBXParseScene parse_scene;
        void* mem = malloc(1024*1024*64);
        int len;
        char err_title[FileLoadData::kMaxErrMsgLen];
        char err_msg[FileLoadData::kMaxErrMsgLen];
        if(!FileLoadData::LoadFile(ASSET_PATH "main_character_rig.fbx", 
            mem,  &len, err_title, err_msg))
        {
            FormattedError(err_title, err_msg);
            exit(1);
        }
        //ParseFBXFromRAM(&parse_scene, mem, len, "RiggedMesh");
        ParseFBXFromRAM(&parse_scene, mem, len, "rig");
        Skeleton& skeleton = parse_scene.skeletons[0];
        for(int i=0; i<skeleton.num_bones; ++i){
            Bone& bone = skeleton.bones[i];
            if(bone.parent != -1){
                mat4 temp;
                for(int i=0; i<16; ++i){
                    temp[i/4][i%4] = bone.transform[i];
                }
                Bone& parent_bone = skeleton.bones[bone.parent];
                mat4 temp_parent;
                for(int i=0; i<16; ++i){
                    temp_parent[i/4][i%4] = parent_bone.transform[i];
                }
                vec3 start(temp * vec4(0,0,0,1));
                vec3 end(temp_parent * vec4(0,0,0,1));
                draw_scene.lines.Add(start, end, vec4(1.0f), kPersistent, 1);
            }
        }
        parse_scene.Dispose();
    }

    for(int i=-10; i<10; ++i){
        sep_transform.translation = vec3(0.0f, floor_bb[1][2], 0.0f);
        sep_transform.translation += vec3(0.0f,0.0f,-1.0f+(float)i*2.0f);
        draw_scene.drawables[draw_scene.num_drawables].vert_vbo = cobble_floor_vert_vbo;
        draw_scene.drawables[draw_scene.num_drawables].index_vbo = cobble_floor_index_vbo;
        draw_scene.drawables[draw_scene.num_drawables].num_indices = num_cobble_floor_indices;
        draw_scene.drawables[draw_scene.num_drawables].vbo_layout = kInterleave_3V2T3N;
        draw_scene.drawables[draw_scene.num_drawables].transform = sep_transform.GetCombination();
        draw_scene.drawables[draw_scene.num_drawables].texture_id = cobbles_texture;
        draw_scene.drawables[draw_scene.num_drawables].shader_id = shader_3d_model;
        ++draw_scene.num_drawables;
    }

	AudioContext audio_context;
	InitAudio(&audio_context, &stack_memory_block);

	GameState game_state;
	game_state.camera.translation = vec3(0.0f,0.0f,20.0f);
	game_state.camera.rotation = angleAxis(0.0f,vec3(1.0f,0.0f,0.0f));
	game_state.camera.scale = vec3(1.0f);

	int last_ticks = SDL_GetTicks();
	bool game_running = true;
	while(game_running){
		SDL_Event event;
        vec2 mouse_rel;
		while(SDL_PollEvent(&event)){
			switch(event.type){
			case SDL_QUIT:
				game_running = false;
				break;
            case SDL_MOUSEMOTION:
                mouse_rel[0] += event.motion.xrel;
                mouse_rel[1] += event.motion.yrel;
                break;
			}
		}
		Draw(&graphics_context, &game_state, &draw_scene, SDL_GetTicks());
		int ticks = SDL_GetTicks();
		Update(&game_state, mouse_rel, (ticks - last_ticks) / 1000.0f);
		last_ticks = ticks;
		UpdateAudio(&audio_context);
		SDL_GL_SwapWindow(graphics_context.window);
	}

	// Wait for the audio to fade out
	// TODO: handle this better -- e.g. force audio fade immediately
	SDL_Delay(200);

	SDL_CloseAudioDevice(audio_context.device_id);
	SDL_GL_DeleteContext(graphics_context.gl_context);  
	SDL_DestroyWindow(graphics_context.window);
	// Cleanly shut down file load thread 
	if (SDL_LockMutex(file_load_data.mutex) == 0) {
		file_load_data.wants_to_quit = true;
		SDL_UnlockMutex(file_load_data.mutex);
		SDL_WaitThread(file_thread, NULL);
	} else {
		FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
		exit(1);
	}
	SDL_Quit();
	free(stack_memory_block.mem);
	return 0;
}