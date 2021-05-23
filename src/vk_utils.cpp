#include "vk_utils.h"

#include <cstring>
#include <cassert>
#include <iostream>
#include <cmath>
#include <algorithm>

#ifdef WIN32
#undef min
#undef max
#endif 

char g_validationLayerData[256];
static const char* g_debugReportExtName  = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkInstance vk_utils::CreateInstance(bool a_enableValidationLayers, std::vector<const char *> a_extensions)
{
  std::vector<const char *> enabledExtensions = std::move(a_extensions);
  std::vector<const char *> enabledLayers;

  if (a_enableValidationLayers)
  {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> layerProperties(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

    bool foundLayer = false;
    std::string found;
    for (VkLayerProperties prop : layerProperties)
    {
//      std::cout << "Found validation layer: " << found <<std::endl;
      if (std::strncmp(prop.layerName, "VK_LAYER_KHRONOS_validation", sizeof(g_validationLayerData)) == 0)
      {
        strncpy(g_validationLayerData, prop.layerName, sizeof(g_validationLayerData));
        enabledLayers.push_back(g_validationLayerData);
        foundLayer = true;
        break;
      }
    }
    if (!foundLayer)
      RUN_TIME_ERROR("VK_LAYER_KHRONOS_validation layer not found\n");

    uint32_t extensionCount;

    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());

    bool foundExtension = false;
    for (VkExtensionProperties prop : extensionProperties) {
      if (strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, prop.extensionName) == 0) {
        foundExtension = true;
        break;
      }
    }

    if (!foundExtension)
      RUN_TIME_ERROR("Extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME not supported\n");

    enabledExtensions.push_back(g_debugReportExtName);
  }

  VkApplicationInfo applicationInfo = {};
  applicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.pApplicationName   = "Async compute test app";
  applicationInfo.applicationVersion = 0;
  applicationInfo.pEngineName        = "Async";
  applicationInfo.engineVersion      = 0;
  applicationInfo.apiVersion         = VK_API_VERSION_1_2;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.flags = 0;
  createInfo.pApplicationInfo = &applicationInfo;

  createInfo.enabledLayerCount       = uint32_t(enabledLayers.size());
  createInfo.ppEnabledLayerNames     = enabledLayers.data();
  createInfo.enabledExtensionCount   = uint32_t(enabledExtensions.size());
  createInfo.ppEnabledExtensionNames = enabledExtensions.data();

  VkInstance instance;
  VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &instance));

  return instance;
}


void vk_utils::InitDebugReportCallback(VkInstance a_instance, DebugReportCallbackFuncType a_callback, VkDebugReportCallbackEXT* a_debugReportCallback)
{
  VkDebugReportCallbackCreateInfoEXT createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
  createInfo.pfnCallback = a_callback;

  auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(a_instance, "vkCreateDebugReportCallbackEXT");
  if (vkCreateDebugReportCallbackEXT == nullptr)
    RUN_TIME_ERROR("Could not load vkCreateDebugReportCallbackEXT");

  VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(a_instance, &createInfo, NULL, a_debugReportCallback));
}


VkPhysicalDevice vk_utils::FindPhysicalDevice(VkInstance a_instance, bool a_printInfo, uint a_preferredDeviceId)
{

  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(a_instance, &deviceCount, NULL);
  if (deviceCount == 0) {
    RUN_TIME_ERROR("vk_utils::FindPhysicalDevice, no Vulkan devices found");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(a_instance, &deviceCount, devices.data());

  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

  if(a_printInfo)
    std::cout << "FindPhysicalDevice: { " << std::endl;

  VkPhysicalDeviceProperties props;
  VkPhysicalDeviceFeatures   features;

  if(devices.empty())
    RUN_TIME_ERROR("vk_utils::FindPhysicalDevice, no Vulkan devices found");

  for (int i = 0; i < devices.size(); i++)
  {
    vkGetPhysicalDeviceProperties(devices[i], &props);
    vkGetPhysicalDeviceFeatures(devices[i], &features);

    if(a_printInfo)
      std::cout << "  device " << i << ", name = " << props.deviceName << std::endl;

    if(i == a_preferredDeviceId)
      physicalDevice = devices[i];
  }
  if(a_printInfo)
    std::cout << "}" << std::endl;

  if(physicalDevice == VK_NULL_HANDLE)
  {
    physicalDevice = devices[0];
  }
  vkGetPhysicalDeviceProperties(physicalDevice, &props);
  std::cout << "Using device: " <<  props.deviceName << std::endl;

  return physicalDevice;
}

uint32_t vk_utils::GetComputeQueueFamilyIndex(VkPhysicalDevice a_physicalDevice)
{
  return vk_utils::GetQueueFamilyIndex(a_physicalDevice, VK_QUEUE_COMPUTE_BIT);
}

uint32_t vk_utils::GetQueueFamilyIndex(VkPhysicalDevice a_physicalDevice, VkQueueFlagBits a_bits)
{
  uint32_t queueFamilyCount;

  vkGetPhysicalDeviceQueueFamilyProperties(a_physicalDevice, &queueFamilyCount, NULL);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(a_physicalDevice, &queueFamilyCount, queueFamilies.data());

  uint32_t i = 0;
  for (; i < queueFamilies.size(); ++i)
  {
    VkQueueFamilyProperties props = queueFamilies[i];

    if (props.queueCount > 0 && (props.queueFlags & a_bits))
      break;
  }

  if (i == queueFamilies.size())
    RUN_TIME_ERROR(" vk_utils::GetComputeQueueFamilyIndex: could not find a queue family that supports operations");

  return i;
}


VkDevice vk_utils::CreateLogicalDevice(const std::vector<uint32_t> &queueFamilyIndices, VkPhysicalDevice physicalDevice, const std::vector<const char *>& a_enabledLayers, std::vector<const char *> a_extentions)
{
  std::vector<VkDeviceQueueCreateInfo> qI;
  float queuePriorities = 0.0;
  for(const auto& idx : queueFamilyIndices)
  {
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = idx;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriorities;

    qI.push_back(queueCreateInfo);
  }

  VkDeviceCreateInfo deviceCreateInfo = {};


  VkPhysicalDeviceFeatures deviceFeatures = {};

  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.enabledLayerCount    = uint32_t(a_enabledLayers.size());
  deviceCreateInfo.ppEnabledLayerNames  = a_enabledLayers.data();
  deviceCreateInfo.pQueueCreateInfos    = qI.data();
  deviceCreateInfo.queueCreateInfoCount = qI.size();
  deviceCreateInfo.pEnabledFeatures     = &deviceFeatures;
  deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(a_extentions.size());
  deviceCreateInfo.ppEnabledExtensionNames = a_extentions.data();

  VkDevice device;
  VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device));

  return device;
}


uint32_t vk_utils::FindMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice)
{
  VkPhysicalDeviceMemoryProperties memoryProperties;

  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
  {
    if ((memoryTypeBits & (1 << i)) &&
        ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
      return i;
  }

  return -1;
}

std::vector<uint32_t> vk_utils::ReadFile(const char* filename)
{
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL)
  {
    std::string errorMsg = std::string("vk_utils::ReadFile, can't open file ") + std::string(filename);
    RUN_TIME_ERROR(errorMsg.c_str());
  }

  fseek(fp, 0, SEEK_END);
  long filesize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  long filesizepadded = long(ceil(filesize / 4.0)) * 4;

  std::vector<uint32_t> resData(filesizepadded/4);

  char *str = (char*)resData.data();
  size_t nRead = fread(str, filesize, sizeof(char), fp);
  if(nRead != 1)
  {
    std::string errorMsg = std::string("vk_utils::ReadFile, can't read file ") + std::string(filename);
    RUN_TIME_ERROR(errorMsg.c_str());
  }
  fclose(fp);

  for (auto i = filesize; i < filesizepadded; i++)
    str[i] = 0;

  return resData;
}

VkShaderModule vk_utils::CreateShaderModule(VkDevice a_device, const std::vector<uint32_t>& code)
{
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size() * sizeof(uint32_t);
  createInfo.pCode = code.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(a_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    throw std::runtime_error("[CreateShaderModule]: failed to create shader module!");

  return shaderModule;
}


