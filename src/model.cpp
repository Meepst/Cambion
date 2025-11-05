#include "model.h"
#include "alloc.h"
#include "primitives.h"


bool loadModel(Allocator& allocator,  Model& model, const char* path){
    fastObjMesh* obj = fast_obj_read(path);
    if(!obj){
        printf("failed to load\n");
        return false;
    }

    model.materials.reserve(obj->material_count);

    for(size_t i=0;i<obj->material_count;i++){
        const fastObjMaterial& mat = obj->materials[i];
        Material tmpMaterial{};
        tmpMaterial.ambient = {mat.Ka[0],mat.Ka[1],mat.Ka[2]};
        tmpMaterial.diffuse = {mat.Kd[0],mat.Kd[1],mat.Kd[2]};
        tmpMaterial.emmission = {mat.Ke[0],mat.Ke[1],mat.Ke[2]};
        tmpMaterial.specular = {mat.Ks[0],mat.Ks[1],mat.Ks[2]};
        tmpMaterial.opacity = mat.d;
        tmpMaterial.indexOfRefraction = mat.Ni;
        tmpMaterial.shininess = mat.Ns;

        tmpMaterial.diffuseMap.path = mat.map_Kd;
        tmpMaterial.normalMap.path = mat.map_bump;
        tmpMaterial.opacityMap.path = mat.map_d;

        model.materials.push_back(tmpMaterial);
    }

    for(size_t i=0;i<obj->group_count;i++){
        const fastObjGroup& group = obj->groups[i];
        if(group.face_count == 0) continue;

        Mesh mesh{};
        mesh.materialID = obj->face_materials[group.face_offset];
        std::unordered_map<Vertex, uint32_t, VertexHash> uniqueVerts;

        uint32_t offsetIndex = 0;
        for(size_t j=0;j<group.face_count;j++){
            size_t faceVerts = obj->face_vertices[group.face_offset+j];
            for(size_t k=0;k<faceVerts;k++){
                fastObjIndex idx = obj->indices[offsetIndex+k];
                Vertex vert{};
                vert.pos = {
                    obj->positions[3*idx.p+0],
                    obj->positions[3*idx.p+1],
                    obj->positions[3*idx.p+2],
                };
                if(idx.t >= 0)
                    vert.texCoord = {
                        obj->texcoords[2*idx.t+0],
                        obj->texcoords[2*idx.t+1]
                    };
                if(idx.n >=0)
                    vert.normal = {
                        obj->normals[3*idx.n+0],
                        obj->normals[3*idx.n+1],
                        obj->normals[3*idx.n+2],
                    };

                if(uniqueVerts.count(vert) == 0){
                    uniqueVerts[vert] = static_cast<uint32_t>(mesh.vertices.size());

                    mesh.vertices.push_back(vert);
                }
                mesh.indices.push_back(uniqueVerts[vert]);
            }
            offsetIndex += faceVerts;
        }
        model.meshes.push_back(mesh);
    }

    fast_obj_destroy(obj);
    return true;
}
