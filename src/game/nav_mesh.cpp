#include "game/nav_mesh.h"
#include "internal/memory.h"
#include "internal/common.h"
#include "internal/geometry.h"
#include "platform_sdl/error.h"
#include "platform_sdl/graphics.h"
#include "glm/glm.hpp"
#include "glm/gtx/norm.hpp"
#include <GL/glew.h>

using namespace glm;


// Compute barycentric coordinates (u, v, w) for
// point p with respect to triangle (a, b, c)
// Transcribed from Christer Ericson's Real-Time Collision Detection
// http://gamedev.stackexchange.com/a/23745
void Barycentric(vec3 p, vec3 a, vec3 b, vec3 c, vec3* bary)
{
    vec3 v0 = b - a, v1 = c - a, v2 = p - a;
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    (*bary)[1] = (d11 * d20 - d01 * d21) / denom;
    (*bary)[2] = (d00 * d21 - d01 * d20) / denom;
    (*bary)[0] = 1.0f - (*bary)[1] - (*bary)[2];
}

void NavMesh::Draw(GraphicsContext* graphics_context, const mat4& proj_view_mat) {
    Shader* the_shader = &graphics_context->shaders[shader];
    glBindBuffer(GL_ARRAY_BUFFER, vert_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_vbo);
    glUseProgram(the_shader->gl_id);
    glUniformMatrix4fv(the_shader->uniforms[Shader::kModelviewMat4], 1, false, (GLfloat*)&proj_view_mat);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);
    glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

struct Vec3EdgeHash {
    int hash;
    int tri_index;
    vec3 verts[2];
};

int Vec3EdgeHashSort(const void* a_ptr, const void* b_ptr){
    const Vec3EdgeHash* a = (const Vec3EdgeHash*)a_ptr;
    const Vec3EdgeHash* b = (const Vec3EdgeHash*)b_ptr;
    if(a->hash < b->hash) {
        return -1;
    } else if(a->hash == b->hash){
        return 0;
    } else {
        return 1;
    }
}

void NavMesh::CalcNeighbors(StackAllocator* stack_allocator) {
    //TODO: just compare each vec3 value in turn instead of using hashes, no need to risk collisions if don't have to
    static const int kMaxEdges = 10000;
    int num_unique_edges = 0;
    Vec3EdgeHash* unique_verts = (Vec3EdgeHash*)stack_allocator->Alloc(
        sizeof(Vec3EdgeHash)*kMaxEdges);
    if(!unique_verts){
        FormattedError("Error", "Could not allocate memory for Navmesh::CalcNeighbors unique_verts");
    }
    for(int i=0; i<num_indices; i+=3){
        for(int j=0; j<3; ++j){
            tri_neighbors[i+j] = -1;
            vec3 edge_verts[2] = {verts[indices[i+j]], verts[indices[i+(j+1)%3]]};
            for(int k=0; k<2; ++k){
                for(int l=0; l<3; ++l){
                    edge_verts[k][l] = glm::round<float>(edge_verts[k][l]*100.0f)/100.0f;
                }
            }
            // Make sure that identical edges have verts in the same order
            int vec3_hash[2];
            for(int k=0; k<2; ++k){
                vec3_hash[k] = djb2_hash_len((unsigned char*)&edge_verts[k], sizeof(vec3));
            }
            if(vec3_hash[0] > vec3_hash[1]){
                vec3 temp = edge_verts[0];
                edge_verts[0] = edge_verts[1];
                edge_verts[1] = temp;
            }
            unique_verts[num_unique_edges].hash = djb2_hash_len((unsigned char*)edge_verts, sizeof(vec3)*2);
            for(int k=0; k<2; ++k){
                unique_verts[num_unique_edges].verts[k] = edge_verts[k];
            }
            unique_verts[num_unique_edges].hash = djb2_hash_len((unsigned char*)edge_verts, sizeof(vec3)*2);
            unique_verts[num_unique_edges].tri_index = i+j;
            ++num_unique_edges;
            if(num_unique_edges >= kMaxEdges) {
                FormattedError("Error", "Too many edges for CalcNeighbors");
                exit(1);
            }
        }
    }
    qsort(unique_verts, num_unique_edges, sizeof(Vec3EdgeHash), Vec3EdgeHashSort);
    for(int i=1; i<num_unique_edges; ++i){
        if(unique_verts[i].hash == unique_verts[i-1].hash){
            if(unique_verts[i].verts[0] != unique_verts[i].verts[0] ||
                unique_verts[i].verts[1] != unique_verts[i].verts[1])
            {
                FormattedError("Error", "Hash collision in CalcNeighbors");
                exit(1);
            }
            tri_neighbors[unique_verts[i].tri_index] = unique_verts[i-1].tri_index;
            tri_neighbors[unique_verts[i-1].tri_index] = unique_verts[i].tri_index;
        }
    }
    stack_allocator->Free(unique_verts);
}

float Distance2FromPointToTriangle(const vec3& point, const vec3 tri[]) {
    vec3 norm = normalize(cross(tri[2]-tri[0], tri[1]-tri[0]));
    float tri_d = dot(norm, tri[0]);
    float point_d = dot(norm, point);
    vec3 proj_point = point - norm * (point_d - tri_d);
    vec3 bary;
    Barycentric(proj_point, tri[0], tri[1], tri[2], &bary);
    bool in_tri = true;
    for(int i=0; i<3; ++i){
        if(bary[i] < 0.0f){
            in_tri = false;
        }
    }
    if(in_tri){
        return glm::abs(point_d - tri_d);
    } else {
        float closest_dist = FLT_MAX;
        for(int i=0; i<3; ++i){
            vec3 closest = ClosestPointOnSegment(point, tri[i], tri[(i+1)%3]);
            float dist = distance2(point, closest);
            if(dist < closest_dist){
                closest_dist = dist;
            }
        }
        return closest_dist;
    }
}

int NavMesh::ClosestTriToPoint(const vec3& pos) {
    //TODO: use a spatial structure to make this O(logN) instead of O(N)
    float closest_dist = FLT_MAX;
    int closest = -1;
    for(int i=0; i<num_indices; i+=3){
        vec3 tri_verts[3];
        for(int j=0; j<3; ++j){
            tri_verts[j] = verts[indices[i+j]];
        }
        float dist = Distance2FromPointToTriangle(pos, tri_verts);
        if(dist < closest_dist){
            closest_dist = dist;
            closest = i/3;
        }
    }
    return closest;
}
