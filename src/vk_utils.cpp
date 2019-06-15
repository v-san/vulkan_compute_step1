//
// Created by frol on 15.06.19.
//

#include "vk_utils.h"

#include <string.h>
#include <assert.h>
#include <iostream>


VkInstance vk_utils::CreateInstance(bool a_enableValidationLayers, std::vector<const char *>& a_enabledLayers)
{
  std::vector<const char *> enabledExtensions;

  /*
  By enabling validation layers, Vulkan will emit warnings if the API
  is used incorrectly. We shall enable the layer VK_LAYER_LUNARG_standard_validation,
  which is basically a collection of several useful validation layers.
  */
  if (a_enableValidationLayers)
  {
    /*
    We get all supported layers with vkEnumerateInstanceLayerProperties.
    */
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);

    std::vector<VkLayerProperties> layerProperties(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

    /*
    And then we simply check if VK_LAYER_LUNARG_standard_validation is among the supported layers.
    */
    bool foundLayer = false;
    for (VkLayerProperties prop : layerProperties) {

      if (strcmp("VK_LAYER_LUNARG_standard_validation", prop.layerName) == 0) {
        foundLayer = true;
        break;
      }

    }

    if (!foundLayer)
      RUN_TIME_ERROR("Layer VK_LAYER_LUNARG_standard_validation not supported\n");

    a_enabledLayers.push_back("VK_LAYER_LUNARG_standard_validation"); // Alright, we can use this layer.

    /*
    We need to enable an extension named VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    in order to be able to print the warnings emitted by the validation layer.

    So again, we just check if the extension is among the supported extensions.
    */

    uint32_t extensionCount;

    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensionProperties.data());

    bool foundExtension = false;
    for (VkExtensionProperties prop : extensionProperties) {
      if (strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, prop.extensionName) == 0) {
        foundExtension = true;
        break;
      }

    }

    if (!foundExtension)
      RUN_TIME_ERROR("Extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME not supported\n");

    enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }

  /*
  Next, we actually create the instance.
  */

  /*
  Contains application info. This is actually not that important.
  The only real important field is apiVersion.
  */
  VkApplicationInfo applicationInfo = {};
  applicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.pApplicationName   = "Hello world app";
  applicationInfo.applicationVersion = 0;
  applicationInfo.pEngineName        = "awesomeengine";
  applicationInfo.engineVersion      = 0;
  applicationInfo.apiVersion         = VK_API_VERSION_1_0;;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.flags = 0;
  createInfo.pApplicationInfo = &applicationInfo;

  // Give our desired layers and extensions to vulkan.
  createInfo.enabledLayerCount       = a_enabledLayers.size();
  createInfo.ppEnabledLayerNames     = a_enabledLayers.data();
  createInfo.enabledExtensionCount   = enabledExtensions.size();
  createInfo.ppEnabledExtensionNames = enabledExtensions.data();

  /*
  Actually create the instance.
  Having created the instance, we can actually start using vulkan.
  */
  VkInstance instance;
  VK_CHECK_RESULT(vkCreateInstance(&createInfo, NULL, &instance));

  return instance;
}


void vk_utils::InitDebugReportCallback(VkInstance a_instance, DebugReportCallbackFuncType a_callback, VkDebugReportCallbackEXT* a_debugReportCallback)
{
  // Register a callback function for the extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME, so that warnings emitted from the validation
  // layer are actually printed.

  VkDebugReportCallbackCreateInfoEXT createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
  createInfo.pfnCallback = a_callback;

  // We have to explicitly load this function.
  //
  auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(a_instance, "vkCreateDebugReportCallbackEXT");
  if (vkCreateDebugReportCallbackEXT == nullptr)
    RUN_TIME_ERROR("Could not load vkCreateDebugReportCallbackEXT");

  // Create and register callback.
  VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(a_instance, &createInfo, NULL, a_debugReportCallback));
}


VkPhysicalDevice vk_utils::FindPhysicalDevice(VkInstance a_instance, bool a_printInfo, int a_preferredDeviceId)
{
  /*
  In this function, we find a physical device that can be used with Vulkan.
  */

  /*
  So, first we will list all physical devices on the system with vkEnumeratePhysicalDevices .
  */
  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(a_instance, &deviceCount, NULL);
  if (deviceCount == 0) {
    RUN_TIME_ERROR("vk_utils::FindPhysicalDevice, no Vulkan devices found");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(a_instance, &deviceCount, devices.data());

  /*
  Next, we choose a device that can be used for our purposes.

  With VkPhysicalDeviceFeatures(), we can retrieve a fine-grained list of physical features supported by the device.
  However, in this demo, we are simply launching a simple compute shader, and there are no
  special physical features demanded for this task.

  With VkPhysicalDeviceProperties(), we can obtain a list of physical device properties. Most importantly,
  we obtain a list of physical device limitations. For this application, we launch a compute shader,
  and the maximum size of the workgroups and total number of compute shader invocations is limited by the physical device,
  and we should ensure that the limitations named maxComputeWorkGroupCount, maxComputeWorkGroupInvocations and
  maxComputeWorkGroupSize are not exceeded by our application.  Moreover, we are using a storage buffer in the compute shader,
  and we should ensure that it is not larger than the device can handle, by checking the limitation maxStorageBufferRange.

  However, in our application, the workgroup size and total number of shader invocations is relatively small, and the storage buffer is
  not that large, and thus a vast majority of devices will be able to handle it. This can be verified by looking at some devices at_
  http://vulkan.gpuinfo.org/

  Therefore, to keep things simple and clean, we will not perform any such checks here, and just pick the first physical
  device in the list. But in a real and serious application, those limitations should certainly be taken into account.

  */
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

  if(a_printInfo)
    std::cout << "FindPhysicalDevice: { " << std::endl;

  VkPhysicalDeviceProperties props;
  VkPhysicalDeviceFeatures   features;

  for (int i=0;i<devices.size();i++)
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


  // try to select some device if preferred was not selected
  //
  if(physicalDevice == VK_NULL_HANDLE)
  {
    for (int i=0;i<devices.size();i++)
    {
      if(true)
      {
        physicalDevice = devices[i];
        break;
      }
    }
  }

  if(physicalDevice == VK_NULL_HANDLE)
    RUN_TIME_ERROR("vk_utils::FindPhysicalDevice, no Vulkan devices with compute capability found");

  return physicalDevice;
}
