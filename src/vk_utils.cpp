//
// Created by frol on 15.06.19.
//

#include "vk_utils.h"

#include <string.h>
#include <assert.h>
#include <stdexcept>


VkInstance vk_utils::CreateInstance(bool a_enableValidationLayers,
                                    std::vector<const char *>& a_enabledLayers, DebugReportCallbackFuncType a_callback, VkDebugReportCallbackEXT* a_debugReportCallback)
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

    if (!foundLayer) {
      throw std::runtime_error("Layer VK_LAYER_LUNARG_standard_validation not supported\n");
    }
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

    if (!foundExtension) {
      throw std::runtime_error("Extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME not supported\n");
    }
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
  applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.pApplicationName = "Hello world app";
  applicationInfo.applicationVersion = 0;
  applicationInfo.pEngineName = "awesomeengine";
  applicationInfo.engineVersion = 0;
  applicationInfo.apiVersion = VK_API_VERSION_1_0;;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.flags = 0;
  createInfo.pApplicationInfo = &applicationInfo;

  // Give our desired layers and extensions to vulkan.
  createInfo.enabledLayerCount = a_enabledLayers.size();
  createInfo.ppEnabledLayerNames = a_enabledLayers.data();
  createInfo.enabledExtensionCount = enabledExtensions.size();
  createInfo.ppEnabledExtensionNames = enabledExtensions.data();

  /*
  Actually create the instance.
  Having created the instance, we can actually start using vulkan.
  */
  VkInstance instance;
  VK_CHECK_RESULT(vkCreateInstance(&createInfo, NULL, &instance));

  /*
  Register a callback function for the extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME, so that warnings emitted from the validation
  layer are actually printed.
  */
  if (a_enableValidationLayers)
  {
    VkDebugReportCallbackCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    createInfo.pfnCallback = a_callback;

    // We have to explicitly load this function.
    auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (vkCreateDebugReportCallbackEXT == nullptr) {
      throw std::runtime_error("Could not load vkCreateDebugReportCallbackEXT");
    }

    // Create and register callback.
    VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(instance, &createInfo, NULL, a_debugReportCallback));
  }

  return instance;
}