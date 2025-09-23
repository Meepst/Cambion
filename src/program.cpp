#include "program.h"
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif 

static VkShaderStageFlagBits getShaderStage(SpvExecutionModel _executionModel) {
    switch (_executionModel) {
    case SpvExecutionModelVertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case SpvExecutionModelGeometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case SpvExecutionModelFragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case SpvExecutionModelGLCompute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    case SpvExecutionModelTaskEXT:
        return VK_SHADER_STAGE_TASK_BIT_EXT;
    case SpvExecutionModelMeshEXT:
        return VK_SHADER_STAGE_MESH_BIT_EXT;
    default:
        assert(!"Unknown execution model");
        return VkShaderStageFlagBits(0);
    }
}

static VkDescriptorType getDescriptorType(SpvOp _op) {
    switch (_op) {
    case SpvOpTypeStruct:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case SpvOpTypeImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case SpvOpTypeSampler:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case SpvOpTypeSampledImage:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case SpvOpTypeAccelerationStructureKHR:
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    default:
        assert(!"Unknown resource type");
        return VkDescriptorType(0);
    }
}

static void readShader(Shader& _shader, const uint32_t* _code,
    uint32_t _codeSize) {
    assert(_code[0] == SpvMagicNumber);

    uint32_t idBound = _code[3];
    std::vector<Id> ids(idBound);

    int localSizeIdX = -1;
    int localSizeIdY = -1;
    int localSizeIdZ = -1;

    // at position 5 instructions begin
    const uint32_t* instructK = _code + 5;

    while (instructK != _code + _codeSize) {
        uint16_t opcode = uint16_t(instructK[0]);
        uint16_t wordCount = uint16_t(instructK[0] >> 16);

        switch (opcode) {
        case SpvOpEntryPoint: {
            assert(wordCount >= 2);
            _shader.stage = getShaderStage(SpvExecutionModel(instructK[1]));
        } break;
        case SpvOpExecutionMode: {
            assert(wordCount >= 3);
            uint32_t mode = instructK[2];

            switch (mode) {
            case SpvExecutionModeLocalSize: {
                assert(wordCount == 6);
                _shader.localSizeX = instructK[3];
                _shader.localSizeY = instructK[4];
                _shader.localSizeZ = instructK[5];
            } break;
            }
        } break;
        case SpvOpExecutionModeId: {
            assert(wordCount >= 3);
            uint32_t mode = instructK[2];

            if (mode == SpvExecutionModeLocalSize) {
                assert(wordCount == 6);
                localSizeIdX = instructK[3];
                localSizeIdY = instructK[4];
                localSizeIdZ = instructK[5];
            }
        } break;
        case SpvOpDecorate: {
            assert(wordCount >= 3);
            uint32_t id = instructK[1];
            assert(id < idBound);

            switch (instructK[2]) {
            case SpvDecorationDescriptorSet: {
                assert(wordCount == 4);
                ids[id].set = instructK[3];
            } break;
            case SpvDecorationBinding: {
                assert(wordCount == 4);
                ids[id].binding = instructK[3];
            } break;
            }
        } break;
        case SpvOpTypeStruct:
        case SpvOpTypeImage:
        case SpvOpTypeSampler:
        case SpvOpTypeSampledImage:
        case SpvOpTypeAccelerationStructureKHR: {
            assert(wordCount >= 2);
            uint32_t id = instructK[1];
            assert(id < idBound);
            assert(ids[id].opcode == 0);
            ids[id].opcode = opcode;
        } break;
        case SpvOpTypePointer: {
            assert(wordCount == 4);
            uint32_t id = instructK[1];
            assert(id < idBound);

            assert(ids[id].opcode == 0);
            ids[id].opcode = opcode;
            ids[id].typeId = instructK[3];
            ids[id].storageClass = instructK[2];
        } break;
        case SpvOpConstant: {
            assert(wordCount >= 4);
            uint32_t id = instructK[2];

            assert(ids[id].opcode == 0);
            ids[id].opcode = opcode;
            ids[id].typeId = instructK[1];
            ids[id].constant = instructK[3];
        } break;
        case SpvOpVariable: {
            assert(wordCount >= 4);

            uint32_t id = instructK[2];
            assert(id < idBound);

            assert(ids[id].opcode == 0);
            ids[id].opcode = opcode;
            ids[id].typeId = instructK[1];
            ids[id].storageClass = instructK[3];
        } break;
        }

        assert(instructK + wordCount <= _code + _codeSize);
        instructK += wordCount;
    }

    for (auto& id : ids) {
        if (id.opcode == SpvOpVariable &&
            (id.storageClass == SpvStorageClassUniform ||
                id.storageClass == SpvStorageClassUniformConstant ||
                id.storageClass == SpvStorageClassStorageBuffer) &&
            id.set == 0) {
            assert(id.binding < 32);
            assert(ids[id.typeId].opcode == SpvOpTypePointer);

            uint32_t typeKind = ids[ids[id.typeId].typeId].opcode;
            VkDescriptorType resourceType = getDescriptorType(SpvOp(typeKind));

            assert((_shader.resourceMask & (1 < id.binding)) == 0 ||
                _shader.resourceTypes[id.binding] == resourceType);
            _shader.resourceTypes[id.binding] = resourceType;
            _shader.resourceMask |= 1 << id.binding;
        }

        if (id.opcode == SpvOpVariable &&
            id.storageClass == SpvStorageClassUniformConstant && id.set == 1) {
            _shader.needDescriptorArray = true;
        }

        if (id.opcode == SpvOpVariable &&
            id.storageClass == SpvStorageClassPushConstant) {
            _shader.needPushConstants = true;
        }
    }

    if (_shader.stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        if (localSizeIdX >= 0) {
            assert(ids[localSizeIdX].opcode == SpvOpConstant);
            _shader.localSizeX = ids[localSizeIdX].constant;
        }
        if (localSizeIdY >= 0) {
            assert(ids[localSizeIdY].opcode == SpvOpConstant);
            _shader.localSizeY = ids[localSizeIdY].constant;
        }
        if (localSizeIdZ >= 0) {
            assert(ids[localSizeIdZ].opcode == SpvOpConstant);
            _shader.localSizeZ = ids[localSizeIdZ].constant;
        }
        assert(_shader.localSizeX && _shader.localSizeY && _shader.localSizeZ);
    }
}

static uint32_t gatherResources(Shaders _shaders,
    VkDescriptorType(&_resourceTypes)[32]) {
    uint32_t resourceMask = 0;

    for (const Shader* shader : _shaders) {
        for (uint32_t i = 0; i < 32; ++i) {
            if (shader->resourceMask & (1 << i)) {
                if (resourceMask & (1 << i)) {
                    assert(_resourceTypes[i] == shader->resourceTypes[i]);
                }
                else {
                    _resourceTypes[i] = shader->resourceTypes[i];
                    resourceMask |= 1 << i;
                }
            }
        }
    }
    return resourceMask;
}

static VkDescriptorSetLayout createSetLayout(VkDevice _device,
    Shaders _shaders) {
    std::vector<VkDescriptorSetLayoutBinding> setBindings;

    VkDescriptorType resourceTypes[32] = {};
    uint32_t resourceMask = gatherResources(_shaders, resourceTypes);

    for (uint32_t i = 0; i < 32; ++i) {
        if (resourceMask & (1 << i)) {

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = i;
            binding.descriptorCount = 1;
            binding.descriptorType = resourceTypes[i];
            binding.stageFlags = 0;

            for (const Shader* shader : _shaders) {
                if (shader->resourceMask & (1 << i)) {
                    binding.stageFlags |= shader->stage;
                }
            }
            setBindings.push_back(binding);
        }
    }
    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
    createInfo.bindingCount = uint32_t(setBindings.size());
    createInfo.pBindings = setBindings.data();

    VkDescriptorSetLayout setLayout = 0;
    VK_CHECK(vkCreateDescriptorSetLayout(_device, &createInfo, nullptr, &setLayout));

    return setLayout;
}

static VkPipelineLayout createPipelineLayout(VkDevice _device, VkDescriptorSetLayout _setLayout, VkDescriptorSetLayout _arrayLayout, VkShaderStageFlags _pushConstantStages, size_t _pushConstantSize) {
    VkDescriptorSetLayout layouts[2] = { _setLayout, _arrayLayout };

    VkPipelineLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.setLayoutCount = _arrayLayout ? 2 : 1;
    createInfo.pSetLayouts = layouts;

    VkPushConstantRange pushConstantRange = {};
    if (_pushConstantSize) {
        pushConstantRange.stageFlags = _pushConstantStages;
        pushConstantRange.size = uint32_t(_pushConstantSize);

        createInfo.pushConstantRangeCount = 1;
        createInfo.pPushConstantRanges = &pushConstantRange;
    }

    VkPipelineLayout layout = 0;
    VK_CHECK(vkCreatePipelineLayout(_device, &createInfo, nullptr, &layout));

    return layout;
}

static VkDescriptorUpdateTemplate createUpdateTemplate(VkDevice _device, VkPipelineBindPoint _bindPoint, VkPipelineLayout _layout, Shaders _shaders, uint32_t* _pushDescriptorCount) {
    std::vector<VkDescriptorUpdateTemplateEntry> entries;

    VkDescriptorType resourceTypes[32] = {};
    uint32_t resourceMask = gatherResources(_shaders, resourceTypes);

    for (uint32_t i = 0; i < 32; ++i) {
        if (resourceMask & (1 << i)) {
            VkDescriptorUpdateTemplateEntry entry = {};
            entry.dstBinding = i;
            entry.dstArrayElement = 0;
            entry.descriptorCount = 1;
            entry.descriptorType = resourceTypes[i];
            entry.offset = sizeof(DescriptorInfo) * i;
            entry.stride = sizeof(DescriptorInfo);

            entries.push_back(entry);
        }
    }

    VkDescriptorUpdateTemplate updateTemplate = 0;

    *_pushDescriptorCount = uint32_t(entries.size());
    if(entries.size() > 0){   
        VkDescriptorUpdateTemplateCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
        createInfo.descriptorUpdateEntryCount = uint32_t(entries.size())>0 ? uint32_t(entries.size()) : 1;
        createInfo.pDescriptorUpdateEntries = entries.data();
        createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS;
        createInfo.pipelineBindPoint = _bindPoint;
        createInfo.pipelineLayout = _layout;
        createInfo.set = 0;

        VK_CHECK(vkCreateDescriptorUpdateTemplate(_device, &createInfo, nullptr, &updateTemplate));
    }

    return updateTemplate;
}


bool loadShader(Shader& _shader, const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    assert(file.is_open());

    size_t filesize = (size_t)file.tellg();
    assert(filesize >= 0);
    std::vector<char> buffer(filesize);
    file.seekg(0);
    file.read(buffer.data(), filesize);
    file.close();
    assert(filesize % 4 == 0);
    readShader(_shader, reinterpret_cast<const uint32_t*>(buffer.data()), filesize / 4);

    _shader.spirvCode = buffer;

    return true;
}

static VkSpecializationInfo fillSpecializationInfo(std::vector<VkSpecializationMapEntry>& entries, const Constants& constants) {
    for (size_t i = 0; i < constants.size(); i++) {
        entries.push_back({ uint32_t(i),uint32_t(i * 4),4 });
    }

    VkSpecializationInfo res{};
    res.mapEntryCount = uint32_t(entries.size());
    res.pMapEntries = entries.data();
    res.dataSize = constants.size() * sizeof(int);
    res.pData = constants.begin();

    return res;
}

Program createProgram(VkDevice _device, VkPipelineBindPoint _bindPoint, Shaders _shaders, size_t _pushConstantSize, VkDescriptorSetLayout _arrayLayout) {
    VkShaderStageFlags pushConstantStages = 0;
    for (const Shader* shader : _shaders) {
        if (shader->needPushConstants)
            pushConstantStages |= shader->stage;
    }

    bool needDescriptorArray = false;
    for (const Shader* shader : _shaders) {
        needDescriptorArray |= shader->needDescriptorArray;
    }

    assert(!needDescriptorArray || _arrayLayout);

    Program program{};
    program.bindPoint = _bindPoint;
    program.setLayout = createSetLayout(_device, _shaders);
    assert(program.setLayout);

    program.layout = createPipelineLayout(_device, program.setLayout, _arrayLayout, pushConstantStages, _pushConstantSize);
    assert(program.layout);

    std::cout << program.pushDescriptorCount << std::endl;
    if(program.pushDescriptorCount > 0){
        program.updateTemplate = createUpdateTemplate(_device, program.bindPoint, program.layout, _shaders, &program.pushDescriptorCount);
        assert(program.updateTemplate);
    }

    program.pushConstantStages = pushConstantStages;
    program.pushConstantSize = uint32_t(_pushConstantSize);

    const Shader* shader = _shaders.size() == 1 ? *_shaders.begin() : nullptr;
    if (shader && shader->stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        program.localSizeX = shader->localSizeX;
        program.localSizeY = shader->localSizeY;
        program.localSizeZ = shader->localSizeZ;
    }

    memset(program.shaders, 0, sizeof(program.shaders));
    program.shaderCount = 0;

    for (const Shader* shader : _shaders) {
        program.shaders[program.shaderCount++] = shader;
    }

    return program;
}

void destroyProgram(VkDevice _device, const Program& program) {
    vkDestroyDescriptorUpdateTemplate(_device, program.updateTemplate, nullptr);
    vkDestroyPipelineLayout(_device, program.layout, nullptr);
    vkDestroyDescriptorSetLayout(_device, program.setLayout, nullptr);
}

bool loadShaders(ShaderSet& shaders, const char* base, const char* path)
{
	std::string spath = base;
	std::string::size_type pos = spath.find_last_of("/\\");
	if (pos == std::string::npos)
		spath = "";
	else
		spath = spath.substr(0, pos + 1);
	spath += path;
    
    #ifdef _WIN32
        _finddata_t finddata;
        intptr_t fh = _findfirst((spath + "/*.spv").c_str(), &finddata);
        if (fh == -1)
            return false;
    
        do
        {
            const char* ext = strrchr(finddata.name, '.');
            if (!ext)
                continue;

            std::string fpath = spath + '/' + finddata.name;
            Shader shader = {};
            if (!loadShader(shader, fpath.c_str()))
            {
                fprintf(stderr, "Warning: %s is not a valid SPIRV module\n", finddata.name);
                continue;
            }

            shader.name = std::string(finddata.name, ext - finddata.name);
            shaders.shaders.push_back(shader);
        } while (_findnext(fh, &finddata) == 0);

        _findclose(fh);
    #else
	DIR* dir = opendir(spath.c_str());
	if (!dir)
		return false;

	while (dirent* de = readdir(dir))
	{
		const char* ext = strstr(de->d_name, ".spv");
		if (!ext || strcmp(ext, ".spv") != 0)
			continue;

		std::string fpath = spath + '/' + de->d_name;
		Shader shader = {};
		if (!loadShader(shader, fpath.c_str()))
		{
			fprintf(stderr, "Warning: %s is not a valid SPIRV module\n", de->d_name);
			continue;
		}

		shader.name = std::string(de->d_name, ext - de->d_name);
		shaders.shaders.push_back(shader);
	}

	closedir(dir);
    #endif

	printf("Loaded %d shaders from %s\n", int(shaders.shaders.size()), spath.c_str());
	return true;
}

std::pair<VkDescriptorPool, VkDescriptorSet> createDescriptorArray(VkDevice _device, VkDescriptorSetLayout _layout, uint32_t _descriptorCount) {
    VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, _descriptorCount };
    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount = 1;
    createInfo.maxSets = 1;
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    createInfo.pPoolSizes = &poolSize;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(_device, &createInfo, nullptr, &pool));

    VkDescriptorSetVariableDescriptorCountAllocateInfo setAllocateCountInfo{};
    setAllocateCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    setAllocateCountInfo.descriptorSetCount = 1;
    setAllocateCountInfo.pDescriptorCounts = &_descriptorCount;

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = pool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &_layout;
    allocateInfo.pNext = &setAllocateCountInfo;

    VkDescriptorSet set = 0;
    VK_CHECK(vkAllocateDescriptorSets(_device, &allocateInfo, &set));

    return std::make_pair(pool, set);
}

VkDescriptorSetLayout createDescriptorArrayLayout(VkDevice _device) {
    VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding setBinding = {};
    setBinding.binding = 0;
    setBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    setBinding.descriptorCount = DESCRIPTOR_LIMIT;
    setBinding.stageFlags = stageFlags;
    setBinding.pImmutableSamplers = VK_NULL_HANDLE;

    VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo setBindingFlags{};
    setBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    setBindingFlags.pBindingFlags = &bindingFlags;
    setBindingFlags.bindingCount = 1;

    VkDescriptorSetLayoutCreateInfo setCreateInfo{};
    setCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    setCreateInfo.bindingCount = 1;
    setCreateInfo.pBindings = &setBinding;
    setCreateInfo.pNext = &setBindingFlags;

    VkDescriptorSetLayout layout = 0;
    VK_CHECK(vkCreateDescriptorSetLayout(_device, &setCreateInfo, nullptr, &layout));

    return layout;
}

VkPipeline createGraphicsPipeline(VkDevice _device, VkPipelineCache _pipelineCache, const VkPipelineRenderingCreateInfo& _renderingInfo, const Program& _program, Constants constants) {
    std::vector<VkSpecializationMapEntry> specializationEntries;
    VkSpecializationInfo specializationInfo = fillSpecializationInfo(specializationEntries, constants);

    std::vector<VkPipelineShaderStageCreateInfo> stages(_program.shaderCount);
    std::vector<VkShaderModuleCreateInfo> modules(_program.shaderCount);
    for (size_t i = 0; i < _program.shaderCount; i++) {
        const Shader* shader = _program.shaders[i];

        VkShaderModuleCreateInfo& module = modules[i];
        module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        module.codeSize = shader->spirvCode.size();
        module.pCode = reinterpret_cast<const uint32_t*>(shader->spirvCode.data());

        VkPipelineShaderStageCreateInfo& stage = stages[i];
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = shader->stage;
        stage.pName = "main";
        stage.pSpecializationInfo = &specializationInfo;
        stage.pNext = &module;
    }

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDescription;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();


    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationState{};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.lineWidth = 1.0f;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationState.depthBiasEnable = VK_TRUE;

    VkPipelineMultisampleStateCreateInfo multisampleState{};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER;

    // for deferred rendering :D
    VkPipelineColorBlendAttachmentState colorAttachmentStates[DYNAMIC_COLOR_ATTACHMENT_COUNT] = {};
    assert(_renderingInfo.colorAttachmentCount <= DYNAMIC_COLOR_ATTACHMENT_COUNT);
    for (uint32_t i = 0; i < _renderingInfo.colorAttachmentCount; i++) {
        colorAttachmentStates[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = _renderingInfo.colorAttachmentCount;
    colorBlendState.pAttachments = colorAttachmentStates;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_CULL_MODE, VK_DYNAMIC_STATE_DEPTH_BIAS };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.pVertexInputState = &vertexInput;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pDepthStencilState = &depthStencilState;
    createInfo.pRasterizationState = &rasterizationState;
    createInfo.pViewportState = &viewportState;
    createInfo.pColorBlendState = &colorBlendState;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pDynamicState = &dynamicState;
    createInfo.renderPass = VK_NULL_HANDLE;
    createInfo.layout = _program.layout;
    createInfo.stageCount = uint32_t(stages.size());
    createInfo.pStages = stages.data();
    createInfo.flags = 0;
    createInfo.pNext = &_renderingInfo;

    VkPipeline pipeline = 0;
    VK_CHECK(vkCreateGraphicsPipelines(_device, _pipelineCache, 1, &createInfo, nullptr, &pipeline));

    return pipeline;
}
