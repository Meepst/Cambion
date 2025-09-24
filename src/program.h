#pragma once
const int DESCRIPTOR_LIMIT = 65536;
const int DYNAMIC_COLOR_ATTACHMENT_COUNT = 8;

#include "common.h"
#include "swapchain.h"
#include <spirv-headers/spirv.h>


struct Vertex{
    glm::vec3 pos;
    glm::vec3 color;
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
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct Shader {
    std::string name;
    std::vector<char> spirvCode;
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorType resourceTypes[32] = {};
    uint32_t resourceMask;

    uint32_t localSizeX;
    uint32_t localSizeY;
    uint32_t localSizeZ;

    bool needPushConstants;
    bool needDescriptorArray;
};

struct ShaderSet {
    std::vector<Shader> shaders;

    const Shader& operator[](const char* name) const {
        for (const Shader& shader : shaders) {
            if (shader.name == name) return shader;
        }

        std::cout << "\tFailed to find shader\n";
        abort();
    }
};

struct DescriptorInfo {
    union {
        VkDescriptorImageInfo image;
        VkDescriptorBufferInfo buffer;
        VkAccelerationStructureKHR accelerationStructure;
    };

    DescriptorInfo() {};
    DescriptorInfo(VkAccelerationStructureKHR structure) {
        accelerationStructure = structure;
    }
    DescriptorInfo(VkImageView imageView, VkImageLayout imageLayout)
    {
        image.sampler = VK_NULL_HANDLE;
        image.imageView = imageView;
        image.imageLayout = imageLayout;
    }
    DescriptorInfo(VkSampler sampler)
    {
        image.sampler = sampler;
        image.imageView = VK_NULL_HANDLE;
        image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
    {
        image.sampler = sampler;
        image.imageView = imageView;
        image.imageLayout = imageLayout;
    }
    DescriptorInfo(VkBuffer buffer_, VkDeviceSize offset, VkDeviceSize range)
    {
        buffer.buffer = buffer_;
        buffer.offset = offset;
        buffer.range = range;
    }
    DescriptorInfo(VkBuffer buffer_)
    {
        buffer.buffer = buffer_;
        buffer.offset = 0;
        buffer.range = VK_WHOLE_SIZE;
    }
};

struct Program {
    VkPipelineBindPoint bindPoint;
    VkPipelineLayout layout;
    VkDescriptorSetLayout setLayout;
    VkDescriptorUpdateTemplate updateTemplate;

    VkShaderStageFlags pushConstantStages;
    uint32_t pushConstantSize;
    uint32_t pushDescriptorCount;

    uint32_t localSizeX;
    uint32_t localSizeY;
    uint32_t localSizeZ;

    const Shader* shaders[8];
    size_t shaderCount;
};

using Shaders = std::initializer_list<const Shader*>;
using Constants = std::initializer_list<int>;

// spirv shader header info as defined in spir-v specification
// https://www.khronos.org/registry/spir-v/specs/1.0/SPIRV.pdf
struct Id {
    uint32_t opcode;
    uint32_t typeId;
    uint32_t storageClass;
    uint32_t binding;
    uint32_t set;
    uint32_t constant;
};

static VkShaderStageFlagBits getShaderStage(SpvExecutionModel _executionModel);

static VkDescriptorType getDescriptorType(SpvOp _op);

static void readShader(Shader& _shader, const uint32_t* _code,
    uint32_t _codeSize);

static uint32_t gatherResources(Shaders _shaders,
    VkDescriptorType(&_resourceTypes)[32]);

static VkDescriptorSetLayout createSetLayout(VkDevice _device, Shaders _shaders);

static VkPipelineLayout createPipelineLayout(VkDevice _device, VkDescriptorSetLayout _setLayout, VkDescriptorSetLayout _arrayLayout, VkShaderStageFlags _pushConstantStages, size_t _pushConstantSize);
static VkDescriptorUpdateTemplate createUpdateTemplate(VkDevice _device, VkPipelineBindPoint _bindPoint, VkPipelineLayout _layout, Shaders _shaders, uint32_t* _pushDescriptorCount);

bool loadShader(Shader& _shader, const std::string& filename);
bool loadShaders(ShaderSet& _shaders, const char* base, const char* path);

static VkSpecializationInfo fillSpecializationInfo(std::vector<VkSpecializationMapEntry>& entries, const Constants& constants);

Program createProgram(VkDevice _device, VkPipelineBindPoint _bindPoint, Shaders _shaders, size_t _pushConstantSize, VkDescriptorSetLayout _arrayLayout);

void destroyProgram(VkDevice _device, const Program& _program);

std::pair<VkDescriptorPool, VkDescriptorSet> createDescriptorArray(VkDevice _device, VkDescriptorSetLayout _layout, uint32_t _descriptorCount);

VkDescriptorSetLayout createDescriptorArrayLayout(VkDevice _device);

VkPipeline createGraphicsPipeline(VkDevice _device, VkPipelineCache _pipelineCache, const VkPipelineRenderingCreateInfo& _renderingInfo, const Program& _program, Constants constants);