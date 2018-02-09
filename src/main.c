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
    /* TODO: Check for layer availability */
    // TODO add debug callbacks
    const VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = enabledLayerNames,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL,
    };

    VkInstance instance;
    BAIL_ON_BAD_RESULT(vkCreateInstance(&instanceCreateInfo, 0, &instance));

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
    BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, inputBuffer, memory, 0));

    VkBuffer outputBuffer;
    BAIL_ON_BAD_RESULT(vkCreateBuffer(device, &bufferCreateInfo, NULL, &outputBuffer));
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

    printf("DERP\n");

    VkShaderModule shaderModule;
    BAIL_ON_BAD_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, 0, &shaderModule));

    printf("DERP\n");

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

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
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
    BAIL_ON_BAD_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, computePipelineCreateInfos, NULL, pipelines))

    return 0;
}
