#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>

const int WIDTH = 800;
const int HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Helper function to call Vulkan extension functions.
// !!! Argument list is NOT checked for correctness. !!!
// R = return type of function
// S = function pointer type for fx being called
// name = name of function to be called
template<typename R, typename S, typename... Args>
R callVKfx(const char* name, VkInstance instance, Args... args) {
    //auto func = (R(*)(VkInstance, Args...)) vkGetInstanceProcAddr(instance, name);
    auto func = (S) vkGetInstanceProcAddr(instance, name);
    if (func != nullptr) {
        return func(instance, args...);
    } else {
        throw std::runtime_error("VK_ERROR_EXTENSION_NOT_PRESENT");
    }
}

// Does the thing where you call a Vulkan function first to figure out how big the output is, then call it again to fill a vector.
template<typename S, typename... Args>
std::vector<S> getVkVector(void (*fx)(Args..., std::uint32_t*, S*), Args... args) {
    std::uint32_t count = 0;
    fx(args..., &count, nullptr);
    std::vector<S> result(count);
    fx(args..., &count, result.data());
    return result;
}

struct QueueFamilyIndices {
    bool indexFound;
    std::uint32_t graphicsFamily;
};

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
    
private:
    GLFWwindow* window;  // GLFW window.
    VkInstance instance; // Vulkan instance.
    VkDebugUtilsMessengerEXT debugMessenger; // Debug messenger object.
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device; // "Logical" device
    VkQueue graphicsQueue; // Graphics queue

    void initWindow() {
        glfwInit(); // Initialize GLFW
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }
    
    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        pickPhysicalDevice();
        createLogicalDevice();
    }
    
    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        
        // Queue setup.
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
        queueCreateInfo.queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        
        // Leaving all values as VK_FALSE
        VkPhysicalDeviceFeatures deviceFeatures = {};
        
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;
        
        createInfo.enabledExtensionCount = 0;
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }
        vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
    }

    void pickPhysicalDevice() {
        std::uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support! Get a better computer LOSER!!!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        
        double score = 0;
        for (const auto& device : devices) {
            double devScore = deviceScore(device);
            if (devScore > score) {
                physicalDevice = device;
                score = devScore;
            }
        }
        
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        } else {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
            std::cout << "Using GPU: " << deviceProperties.deviceName << std::endl;
        }
    }
    
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices{false, 0};
//        std::uint32_t queueFamilyCount = 0;
//        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        auto queueFamilies = getVkVector<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, device);
        
        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
                indices.indexFound = true;
            }
            
            if (indices.indexFound) {
                break;
            }
            
            i++;
        }
        
        return indices;
    }
    
    double deviceScore(VkPhysicalDevice device) {
        double score = 0;
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        
        std::cout << deviceProperties.deviceName << ": ";
        
        score += deviceProperties.limits.maxImageDimension2D;
        score += deviceProperties.limits.maxImageDimension3D;

        // Example of required feature support.
        // if (!deviceFeatures.tessellationShader) {
        //       score = -1;
        // }
        
        QueueFamilyIndices indices = findQueueFamilies(device);
        if (!indices.indexFound) {
            score = -1;
        }
        
        std::cout << score << std::endl;
        return score;
    }

    void createInstance() {
        // Enumerate available extensions.
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        // Print the extensions.
        std::cout << "Available extensions:" << std::endl;
        for (const auto& extension : extensions) {
            std::cout << extension.extensionName << std::endl;
        }
        
        // App info for instance.
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        
        // Instance creation info.
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        // Query for extensions we need.
        auto reqExtensions = getRequiredExtensions();
        
        // Check that GLFW requested extensions are available.
        std::cout << "Requested extensions:" << std::endl;
        for (auto reqExtension : reqExtensions) {
            std::cout << reqExtension << "... ";
            bool isPresent = false;
            for (const auto& extension : extensions) {
                if (std::strcmp(extension.extensionName, reqExtension)) {
                    isPresent = true;
                    break;
                };
            }
            std::cout << (isPresent ? "present." : "not available.") << std::endl;
        }
        
        // Enable requested extensions when creating instance.
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(reqExtensions.size());
        createInfo.ppEnabledExtensionNames = reqExtensions.data();
        
        // Check for validation layers if in debug mode.
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
        
        // Not enabling any validation layers.
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }
        // Finally create the instance using the standard allocator.
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create instance.");
        }
    }
    
    bool checkValidationLayerSupport() {
        std::uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        bool allLayersAvailable = true;

        std::cout << "Requested validation layer:" << std::endl;
        for (const auto& requestedLayer : validationLayers) {
            std::cout << requestedLayer << "... ";
            bool isPresent = false;
            for (const auto& availableLayer : availableLayers) {
                if (std::strcmp(availableLayer.layerName, requestedLayer)) {
                    isPresent = true;
                    break;
                };
            }
            std::cout << (isPresent ? "present." : "not available.") << std::endl;
            allLayersAvailable &= isPresent;
        }
        
        return allLayersAvailable;
    }
    
    std::vector<const char*> getRequiredExtensions() {
        std::uint32_t extensionCount = 0;
        
        // Ask GLFW for needed extensions.
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        
        // Convert GLFW extension list to a vector.
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);
        
        // If in debug, add extensions for debug layer callbacks.
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        
        return extensions;
    }
    
    void setupDebugMessenger() {
        if (!enableValidationLayers) return;
        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
           VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
           VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr;
        if (callVKfx<VkResult, PFN_vkCreateDebugUtilsMessengerEXT>("vkCreateDebugUtilsMessengerEXT", instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        };
    }
    
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                      VkDebugUtilsMessageTypeFlagsEXT messageType,
                      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                      void* pUserData) {
        std::cerr << "Validation later: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }
    
    void cleanup() {
        // DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        vkDestroyDevice(device, nullptr);
        callVKfx<void, PFN_vkDestroyDebugUtilsMessengerEXT>("vkDestroyDebugUtilsMessengerEXT", instance, debugMessenger, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    HelloTriangleApplication app;
    
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
