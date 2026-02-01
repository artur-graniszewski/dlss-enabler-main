#pragma once

struct StructType
{
	unsigned long  data1;
	unsigned short data2;
	unsigned short data3;
	unsigned char  data4[8];

	inline bool operator==(const StructType& rhs) const { return memcmp(this, &rhs, sizeof(*this)) == 0; }
	inline bool operator!=(const StructType& rhs) const { return memcmp(this, &rhs, sizeof(*this)) != 0; }
};

struct BaseStructure
{
	BaseStructure() = delete;
	BaseStructure(StructType t, uint32_t v) : structType(t), structVersion(v) {};
	BaseStructure* next{};
	StructType structType{};
	size_t structVersion;
};


#define SL_STRUCT(name, guid, version)                                      \
struct name : public BaseStructure                                          \
{                                                                           \
    name() : BaseStructure(guid, version){}                                 \
    constexpr static StructType s_structType = guid;                        \

#define SL_STRUCT_PROTECTED(name, guid, version)                            \
struct name : public BaseStructure                                          \
{                                                                           \
protected:                                                                  \
    name() : BaseStructure(guid, version){}                                 \
public:                                                                     \
    constexpr static StructType s_structType = guid;                        \


enum class ResourceType : char
{
	eTex2d,
	eBuffer,
	eCommandQueue,
	eCommandBuffer,
	eCommandPool,
	eFence,
	eSwapchain,
	eCount
};

using BufferType = uint32_t;

constexpr uint32_t kStructVersion1 = 1;
constexpr uint32_t kStructVersion2 = 2;

enum ResourceLifecycle
{
	//! Resource can change, get destroyed or reused for other purposes after it is provided to SL
	eOnlyValidNow,
	//! Resource does NOT change, gets destroyed or reused for other purposes from the moment it is provided to SL until the frame is presented
	eValidUntilPresent,
	//! Resource does NOT change, gets destroyed or reused for other purposes from the moment it is provided to SL until after the slEvaluateFeature call has returned.
	eValidUntilEvaluate
};

enum class DeepDVCMode : uint32_t
{
	eOff,
	eOn,
	eCount
};

// {23288AAD-7E7E-BE2A-916F-27DA30A3046B}
SL_STRUCT(DeepDVCOptions, StructType({ 0x23288aad, 0x7e7e, 0xbe2a, { 0x91, 0x67, 0x27, 0xda, 0x30, 0xa3, 0x04, 0x6b } }), kStructVersion1)
//! Specifies which mode should be used
DeepDVCMode mode = DeepDVCMode::eOff;
//! Specifies intensity level in range [0,1]. Default 0.5
float intensity = 0.5f;
//! Specifies saturation boost in range [0,1]. Default 0.25
float saturationBoost = 0.25f;
};

SL_STRUCT(Resource, StructType({ 0x3a9d70cf, 0x2418, 0x4b72, { 0x83, 0x91, 0x13, 0xf8, 0x72, 0x1c, 0x72, 0x61 } }), kStructVersion1)
//! Constructors
//! 
//! Resource type, native pointer are MANDATORY always
//! Resource state is MANDATORY unless using D3D11
//! Resource view, description etc. are MANDATORY only when using Vulkan
//! 
Resource(ResourceType _type, void* _native, void* _mem, void* _view, uint32_t _state = UINT_MAX) : BaseStructure(Resource::s_structType, kStructVersion1), type(_type), native(_native), memory(_mem), view(_view), state(_state) {};
Resource(ResourceType _type, void* _native, uint32_t _state = UINT_MAX) : BaseStructure(Resource::s_structType, kStructVersion1), type(_type), native(_native), state(_state) {};


//! Indicates the type of resource
ResourceType type = ResourceType::eTex2d;
//! ID3D11Resource/ID3D12Resource/VkBuffer/VkImage
void* native{};
//! vkDeviceMemory or nullptr
void* memory{};
//! VkImageView/VkBufferView or nullptr
void* view{};
//! State as D3D12_RESOURCE_STATES or VkImageLayout
//! 
//! IMPORTANT: State is MANDATORY and needs to be correct when tagged resources are actually used.
//! 
uint32_t state = UINT_MAX;
//! Width in pixels
uint32_t width{};
//! Height in pixels
uint32_t height{};
//! Native format
uint32_t nativeFormat{};
//! Number of mip-map levels
uint32_t mipLevels{};
//! Number of arrays
uint32_t arrayLayers{};
//! Virtual address on GPU (if applicable)
uint64_t gpuVirtualAddress{};
//! VkImageCreateFlags
uint32_t flags;
//! VkImageUsageFlags
uint32_t usage{};
//! Reserved for internal use
uint32_t reserved{};

//! IMPORTANT: New members go here or if optional can be chained in a new struct, see sl_struct.h for details
};

struct Extent
{
	uint32_t top{};
	uint32_t left{};
	uint32_t width{};
	uint32_t height{};

	inline operator bool() const { return width != 0 && height != 0; }
	inline bool operator==(const Extent& rhs) const
	{
		return top == rhs.top && left == rhs.left &&
			width == rhs.width && height == rhs.height;
	}
	inline bool operator!=(const Extent& rhs) const
	{
		return !operator==(rhs);
	}
};



SL_STRUCT(ResourceTag, StructType({ 0x4c6a5aad, 0xb445, 0x496c, { 0x87, 0xff, 0x1a, 0xf3, 0x84, 0x5b, 0xe6, 0x53 } }), kStructVersion1)
ResourceTag(Resource* r, BufferType t, ResourceLifecycle l, const Extent* e = nullptr)
	: BaseStructure(ResourceTag::s_structType, kStructVersion1), resource(r), type(t), lifecycle(l)
{
	if (e) extent = *e;
};

//! Resource description
Resource* resource{};
//! Type of the tagged buffer
BufferType type{};
//! The life-cycle for the tag, if resource is volatile a valid command buffer must be specified
ResourceLifecycle lifecycle{};
//! The area of the tagged resource to use (if using the entire resource leave as null)
Extent extent{};

//! IMPORTANT: New members go here or if optional can be chained in a new struct, see sl_struct.h for details
};

//! Depth buffer - IMPORTANT - Must be suitable to use with clipToPrevClip transformation (see Constants below)
constexpr BufferType kBufferTypeDepth = 0;
//! Object and optional camera motion vectors (see Constants below)
constexpr BufferType kBufferTypeMotionVectors = 1;
//! Color buffer with all post-processing effects applied but without any UI/HUD elements
constexpr BufferType kBufferTypeHUDLessColor = 2;
//! Color buffer containing jittered input data for the image scaling pass
constexpr BufferType kBufferTypeScalingInputColor = 3;
//! Color buffer containing results from the image scaling pass
constexpr BufferType kBufferTypeScalingOutputColor = 4;
//! Normals
constexpr BufferType kBufferTypeNormals = 5;
//! Roughness
constexpr BufferType kBufferTypeRoughness = 6;
//! Albedo
constexpr BufferType kBufferTypeAlbedo = 7;
//! Specular Albedo
constexpr BufferType kBufferTypeSpecularAlbedo = 8;
//! Indirect Albedo
constexpr BufferType kBufferTypeIndirectAlbedo = 9;
//! Specular Motion Vectors
constexpr BufferType kBufferTypeSpecularMotionVectors = 10;
//! Disocclusion Mask
constexpr BufferType kBufferTypeDisocclusionMask = 11;
//! Emissive
constexpr BufferType kBufferTypeEmissive = 12;
//! Exposure
constexpr BufferType kBufferTypeExposure = 13;
//! Buffer with normal and roughness in alpha channel
constexpr BufferType kBufferTypeNormalRoughness = 14;
//! Diffuse and camera ray length
constexpr BufferType kBufferTypeDiffuseHitNoisy = 15;
//! Diffuse denoised
constexpr BufferType kBufferTypeDiffuseHitDenoised = 16;
//! Specular and reflected ray length
constexpr BufferType kBufferTypeSpecularHitNoisy = 17;
//! Specular denoised
constexpr BufferType kBufferTypeSpecularHitDenoised = 18;
//! Shadow noisy
constexpr BufferType kBufferTypeShadowNoisy = 19;
//! Shadow denoised
constexpr BufferType kBufferTypeShadowDenoised = 20;
//! AO noisy
constexpr BufferType kBufferTypeAmbientOcclusionNoisy = 21;
//! AO denoised
constexpr BufferType kBufferTypeAmbientOcclusionDenoised = 22;
//! Optional - UI/HUD color and alpha
//! IMPORTANT: Please make sure that alpha channel has enough precision (for example do NOT use formats like R10G10B10A2)
constexpr BufferType kBufferTypeUIColorAndAlpha = 23;
//! Optional - Shadow pixels hint (set to 1 if a pixel belongs to the shadow area, 0 otherwise)
constexpr BufferType kBufferTypeShadowHint = 24;
//! Optional - Reflection pixels hint (set to 1 if a pixel belongs to the reflection area, 0 otherwise)
constexpr BufferType kBufferTypeReflectionHint = 25;
//! Optional - Particle pixels hint (set to 1 if a pixel represents a particle, 0 otherwise)
constexpr BufferType kBufferTypeParticleHint = 26;
//! Optional - Transparency pixels hint (set to 1 if a pixel belongs to the transparent area, 0 otherwise)
constexpr BufferType kBufferTypeTransparencyHint = 27;
//! Optional - Animated texture pixels hint (set to 1 if a pixel belongs to the animated texture area, 0 otherwise)
constexpr BufferType kBufferTypeAnimatedTextureHint = 28;
//! Optional - Bias for current color vs history hint - lerp(history, current, bias) (set to 1 to completely reject history)
constexpr BufferType kBufferTypeBiasCurrentColorHint = 29;
//! Optional - Ray-tracing distance (camera ray length)
constexpr BufferType kBufferTypeRaytracingDistance = 30;
//! Optional - Motion vectors for reflections
constexpr BufferType kBufferTypeReflectionMotionVectors = 31;
//! Optional - Position, in same space as eNormals
constexpr BufferType kBufferTypePosition = 32;
//! Optional - Indicates (via non-zero value) which pixels have motion/depth values that do not match the final color content at that pixel (e.g. overlaid, opaque Picture-in-Picture)
constexpr BufferType kBufferTypeInvalidDepthMotionHint = 33;
//! Alpha
constexpr BufferType kBufferTypeAlpha = 34;
//! Color buffer containing only opaque geometry
constexpr BufferType kBufferTypeOpaqueColor = 35;
//! Optional - Reduce reliance on history instead using current frame hint (0 if a pixel is not at all reactive and default composition should be used, 1 if fully reactive)
constexpr BufferType kBufferTypeReactiveMaskHint = 36;
//! Optional - Pixel lock adjustment hint (set to 1 if pixel lock should be completely removed, 0 otherwise)
constexpr BufferType kBufferTypeTransparencyAndCompositionMaskHint = 37;
//! Optional - Albedo of the reflection ray hit point. For multibounce reflections, this should be the albedo of the first non-specular bounce.
constexpr BufferType kBufferTypeReflectedAlbedo = 38;
//! Optional - Color buffer before particles are drawn.
constexpr BufferType kBufferTypeColorBeforeParticles = 39;
//! Optional - Color buffer before transparent objects are drawn.
constexpr BufferType kBufferTypeColorBeforeTransparency = 40;
//! Optional - Color buffer before fog is drawn.
constexpr BufferType kBufferTypeColorBeforeFog = 41;
//! Optional - Buffer containing the hit distance of a specular ray.
constexpr BufferType kBufferTypeSpecularHitDistance = 42;
//! Optional - Buffer that contains 3 components of a specular ray direction, and 1 component of specular hit distance.
constexpr BufferType kBufferTypeSpecularRayDirectionHitDistance = 43;
//! Optional - Buffer containing normalized direction of a specular ray.
constexpr BufferType kBufferTypeSpecularRayDirection = 44;
// !Optional - Buffer containing the hit distance of a diffuse ray.
constexpr BufferType kBufferTypeDiffuseHitDistance = 45;
//! Optional - Buffer that contains 3 components of a diffuse ray direction, and 1 component of diffuse hit distance.
constexpr BufferType kBufferTypeDiffuseRayDirectionHitDistance = 46;
//! Optional - Buffer containing normalized direction of a diffuse ray.
constexpr BufferType kBufferTypeDiffuseRayDirection = 47;
//! Optional - Buffer containing display resolution depth.
constexpr BufferType kBufferTypeHiResDepth = 48;
//! Required either this or kBufferTypeDepth - Buffer containing linear depth.
constexpr BufferType kBufferTypeLinearDepth = 49;
//! Optional - Bidirectional distortion field. 4 channels in normalized [0,1] pixel space. RG = distorted pixel to undistorted pixel displacement. BA = undistorted pixel to distorted pixel displacement.
constexpr BufferType kBufferTypeBidirectionalDistortionField = 50;
//!Optional - Buffer containing particles or other similar transparent effects rendered into it instead of passing it as part of the input color
constexpr BufferType kBufferTypeTransparencyLayer = 51;
//!Optional - Butffer to be used in addition to TransparencyLayer which allows 3-channels of Opacity versus 1-channel. 
//            In this case, TransparencyLayer represents Color (RcGcBc), TransparencyLayerOpacity represents alpha (RaGaBa)'
constexpr BufferType kBufferTypeTransparencyLayerOpacity = 52;

//! Resource types
enum ResourceTypeV1 : char
{
	eResourceTypeTex2d,
	eResourceTypeBuffer
};

//! Native resource
struct ResourceV1
{
	//! Indicates the type of resource
	ResourceTypeV1 type = eResourceTypeTex2d;
	//! ID3D11Resource/ID3D12Resource/VkBuffer/VkImage
	void* native{};
	//! vkDeviceMemory or nullptr
	void* memory{};
	//! VkImageView/VkBufferView or nullptr
	void* view{};
	//! State as D3D12_RESOURCE_STATES or VkImageLayout
	//! 
	//! IMPORTANT: State needs to be correct when tagged resources are actually used.
	//! 
	uint32_t state{};
	//! Reserved for future expansion, must be set to null
	void* ext{};
};

template<typename T>
T* findStruct(void* ptr)
{
	auto base = static_cast<const BaseStructure*>(ptr);
	while (base && base->structType != T::s_structType)
	{
		base = base->next;
	}
	return (T*)base;
}

struct Version
{
	Version() : major(0), minor(0), build(0) {};
	Version(uint32_t v1, uint32_t v2, uint32_t v3) : major(v1), minor(v2), build(v3) {};

	inline operator bool() const { return major != 0 || minor != 0 || build != 0; }

	inline std::string toStr() const
	{
		return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build);
	}
	inline std::wstring toWStr() const
	{
		return std::to_wstring(major) + L"." + std::to_wstring(minor) + L"." + std::to_wstring(build);
	}
	inline std::wstring toWStrOTAId() const
	{
		return std::to_wstring((major << 16) | (minor << 8) | build);
	}
	inline bool operator==(const Version& rhs) const
	{
		return major == rhs.major && minor == rhs.minor && build == rhs.build;
	}
	inline bool operator>(const Version& rhs) const
	{
		if (major < rhs.major) return false;
		else if (major > rhs.major) return true;
		// major version the same
		if (minor < rhs.minor) return false;
		else if (minor > rhs.minor) return true;
		// minor version the same
		if (build < rhs.build) return false;
		else if (build > rhs.build) return true;
		// build version the same
		return false;
	};
	inline bool operator>=(const Version& rhs) const
	{
		return operator>(rhs) || operator==(rhs);
	};
	inline bool operator<(const Version& rhs) const
	{
		if (major > rhs.major) return false;
		else if (major < rhs.major) return true;
		// major version the same
		if (minor > rhs.minor) return false;
		else if (minor < rhs.minor) return true;
		// minor version the same
		if (build > rhs.build) return false;
		else if (build < rhs.build) return true;
		// build version the same
		return false;
	};
	inline bool operator<=(const Version& rhs) const
	{
		return operator<(rhs) || operator==(rhs);
	};

	uint32_t major;
	uint32_t minor;
	uint32_t build;
};

//! Specifies feature requirement flags
//! 
enum class FeatureRequirementFlags : uint32_t
{
	//! Rendering APIs
	eD3D11Supported = 1 << 0,
	eD3D12Supported = 1 << 1,
	eVulkanSupported = 1 << 2,
	//! If set V-Sync must be disabled when feature is active
	eVSyncOffRequired = 1 << 3,
	//! If set GPU hardware scheduling OS feature must be turned on
	eHardwareSchedulingRequired = 1 << 4
};

//! Specifies feature requirements
//! 
//! {66714097-AC6D-4BC6-8915-1E0F55A6B61F}
SL_STRUCT(FeatureRequirements, StructType({ 0x66714097, 0xac6d, 0x4bc6, { 0x89, 0x15, 0x1e, 0xf, 0x55, 0xa6, 0xb6, 0x1f } }), kStructVersion2)
//! Various Flags
FeatureRequirementFlags flags{};

//! Feature will create this many CPU threads
uint32_t maxNumCPUThreads{};

//! Feature supports only this many viewports
uint32_t maxNumViewports{};

//! Required buffer tags
uint32_t numRequiredTags{};
const BufferType* requiredTags{};

//! OS and Driver versions
Version osVersionDetected{};
Version osVersionRequired{};
Version driverVersionDetected{};
Version driverVersionRequired{};

//! Vulkan specific bits

//! Command queues
uint32_t vkNumComputeQueuesRequired{};
uint32_t vkNumGraphicsQueuesRequired{};

//! Device extensions
uint32_t vkNumDeviceExtensions{};
const char** vkDeviceExtensions{};
//! Instance extensions
uint32_t vkNumInstanceExtensions{};
const char** vkInstanceExtensions{};
//! 1.2 features
//! 
//! NOTE: Use getVkPhysicalDeviceVulkan12Features from sl_helpers_vk.h
uint32_t vkNumFeatures12{};
const char** vkFeatures12{};
//! 1.3 features
//! 
//! NOTE: Use getVkPhysicalDeviceVulkan13Features from sl_helpers_vk.h
uint32_t vkNumFeatures13{};
const char** vkFeatures13{};

//! Vulkan optical flow feature
uint32_t vkNumOpticalFlowQueuesRequired{};

//! IMPORTANT: New members go here or if optional can be chained in a new struct, see sl_struct.h for details
};

enum class EngineType : uint32_t
{
	eCustom,
	eUnreal,
	eUnity,
	eCount
};

enum class RenderAPI : uint32_t
{
	eD3D11,
	eD3D12,
	eVulkan,
	eCount
};

enum class PreferenceFlags : uint64_t
{
	//! Set by default - Disables command list state tracking - Host application is responsible for restoring CL state correctly after each 'slEvaluateFeature' call
	eDisableCLStateTracking = 1 << 0,
	//! Optional - Disables debug text on screen in development builds
	eDisableDebugText = 1 << 1,
	//! Optional - IMPORTANT: Only to be used in the advanced integration mode, see the 'manual hooking' programming guide for more details
	eUseManualHooking = 1 << 2,
	//! Optional - Enables downloading of Over The Air (OTA) updates for SL and NGX
	//! This will invoke the OTA updater to look for new updates. A separate
	//! flag below is used to control whether or not OTA-downloaded SL Plugins are
	//! loaded.
	eAllowOTA = 1 << 3,
	//! Do not check OS version when deciding if feature is supported or not
	//! 
	//! IMPORTANT: ONLY SET THIS FLAG IF YOU KNOW WHAT YOU ARE DOING. 
	//! 
	//! VARIOUS WIN APIs INCLUDING BUT NOT LIMITED TO `IsWindowsXXX`, `GetVersionX`, `rtlGetVersion` ARE KNOWN FOR RETURNING INCORRECT RESULTS.
	eBypassOSVersionCheck = 1 << 4,
	//! Optional - If specified SL will create DXGI factory proxy rather than modifying the v-table for the base interface.
	//! 
	//! This can help with 3rd party overlays which are NOT integrated with the host application but rather operate via injection.
	eUseDXGIFactoryProxy = 1 << 5,
	//! Optional - Enables loading of plugins downloaded Over The Air (OTA), to
	//! be used in conjunction with the eAllowOTA flag.
	eLoadDownloadedPlugins = 1 << 6,
};

//! Log type
enum class LogType
{
	//! Controlled by LogLevel, SL can show more information in eLogLevelVerbose mode
	eInfo,
	//! Always shown regardless of LogLevel
	eWarn,
	//! Always shown regardless of LogLevel
	eError,
	//! Total count
	eCount
};

using PFun_LogMessageCallback = void(LogType type, const char* msg);

//! Different levels for logging
enum class LogLevel : uint32_t
{
	//! No logging
	eOff,
	//! Default logging
	eDefault,
	//! Verbose logging
	eVerbose,
	//! Total count
	eCount
};
using Feature = uint32_t;

constexpr Feature kFeatureDLSS = 0;
constexpr Feature kFeatureNRD = 1;
constexpr Feature kFeatureNIS = 2;
constexpr Feature kFeatureReflex = 3;
constexpr Feature kFeaturePCL = 4;
constexpr Feature kFeatureDeepDVC = 5;
constexpr Feature kFeatureDLSS_G = 1000;
constexpr Feature kFeatureDLSS_RR = 1001;
constexpr Feature kFeatureNvPerf = 1002;
constexpr Feature kFeatureDirectSR = 1003;

SL_STRUCT(Preferences, StructType({ 0x1ca10965, 0xbf8e, 0x432b, { 0x8d, 0xa1, 0x67, 0x16, 0xd8, 0x79, 0xfb, 0x14 } }), kStructVersion1)
//! Optional - In non-production builds it is useful to enable debugging console window
bool showConsole = false;
//! Optional - Various logging levels
LogLevel logLevel = LogLevel::eDefault;
//! Optional - Absolute paths to locations where to look for plugins, first path in the list has the highest priority
const wchar_t** pathsToPlugins{};
//! Optional - Number of paths to search
uint32_t numPathsToPlugins = 0;
//! Optional - Absolute path to location where logs and other data should be stored
//! 
//! NOTE: Set this to nullptr in order to disable logging to a file
const wchar_t* pathToLogsAndData{};
//! Optional - Allows resource allocation tracking on the host side
void* allocateCallback{};
//! Optional - Allows resource deallocation tracking on the host side
void* releaseCallback{};
//! Optional - Allows log message tracking including critical errors if they occur
PFun_LogMessageCallback* logMessageCallback{};
//! Optional - Flags used to enable or disable advanced options
PreferenceFlags flags = PreferenceFlags::eDisableCLStateTracking;
//! Required - Features to load (assuming appropriate plugins are found), if not specified NO features will be loaded by default
const Feature* featuresToLoad{};
//! Required - Number of features to load, only used when list is not a null pointer
uint32_t numFeaturesToLoad{};
//! Optional - Id provided by NVIDIA, if not specified then engine type and version are required
uint32_t applicationId{};
//! Optional - Type of the rendering engine used, if not specified then applicationId is required
EngineType engine = EngineType::eCustom;
//! Optional - Version of the rendering engine used
const char* engineVersion{};
//! Optional - GUID (like for example 'a0f57b54-1daf-4934-90ae-c4035c19df04')
const char* projectId{};
//! Optional - Which rendering API host is planning to use
//! 
//! NOTE: To ensure correct `slGetFeatureRequirements` behavior please specify if planning to use Vulkan.
RenderAPI renderAPI = RenderAPI::eD3D12;

//! IMPORTANT: New members go here or if optional can be chained in a new struct, see sl_struct.h for details
};

enum class DLSSGFlags : uint32_t
{
	eShowOnlyInterpolatedFrame = 1 << 0,
	eDynamicResolutionEnabled = 1 << 1,
	eRequestVRAMEstimate = 1 << 2,
	eRetainResourcesWhenOff = 1 << 3,
	eEnableFullscreenMenuDetection = 1 << 4,
};

enum class DLSSGMode : uint32_t
{
	eOff,
	eOn,
	eAuto,
	eCount
};


// {FAC5F1CB-2DFD-4F36-A1E6-3A9E865256C5}
SL_STRUCT(DLSSGOptions, StructType({ 0xfac5f1cb, 0x2dfd, 0x4f36, { 0xa1, 0xe6, 0x3a, 0x9e, 0x86, 0x52, 0x56, 0xc5 } }), kStructVersion1)
//! Specifies which mode should be used.
DLSSGMode mode = DLSSGMode::eOff;
//! Must be 1
uint32_t numFramesToGenerate = 1;
//! Optional - Flags used to enable or disable certain functionality
DLSSGFlags flags{};
//! Optional - Dynamic resolution optimal width (used only if eDynamicResolutionEnabled is set)
uint32_t dynamicResWidth{};
//! Optional - Dynamic resolution optimal height (used only if eDynamicResolutionEnabled is set)
uint32_t dynamicResHeight{};
//! Optional - Expected number of buffers in the swap-chain
uint32_t numBackBuffers{};
//! Optional - Expected width of the input render targets (depth, motion-vector buffers etc)
uint32_t mvecDepthWidth{};
//! Optional - Expected height of the input render targets (depth, motion-vector buffers etc)
uint32_t mvecDepthHeight{};
//! Optional - Expected width of the back buffers in the swap-chain
uint32_t colorWidth{};
//! Optional - Expected height of the back buffers in the swap-chain
uint32_t colorHeight{};
//! Optional - Indicates native format used for the swap-chain back buffers
uint32_t colorBufferFormat{};
//! Optional - Indicates native format used for eMotionVectors
uint32_t mvecBufferFormat{};
//! Optional - Indicates native format used for eDepth
uint32_t depthBufferFormat{};
//! Optional - Indicates native format used for eHUDLessColor
uint32_t hudLessBufferFormat{};
//! Optional - Indicates native format used for eUIColorAndAlpha
uint32_t uiBufferFormat{};
//! Optional - if specified DLSSG will return any errors which occur when calling underlying API (DXGI or Vulkan)
void* onErrorCallback{};

//! IMPORTANT: New members go here or if optional can be chained in a new struct, see sl_struct.h for details
};

// Define the type of the original function pointer
typedef int (*SLINIT)(Preferences& arg, uint64_t sdkVersion);
typedef int (*SL0ARGS)();
typedef int (*SLFEATINIT)(void* arg, bool& loaded);
typedef int (*SLGETFEATFUNC)(Feature feature, const char* functionName, void*& function);
typedef int (*SL1ARG)(void* arg);
typedef int (*SL2ARGS)(void* arg, void* arg2);
typedef int (*SLGETFEATREQ)(void* arg, FeatureRequirements& requirements);
typedef int (*SL3ARGS)(void* arg, void* arg2, void* arg3);
typedef int (*SL4ARGS)(void* arg, void* arg2, void* arg3, void* arg4);
typedef int (*SL5ARGS)(void* arg, void* arg2, void* arg3, void* arg4, void* arg5);
typedef int (*SLFEATUREARGS)(Feature arg, void* arg2, void* arg3, void* arg4, void* arg5);
typedef int (*DLSSGSETFEATLOADED)(Feature feature, bool value);
typedef int (*SLSETTAG)(void* arg, const ResourceTag* resources, uint32_t numResources, void* arg4);
//typedef int (*SLSETTAGV1)(const Resource* resource, BufferType tag, uint32_t id, const Extent* extent);
typedef int (*SLSETTAGV1)(const ResourceV1* resource, BufferType tag, uint32_t id, void* arg4);
typedef int (*DLSSGSETOPTS)(void* arg, const DLSSGOptions& options);
typedef int (*DEEPDVCSETOPTS)(void* viewport, const DeepDVCOptions& options);
int detoured_slInit(Preferences& pref, uint64_t sdkVersion);
int detoured_slEvaluateFeature(Feature feature, void* arg2, void* arg3, void* arg4, void* arg5);
int detoured_slGetFeatureFunction(Feature feature, const char* functionName, void*& function);
int detoured_slSetFeatureLoaded(Feature feature, bool loaded);
int detoured_slSetTagForCyberpunkFixed(void* arg, const ResourceTag* resources, uint32_t numResources, void* arg4);