#pragma once

#include "common.h"
#include "resources.h"


struct Vertex{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription(){
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
    bool operator==(const Vertex& other) const {
        return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
    }
};

struct VertexHash{
    size_t operator()(const Vertex& ver) const noexcept{
        auto h1 = std::hash<float>{}(ver.pos.x);
        auto h2 = std::hash<float>{}(ver.pos.y);
        auto h3 = std::hash<float>{}(ver.pos.z);

        auto h4 = std::hash<float>{}(ver.normal.x);
        auto h5 = std::hash<float>{}(ver.normal.y);
        auto h6 = std::hash<float>{}(ver.normal.z);

        auto h7 = std::hash<float>{}(ver.texCoord.x);
        auto h8 = std::hash<float>{}(ver.texCoord.y);

        size_t seed = 0;
        auto hashCombine = [&seed](size_t h) {
            seed ^= h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };

        hashCombine(h1);
        hashCombine(h2);
        hashCombine(h3);
        hashCombine(h4);
        hashCombine(h5);
        hashCombine(h6);
        hashCombine(h7);
        hashCombine(h8);

        return seed;
    }
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t materialID;
};

struct TextureImage{
    std::string path;
    Image image;
    VkSampler sampler;
};

struct Material{
    glm::vec3 ambient, diffuse, specular, emmission;
    float shininess, indexOfRefraction, opacity;
    TextureImage diffuseMap, normalMap, opacityMap;
};

struct Model{
  std::vector<Mesh> meshes;
  std::vector<Material> materials;

  Model(std::vector<Mesh> meshes, std::vector<Material> materials) :
    meshes(meshes), materials(materials){}
};
