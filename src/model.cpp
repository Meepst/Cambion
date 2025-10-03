#include "model.h"


bool loadModel(Allocator& allocator, DynArray<Vertex>& vertices, DynArray<uint32_t>& indices, const char* path){
    fastObjMesh* obj = fast_obj_read(path);
    if(!obj){
        printf("failed to load\n");
        return false;
    }

    int indices_reserved = 0;
    for(int i = 0; i < obj->face_count; ++i) {
        int face_vertices = obj->face_vertices[i];
        indices_reserved += face_vertices;
    }
    vertices.reserve(allocator, 0, indices_reserved); // May need resizing
    indices.reserve(allocator, 0, indices_reserved);

    size_t vertexOffset = 0;
    size_t indexOffset = 0;

    std::unordered_map<Vertex, uint32_t, VertexHash> uniqueVertices;

    // Vertex : Vec2 pos & Vec3 color
    for(unsigned int i=0;i<obj->face_count; i++){
        for(unsigned int j=0;j<obj->face_vertices[i]-2;j+=3){
            fastObjIndex indexX = obj->indices[indexOffset];
            fastObjIndex indexY = obj->indices[indexOffset+j+1];
            fastObjIndex indexZ = obj->indices[indexOffset+j+2];

            fastObjIndex triIdx[3] = {indexX,indexY,indexZ};

            for(unsigned k=0;k<3;k++){
                float px = obj->positions[3*triIdx[k].p+0];
                float py = obj->positions[3*triIdx[k].p+1];
                float pz = obj->positions[3*triIdx[k].p+2];

                glm::vec3 pos = {px,py,pz};
                glm::vec3 color = {1.0f,1.0f,0.0f};
                glm::vec2 texCoord = {0.0f,0.0f};

                if(triIdx[k].t != 0){
                    float u = obj->texcoords[2*triIdx[k].t+0];
                    float v = obj->texcoords[2*triIdx[k].t+1];
                    texCoord = {u,v};
                }

                Vertex vert = {pos,color,texCoord};

                if(!uniqueVertices.contains(vert)){
                    uint32_t idx = static_cast<uint32_t>(vertices.size());
                    uniqueVertices[vert] = idx;
                    vertices.push(allocator, vert);
                }

                indices.push(allocator, uniqueVertices[vert]);
            }
        }
        indexOffset += obj->face_vertices[i];
    }

    fast_obj_destroy(obj);
    return true;
}
