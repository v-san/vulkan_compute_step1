#include <vulkan/vulkan.h>

#include <vector>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <cmath>
#include <thread>
#include <iostream>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

#include "../shaders/shaderCommon.h"
#include "vk_utils.h"
#include "Bitmap.h" // Save bmp file


class ComputeApplication
{
private:
  struct Pixel
  {
      float r, g, b, a;
  };

  static constexpr unsigned cmdSubmitIterations = 1;

  VkInstance instance;

  VkDebugReportCallbackEXT debugReportCallback;
  VkPhysicalDevice physicalDevice;
  VkDevice device;

  VkPipeline       pipeline;
  VkPipelineLayout pipelineLayout;
  VkShaderModule   computeShaderModule;

  VkCommandPool   commandPool1;
  VkCommandPool   commandPool2;

  VkDescriptorPool      descriptorPool;
  VkDescriptorSet       descriptorSet;
  VkDescriptorSetLayout descriptorSetLayout;

  VkBuffer       fractalBuffer;
  VkDeviceMemory bufferMemory;

  struct pushConstants
  {
    uint32_t offX;
    uint32_t offY;
  };

  std::vector<const char *> enabledLayers;

  VkQueue queue1;
  VkQueue queue2;

  static constexpr unsigned long long FENCE_TIMEOUT = 100000000000ul;

public:

  void run(unsigned deviceId = 0, const std::vector<uint32_t> &queueFamilyIndices = {0, 2})
  {
    assert(queueFamilyIndices.size() == 2);

    std::cout << "init vulkan for device " << deviceId << " ... " << std::endl;

    instance = vk_utils::CreateInstance(enableValidationLayers, enabledLayers);

    if(enableValidationLayers)
    {
      vk_utils::InitDebugReportCallback(instance, &debugReportCallbackFn, &debugReportCallback);
    }

    physicalDevice = vk_utils::FindPhysicalDevice(instance, true, deviceId);

    device = vk_utils::CreateLogicalDevice(queueFamilyIndices, physicalDevice, enabledLayers);

    vkGetDeviceQueue(device, queueFamilyIndices[0], 0, &queue1);
    vkGetDeviceQueue(device, queueFamilyIndices[1], 0, &queue2);

    size_t bufferSize = sizeof(Pixel) * WIDTH * HEIGHT;

    std::cout << "creating resources ... " << std::endl;
    createBuffer(device, physicalDevice, bufferSize, &fractalBuffer, &bufferMemory, queueFamilyIndices);

    createDescriptorSetLayout(device, &descriptorSetLayout);
    createDescriptorSetForOurBuffer(device, fractalBuffer, bufferSize, &descriptorSetLayout,
                                    &descriptorPool, &descriptorSet);

    std::cout << "compiling shaders  ... " << std::endl;
    createComputePipeline(device, descriptorSetLayout,
                          &computeShaderModule, &pipeline, &pipelineLayout);

    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    createStagingBuffer(device, physicalDevice, bufferSize, &stagingBuf, &stagingMem);

    constexpr unsigned total = WIDTH * HEIGHT;
    constexpr unsigned perTileX = TILE_X;
    constexpr unsigned perTileY = TILE_Y;
    constexpr unsigned perTile = perTileX * perTileY;
    constexpr unsigned nTilesX  = WIDTH / perTileX;
    constexpr unsigned nTilesY  = HEIGHT / perTileY;
    constexpr unsigned nTiles  = nTilesX * nTilesY;
    std::vector<VkCommandBuffer> cmds1(nTiles / 2);
    std::vector<VkCommandBuffer> cmds2(nTiles / 2);

    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices[0];
    VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool1));

    VkCommandPoolCreateInfo commandPoolCreateInfo2 = {};
    commandPoolCreateInfo2.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo2.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo2.queueFamilyIndex = queueFamilyIndices[1];
    VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo2, nullptr, &commandPool2));

    createCommandBuffers(device, commandPool1, cmds1, cmds1.size());
    createCommandBuffers(device, commandPool2, cmds2, cmds2.size());

    size_t idx1 = 0;
    size_t idx2 = 0;
    for(size_t i = 0; i < nTilesY; ++i)
    {
      for(size_t j = 0; j < nTilesX; ++j)
      {
        if((i + j) % 2 == 0)
        {
          recordCommandsTo(cmds1[idx1], pipeline, pipelineLayout, descriptorSet,
                           perTileX, perTileX * j, perTileY, perTileY * i);
          idx1++;
        }
        else
        {
          recordCommandsTo(cmds2[idx2], pipeline, pipelineLayout, descriptorSet,
                           perTileX, perTileX * j, perTileY, perTileY * i);
          idx2++;
        }
      }
    }

    std::cout << "doing computations ... " << std::endl;

    std::vector<VkFence> fences(2);
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fences[0]));
    VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fences[1]));

#ifdef MULTITHREADED_SUBMIT
    auto work = [](VkDevice d, VkQueue &q, VkFence fence, std::vector<VkCommandBuffer> &cmds, size_t nIters){
      std::cout << "thread " << std::this_thread::get_id() << " : using queue " << q << std::endl;
      auto perIter = cmds.size() / nIters;
      for(size_t i = 0; i < nIters; ++i)
      {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = perIter;
        submitInfo.pCommandBuffers = cmds.data() + i * perIter;
        VK_CHECK_RESULT(vkQueueSubmit(q, 1, &submitInfo, fence));
        VK_CHECK_RESULT(vkWaitForFences(d, 1, &fence, VK_TRUE, FENCE_TIMEOUT));
        vkResetFences(d, 1, &fence);
      }
    };
    std::vector<std::thread> workers(2);
    workers[0] = std::move(std::thread(work, device, std::ref(queue1), fences[0], std::ref(cmds1), cmdSubmitIterations));
    workers[1] = std::move(std::thread(work, device, std::ref(queue2), fences[1], std::ref(cmds2), cmdSubmitIterations));

    for(size_t i = 0; i < 2; ++i)
    {
      if(workers[i].joinable())
        workers[i].join();
    }
#else
    auto perIter = cmds1.size() / cmdSubmitIterations;
    for (size_t i = 0; i < cmdSubmitIterations; ++i)
    {
      {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = perIter;
        submitInfo.pCommandBuffers = cmds1.data() + i * perIter;
        VK_CHECK_RESULT(vkQueueSubmit(queue1, 1, &submitInfo, fences[0]));
      }
      {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = perIter;
        submitInfo.pCommandBuffers = cmds2.data() + i * perIter;
        VK_CHECK_RESULT(vkQueueSubmit(queue2, 1, &submitInfo, fences[1]));
      }


      VK_CHECK_RESULT(vkWaitForFences(device, 2, fences.data(), VK_TRUE, FENCE_TIMEOUT));
      vkResetFences(device, 2, fences.data());
    }

#endif


    vkDestroyFence(device, fences[0], nullptr);
    vkDestroyFence(device, fences[1], nullptr);

    std::cout << "saving image       ... " << std::endl;
    saveRenderedImageFromDeviceMemory(device, fractalBuffer, stagingBuf, stagingMem, commandPool1, queue1, 0, WIDTH, HEIGHT);
    std::cout << "destroying all     ... " << std::endl;

    vkDestroyBuffer(device, stagingBuf, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);
    cleanup();
  }


  static void saveRenderedImageFromDeviceMemory(VkDevice a_device, VkBuffer a_srcBuf,
                                                VkBuffer a_stagingBuf, VkDeviceMemory a_stagingBufferMemory,
                                                VkCommandPool a_cmdPool, VkQueue a_queue,
                                                size_t a_offset, int a_width, int a_height)
  {
    std::vector<unsigned char> image;
    image.reserve(a_width * a_height * 4);

    VkCommandBuffer copyBuf;
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = a_cmdPool;
    commandBufferAllocateInfo.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    VK_CHECK_RESULT(vkAllocateCommandBuffers(a_device, &commandBufferAllocateInfo, &copyBuf));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(copyBuf, &beginInfo);
    VkBufferCopy region0 = {};
    region0.srcOffset    = 0;
    region0.dstOffset    = 0;
    region0.size         = WIDTH * HEIGHT * sizeof(Pixel);
    vkCmdCopyBuffer(copyBuf, a_srcBuf, a_stagingBuf, 1, &region0);
    vkEndCommandBuffer(copyBuf);

    VkSubmitInfo submitInfo       = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &copyBuf;

    VkFence fence;
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;

    VK_CHECK_RESULT(vkCreateFence(a_device, &fenceCreateInfo, nullptr, &fence));
    VK_CHECK_RESULT(vkQueueSubmit(a_queue, 1, &submitInfo, fence));
    VK_CHECK_RESULT(vkWaitForFences(a_device, 1, &fence, VK_TRUE, FENCE_TIMEOUT));

    vkDestroyFence(a_device, fence, nullptr);

    void* mappedMemory = nullptr;
    for (int i = 0; i < a_height; i += 1)
    {
      size_t offset = a_offset + i * a_width * sizeof(Pixel);

      mappedMemory = nullptr;
      vkMapMemory(a_device, a_stagingBufferMemory, offset, a_width * sizeof(Pixel), 0, &mappedMemory);
      auto pmappedMemory = static_cast<Pixel*>(mappedMemory);

      for (int j = 0; j < a_width; j += 1)
      {
        image.push_back((unsigned char)(255.0f * (pmappedMemory[j].r)));
        image.push_back((unsigned char)(255.0f * (pmappedMemory[j].g)));
        image.push_back((unsigned char)(255.0f * (pmappedMemory[j].b)));
        image.push_back((unsigned char)(255.0f * (pmappedMemory[j].a)));
      }

      vkUnmapMemory(a_device, a_stagingBufferMemory);
    }

    SaveBMP("mandelbrot.bmp", (const uint32_t*)image.data(), WIDTH, HEIGHT);
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFn(
      VkDebugReportFlagsEXT                       flags,
      VkDebugReportObjectTypeEXT                  objectType,
      uint64_t                                    object,
      size_t                                      location,
      int32_t                                     messageCode,
      const char*                                 pLayerPrefix,
      const char*                                 pMessage,
      void*                                       pUserData)
  {
      printf("Debug Report: %s: %s\n", pLayerPrefix, pMessage);
      return VK_FALSE;
  }


  static void createBuffer(VkDevice a_device, VkPhysicalDevice a_physDevice, const size_t a_bufferSize,
                           VkBuffer* a_pBuffer, VkDeviceMemory* a_pBufferMemory, const std::vector<uint32_t>& queueFamilyIndices)
  {

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size        = a_bufferSize;
    bufferCreateInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
    bufferCreateInfo.queueFamilyIndexCount = queueFamilyIndices.size();
    bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();

    VK_CHECK_RESULT(vkCreateBuffer(a_device, &bufferCreateInfo, nullptr, a_pBuffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(a_device, (*a_pBuffer), &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize  = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(memoryRequirements.memoryTypeBits,
                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, a_physDevice);

    VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, nullptr, a_pBufferMemory));

    VK_CHECK_RESULT(vkBindBufferMemory(a_device, (*a_pBuffer), (*a_pBufferMemory), 0));
  }

  static void createStagingBuffer(VkDevice a_device, VkPhysicalDevice a_physDevice, const size_t a_bufferSize,
                                  VkBuffer* a_pBuffer, VkDeviceMemory* a_pBufferMemory)
  {

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size        = a_bufferSize;
    bufferCreateInfo.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateBuffer(a_device, &bufferCreateInfo, nullptr, a_pBuffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(a_device, (*a_pBuffer), &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize  = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(memoryRequirements.memoryTypeBits,
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                            a_physDevice);

    VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, nullptr, a_pBufferMemory));

    VK_CHECK_RESULT(vkBindBufferMemory(a_device, (*a_pBuffer), (*a_pBufferMemory), 0));
  }


  static void createDescriptorSetLayout(VkDevice a_device, VkDescriptorSetLayout* a_pDSLayout)
  {
     VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
     descriptorSetLayoutBinding.binding         = 0;
     descriptorSetLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
     descriptorSetLayoutBinding.descriptorCount = 1;
     descriptorSetLayoutBinding.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

     VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
     descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
     descriptorSetLayoutCreateInfo.bindingCount = 1;
     descriptorSetLayoutCreateInfo.pBindings    = &descriptorSetLayoutBinding;
     VK_CHECK_RESULT(vkCreateDescriptorSetLayout(a_device, &descriptorSetLayoutCreateInfo, nullptr, a_pDSLayout));
  }

  static void createDescriptorSetForOurBuffer(VkDevice a_device, VkBuffer a_buffer, size_t a_bufferSize,
                                              const VkDescriptorSetLayout* a_pDSLayout,
                                              VkDescriptorPool* a_pDSPool, VkDescriptorSet* a_pDS)
  {

    VkDescriptorPoolSize descriptorPoolSize = {};
    descriptorPoolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorPoolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets       = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes    = &descriptorPoolSize;

    VK_CHECK_RESULT(vkCreateDescriptorPool(a_device, &descriptorPoolCreateInfo, nullptr, a_pDSPool));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool     = (*a_pDSPool);
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts        = a_pDSLayout;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(a_device, &descriptorSetAllocateInfo, a_pDS));

    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = a_buffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range  = a_bufferSize;

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet          = (*a_pDS);
    writeDescriptorSet.dstBinding      = 0;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSet.pBufferInfo     = &descriptorBufferInfo;

    vkUpdateDescriptorSets(a_device, 1, &writeDescriptorSet, 0, nullptr);
  }

  static void createComputePipeline(VkDevice a_device, const VkDescriptorSetLayout& a_dsLayout,
                                    VkShaderModule* a_pShaderModule, VkPipeline* a_pPipeline, VkPipelineLayout* a_pPipelineLayout)
  {

    std::vector<uint32_t> code = vk_utils::ReadFile("../shaders/comp.spv");
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode    = code.data();
    createInfo.codeSize = code.size()*sizeof(uint32_t);
    VK_CHECK_RESULT(vkCreateShaderModule(a_device, &createInfo, nullptr, a_pShaderModule));


    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
    shaderStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfo.module = (*a_pShaderModule);
    shaderStageCreateInfo.pName  = "main";

    VkPushConstantRange pcRange = {};
    pcRange.size = sizeof(pushConstants);
    pcRange.offset = 0;
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts    = &a_dsLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pcRange;
    VK_CHECK_RESULT(vkCreatePipelineLayout(a_device, &pipelineLayoutCreateInfo, nullptr, a_pPipelineLayout));

    VkComputePipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stage  = shaderStageCreateInfo;
    pipelineCreateInfo.layout = (*a_pPipelineLayout);

    VK_CHECK_RESULT(vkCreateComputePipelines(a_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, a_pPipeline));
  }

  static void createCommandBuffers(VkDevice a_device, VkCommandPool &a_pool, std::vector<VkCommandBuffer> &cmdBufs, size_t nBufs)
  {
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = a_pool;
    commandBufferAllocateInfo.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = nBufs;
    VK_CHECK_RESULT(vkAllocateCommandBuffers(a_device, &commandBufferAllocateInfo, cmdBufs.data()));
  }

  static void recordCommandsTo(VkCommandBuffer a_cmdBuff, VkPipeline a_pipeline, VkPipelineLayout a_layout, const VkDescriptorSet& a_ds,
                               uint32_t xWork, uint32_t xOffset, uint32_t yWork, uint32_t yOffset)
  {

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

    pushConstants pcData {xOffset, yOffset};

    vkCmdPushConstants(a_cmdBuff, a_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcData), &pcData);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, a_pipeline);
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, a_layout, 0, 1, &a_ds, 0, NULL);

    vkCmdDispatch(a_cmdBuff, (xWork + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE,
                  (yWork + WORKGROUP_SIZE) / WORKGROUP_SIZE,
                  1);

    VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
  }

  void cleanup()
  {

      if (enableValidationLayers)
      {
          auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
          if (func == nullptr)
          {
              throw std::runtime_error("Could not load vkDestroyDebugReportCallbackEXT");
          }
          func(instance, debugReportCallback, nullptr);
      }

      vkFreeMemory(device, bufferMemory, nullptr);
      vkDestroyBuffer(device, fractalBuffer, nullptr);
      vkDestroyShaderModule(device, computeShaderModule, nullptr);
      vkDestroyDescriptorPool(device, descriptorPool, nullptr);
      vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
      vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
      vkDestroyPipeline(device, pipeline, nullptr);
      vkDestroyCommandPool(device, commandPool1, nullptr);
      vkDestroyCommandPool(device, commandPool2, nullptr);
      vkDestroyDevice(device, nullptr);
      vkDestroyInstance(instance, nullptr);
  }
};

int main()
{
  ComputeApplication app;

  constexpr unsigned deviceId = 1;

  try
  {
    app.run(deviceId);
  }
  catch (const std::exception& e)
  {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }
    
  return EXIT_SUCCESS;
}
