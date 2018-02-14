#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <inttypes.h>
#include <vulkan/vulkan.h>

#include "shader.c"

#define BAIL_ON_BAD_RESULT(result) \
if (VK_SUCCESS != (result)) { fprintf(stderr, "Failure at %u %s\n", __LINE__, __FILE__); exit(-1); }

char* getPhysicalDeviceTypeString(int physicalDeviceType) {
    switch (physicalDeviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "Other";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
        default: return "Undefined";
    }
}

void printPhysicalDeviceProperties(VkPhysicalDeviceProperties *properties) {
    printf("%s {\n\tapiVersion: %i.%i.%i\n\tdriverVersion: %i\n\tvendorID: %i\n\tdeviceID: %i\n\tdeviceType: %s\n\tdeviceName: %s\n}\n",
            properties->deviceName,
            VK_VERSION_MAJOR(properties->apiVersion),
            VK_VERSION_MINOR(properties->apiVersion),
            VK_VERSION_PATCH(properties->apiVersion),
            properties->driverVersion,
            properties->vendorID,
            properties->deviceID,
            getPhysicalDeviceTypeString(properties->deviceType),
            properties->deviceName
        );
}

void flushUnreadCharacters(FILE *stream) {
    int ch;
    while ((ch = fgetc(stream)) != EOF && ch != '\n') {}
}

uint32_t choosePhysicalDeviceIndex(uint32_t physicalDeviceCount, VkPhysicalDevice *physicalDevices) {
    uint32_t chosenPhysicalDeviceIndex = 0;

    if (physicalDeviceCount <= 1) {
        printf("Automatically choosing the only available device.\n");
    } else {
        do {
            printf("Chosen physical device index: ");

            int result = scanf("%i", &chosenPhysicalDeviceIndex);
            flushUnreadCharacters(stdin);

            if (result != 1) {
                fprintf(stderr, "Invalid input.\n");
                continue;
            }

            if (chosenPhysicalDeviceIndex < 0
                    || chosenPhysicalDeviceIndex >= physicalDeviceCount) {
                fprintf(stderr, "Index out of range.\n");
                continue;
            }

            break;
        } while (true);
    }

    return chosenPhysicalDeviceIndex;
}

// Prefer devices with VK_QUEUE_COMPUTE_BIT only
uint32_t chooseQueueFamilyIndex(uint32_t queueFamilyPropertiesCount, VkQueueFamilyProperties *const queueFamilyProperties) {
    uint32_t index;
    bool foundIndex = false;

    for (uint32_t i = 0; i < queueFamilyPropertiesCount; i += 1) {
        VkQueueFlags flags = queueFamilyProperties[i].queueFlags;

        if (!(VK_QUEUE_GRAPHICS_BIT & flags) && (VK_QUEUE_COMPUTE_BIT & flags))
        {
            return i;
        }

        if (!foundIndex && (VK_QUEUE_COMPUTE_BIT & flags)) {
            index = i;
            foundIndex = true;
        }
    }

    if (!foundIndex) {
        fprintf(stderr, "Could not find any queue on this device with compute capabilities.\n");
        exit(1);
    }

    return index;
}

uint32_t chooseMemoryTypeIndex(VkPhysicalDeviceMemoryProperties *physicalDeviceMemoryProperties, VkDeviceSize memorySize) {
    for (uint32_t i = 0; i < physicalDeviceMemoryProperties->memoryTypeCount; i += 1) {
        VkMemoryType *currentMemoryType = &physicalDeviceMemoryProperties->memoryTypes[i];
        VkMemoryPropertyFlags flags = currentMemoryType->propertyFlags;
        if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & flags) &&
            (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & flags) &&
            (memorySize < physicalDeviceMemoryProperties->memoryHeaps[currentMemoryType->heapIndex].size)) {
            return i;
        }
    }

    fprintf(stderr, "Could not find a sufficient memory type.\n");
    exit(1);
}

static void appendPrefix(size_t *prefixLen, char *prefix, char character) {
    if (*prefixLen > 0) {
        prefix[(*prefixLen)++] = '|';
    }

    prefix[(*prefixLen)++] = character;
}

static void buildPrefix(VkDebugReportFlagsEXT *flags, size_t *prefixLen, char *prefix, VkDebugReportFlagBitsEXT bit, char character) {
    if ((*flags & bit) == bit) {
        appendPrefix(prefixLen, prefix, character);

        *flags -= bit;
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t obj,
        size_t location,
        int32_t code,
        const char *layerPrefix,
        const char *msg,
        void *userData) {
    size_t prefixLen = 0;
    char prefix[12]; // max 5 items, max 1 unknown item, max 5 separators, zero byte

    buildPrefix(&flags, &prefixLen, prefix, VK_DEBUG_REPORT_INFORMATION_BIT_EXT, 'I');
    buildPrefix(&flags, &prefixLen, prefix, VK_DEBUG_REPORT_WARNING_BIT_EXT, 'W');
    buildPrefix(&flags, &prefixLen, prefix, VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT, 'P');
    buildPrefix(&flags, &prefixLen, prefix, VK_DEBUG_REPORT_ERROR_BIT_EXT, 'E');
    buildPrefix(&flags, &prefixLen, prefix, VK_DEBUG_REPORT_DEBUG_BIT_EXT, 'D');

    if (flags != 0) {
        appendPrefix(&prefixLen, prefix, '?');
    }

    prefix[prefixLen] = '\0';

    printf("[%s] %s\n", prefix, msg);

    return VK_FALSE;
}

// Extensions need to be loaded manually
VkResult loadVkCreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
    PFN_vkCreateDebugReportCallbackEXT func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

int main(int argc, char *argv[]) {
    printf("Hello, world.\n");

    const VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "vkcscratch",
        .applicationVersion = 0,
        .pEngineName = NULL,
        .engineVersion = 0,
        .apiVersion = VK_MAKE_VERSION(1, 0, 65),
    };
    const char *enabledLayerNames[] = { "VK_LAYER_LUNARG_standard_validation" };
    const char *enabledExtensionNames[] = { VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
    const VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfoEXT = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT
            | VK_DEBUG_REPORT_WARNING_BIT_EXT
            | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
            | VK_DEBUG_REPORT_ERROR_BIT_EXT
            | VK_DEBUG_REPORT_DEBUG_BIT_EXT,
        .pfnCallback = debugCallback,
        .pUserData = NULL,

    };
    const VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = enabledLayerNames,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = enabledExtensionNames,
    };

    VkInstance instance;
    BAIL_ON_BAD_RESULT(vkCreateInstance(&instanceCreateInfo, 0, &instance));

    VkDebugReportCallbackEXT debugReportCallbackEXT;
    BAIL_ON_BAD_RESULT(loadVkCreateDebugReportCallbackEXT(instance, &debugReportCallbackCreateInfoEXT, NULL, &debugReportCallbackEXT));

    uint32_t physicalDeviceCount;
    BAIL_ON_BAD_RESULT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL));

    VkPhysicalDevice *const physicalDevices = (VkPhysicalDevice*) malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    BAIL_ON_BAD_RESULT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    for (uint32_t physicalDeviceIndex = 0; physicalDeviceIndex < physicalDeviceCount; physicalDeviceIndex += 1) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevices[physicalDeviceIndex], &properties);
        printf("%i: ", physicalDeviceIndex);
        printPhysicalDeviceProperties(&properties);
    }

    uint32_t chosenPhysicalDeviceIndex = choosePhysicalDeviceIndex(
            physicalDeviceCount, physicalDevices);
    VkPhysicalDevice physicalDevice = physicalDevices[chosenPhysicalDeviceIndex];

    uint32_t queueFamilyPropertiesCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, NULL);

    VkQueueFamilyProperties *const queueFamilyProperties =
        (VkQueueFamilyProperties*) malloc(sizeof(VkQueueFamilyProperties) * queueFamilyPropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties);

    uint32_t queueFamilyPropertiesIndex = chooseQueueFamilyIndex(queueFamilyPropertiesCount, queueFamilyProperties);

    const float queuePriorities[] = { 1.0f };
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = queueFamilyPropertiesIndex,
        .queueCount = 1,
        .pQueuePriorities = queuePriorities,
    };

    const VkDeviceQueueCreateInfo queueCreateInfos[] = { deviceQueueCreateInfo };
    const VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = queueCreateInfos, 
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL,
        .pEnabledFeatures = NULL,
    };

    VkDevice device;
    BAIL_ON_BAD_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device));

    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyPropertiesIndex, NULL, &queue);

    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

    const uint32_t bufferLength = 16384;
    const uint32_t bufferSize = sizeof(int32_t) * bufferLength;
    const VkDeviceSize memorySize = bufferSize * 2; // input + output buffer in a single memory
    uint32_t memoryTypeIndex = chooseMemoryTypeIndex(&physicalDeviceMemoryProperties, memorySize);

    const VkMemoryAllocateInfo memoryAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memorySize,
        .memoryTypeIndex = memoryTypeIndex,
    };

    VkDeviceMemory memory;
    BAIL_ON_BAD_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, NULL, &memory));

    uint32_t *payload;
    BAIL_ON_BAD_RESULT(vkMapMemory(device, memory, 0, memorySize, 0, (void**) &payload));

    for (uint32_t i = 0; i < memorySize / sizeof(uint32_t); i += 1) {
        payload[i] = rand();
    }

    vkUnmapMemory(device, memory);

    const uint32_t queueFamilyIndices[] = { queueFamilyPropertiesIndex };
    const VkBufferCreateInfo bufferCreateInfo = (VkBufferCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = queueFamilyIndices,
    };

    VkBuffer inputBuffer;
    BAIL_ON_BAD_RESULT(vkCreateBuffer(device, &bufferCreateInfo, NULL, &inputBuffer));

    // TODO ensure alignment
    VkMemoryRequirements inputBufferMemoryRequirements;
    vkGetBufferMemoryRequirements(device, inputBuffer, &inputBufferMemoryRequirements);
    printf("input { size: %lu, alignment: %lu }\n", inputBufferMemoryRequirements.size, inputBufferMemoryRequirements.alignment);

    BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, inputBuffer, memory, 0));

    VkBuffer outputBuffer;
    BAIL_ON_BAD_RESULT(vkCreateBuffer(device, &bufferCreateInfo, NULL, &outputBuffer));

    // TODO ensure alignment
    VkMemoryRequirements outputBufferMemoryRequirements;
    vkGetBufferMemoryRequirements(device, outputBuffer, &outputBufferMemoryRequirements);
    printf("output { size: %lu, alignment: %lu }\n", outputBufferMemoryRequirements.size, outputBufferMemoryRequirements.alignment);

    BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, outputBuffer, memory, bufferSize));

    uint32_t shaderSize;
    uint32_t *shaderData;

    shaderLoad(&shaderSize, &shaderData);

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = shaderSize,
        .pCode = shaderData,
    };

    printf("shader { size: %u, last: %u }\n", shaderSize, shaderData[shaderSize / sizeof(uint32_t) - 1] - 65536);

    VkShaderModule shaderModule;
    BAIL_ON_BAD_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, 0, &shaderModule));

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        (VkDescriptorSetLayoutBinding) {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        (VkDescriptorSetLayoutBinding) {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = descriptorSetLayoutBindings,
    };

    VkDescriptorSetLayout descriptorSetLayout;
    BAIL_ON_BAD_RESULT(vkCreateDescriptorSetLayout(
                device, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout));

    VkDescriptorSetLayout descriptorSetLayouts[] = { descriptorSetLayout };
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = descriptorSetLayouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    VkPipelineLayout pipelineLayout;
    BAIL_ON_BAD_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout));

    VkComputePipelineCreateInfo computePipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaderModule,
            .pName = "f", // entry point name of the shader for this stage
            .pSpecializationInfo = NULL,
        },
        .layout = pipelineLayout,
        .basePipelineHandle = NULL,
        .basePipelineIndex = 0,
    };

    VkPipeline pipelines[1];
    VkComputePipelineCreateInfo computePipelineCreateInfos[] = { computePipelineCreateInfo };
    BAIL_ON_BAD_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, computePipelineCreateInfos, NULL, pipelines));

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = queueFamilyPropertiesIndex,
    };

    VkDescriptorPoolSize descriptorPoolSize = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 2,
    };

    VkDescriptorPoolSize descriptorPoolSizes[] = { descriptorPoolSize };
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = descriptorPoolSizes,
    };

    VkDescriptorPool descriptorPool;
    BAIL_ON_BAD_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = descriptorSetLayouts,
    };

    VkDescriptorSet descriptorSets[1];
    BAIL_ON_BAD_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, descriptorSets));

    VkDescriptorBufferInfo inputDescriptorBufferInfo = {
        .buffer = inputBuffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkDescriptorBufferInfo outputDescriptorBufferInfo = {
        .buffer = outputBuffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkWriteDescriptorSet writeDescriptorSet[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = descriptorSets[0],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = NULL,
            .pBufferInfo = &inputDescriptorBufferInfo,
            .pTexelBufferView = NULL,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = descriptorSets[0],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = NULL,
            .pBufferInfo = &outputDescriptorBufferInfo,
            .pTexelBufferView = NULL,
        },
    };

    vkUpdateDescriptorSets(device, 2, writeDescriptorSet, 0, NULL);

    VkCommandPool commandPool;
    BAIL_ON_BAD_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer commandBuffer;
    BAIL_ON_BAD_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };

    BAIL_ON_BAD_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[0]);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, descriptorSets, 0, NULL);

    vkCmdDispatch(commandBuffer, bufferSize / sizeof(int32_t), 1, 1);

    BAIL_ON_BAD_RESULT(vkEndCommandBuffer(commandBuffer));

    VkCommandBuffer commandBuffers[] = { commandBuffer };
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = commandBuffers,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    BAIL_ON_BAD_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    BAIL_ON_BAD_RESULT(vkQueueWaitIdle(queue));
    BAIL_ON_BAD_RESULT(vkMapMemory(device, memory, 0, memorySize, 0, (void**) &payload));

    for (uint32_t i = 0, len = bufferSize / sizeof(int32_t); i < len; i++) {
        /* printf("input: %u; output: %u\n", payload[i], payload[i + len]); */
        assert(payload[i + len] == payload[i]);
    }

    return 0;
}
