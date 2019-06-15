//
// Created by frol on 15.06.19.
//

#ifndef VULKAN_MINIMAL_COMPUTE_VK_UTILS_H
#define VULKAN_MINIMAL_COMPUTE_VK_UTILS_H

#include <vulkan/vulkan.h>
#include <vector>

namespace vk_utils
{

  typedef VkBool32 (VKAPI_PTR *DebugReportCallbackFuncType)(VkDebugReportFlagsEXT      flags,
                                                            VkDebugReportObjectTypeEXT objectType,
                                                            uint64_t                   object,
                                                            size_t                     location,
                                                            int32_t                    messageCode,
                                                            const char*                pLayerPrefix,
                                                            const char*                pMessage,
                                                            void*                      pUserData);



  VkInstance CreateInstance(bool a_enableValidationLayers, std::vector<const char *>& a_enabledLayers);
  
  void InitDebugReportCallback(VkInstance a_instance, DebugReportCallbackFuncType a_callback, VkDebugReportCallbackEXT* a_debugReportCallback);

};


// Used for validating return values of Vulkan API calls.
//
#define VK_CHECK_RESULT(f) 													\
{																										\
    VkResult res = (f);															\
    if (res != VK_SUCCESS)													\
    {																								\
        printf("Fatal : VkResult is %d in %s at line %d\n", res,  __FILE__, __LINE__); \
        assert(res == VK_SUCCESS);									\
    }																								\
}

#endif //VULKAN_MINIMAL_COMPUTE_VK_UTILS_H
