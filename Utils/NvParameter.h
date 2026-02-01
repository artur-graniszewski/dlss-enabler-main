#pragma once
#include "../Includes/dlss/nvsdk_ngx_params.h"
#include "../Includes/dlss/nvsdk_ngx_defs.h"
#include <vector>
#include <memory>
#include <map>

/*
enum class NvParameter
{
	Invalid,

	//SuperSampling
	SuperSampling_ScaleFactor,
	SuperSampling_Available,
	SuperSampling_MinDriverVersionMajor,
	SuperSampling_MinDriverVersionMinor,
	SuperSampling_FeatureInitResult,
	SuperSampling_NeedsUpdatedDriver,
	//User settings stuff
	Width,
	Height,
	PerfQualityValue,
	RTXValue,
	FreeMemOnReleaseFeature,
	//Resolution stuff
	OutWidth,
	OutHeight,

	DLSS_Render_Subrect_Dimensions_Width,
	DLSS_Render_Subrect_Dimensions_Height,
	DLSS_Get_Dynamic_Max_Render_Width,
	DLSS_Get_Dynamic_Max_Render_Height,
	DLSS_Get_Dynamic_Min_Render_Width,
	DLSS_Get_Dynamic_Min_Render_Height,
	Sharpness,
	//Callbacks
	DLSSGetStatsCallback,
	DLSSOptimalSettingsCallback,

	//Render stuff
	CreationNodeMask,
	VisibilityNodeMask,
	DLSS_Feature_Create_Flags,
	DLSS_Enable_Output_Subrects,

	//D3D12 Buffers
	Color,
	MotionVectors,
	Depth,
	Output,
	TransparencyMask,
	ExposureTexture,
	DLSS_Input_Bias_Current_Color_Mask,
	Pre_Exposure,
	Exposure_Scale,

	Reset,
	MV_Scale_X,
	MV_Scale_Y,
	Jitter_Offset_X,
	Jitter_Offset_Y,

	//Dev Stuff
	SizeInBytes,
	OptLevel,
	IsDevSnippetBranch
};



NvParameter NvParameterToEnum(const char* name)
{
	static ankerl::unordered_dense::map<std::string, NvParameter> NvParamTranslation = {
		{"SuperSampling.ScaleFactor", NvParameter::SuperSampling_ScaleFactor},
		{"SuperSampling.Available", NvParameter::SuperSampling_Available},
		{"SuperSampling.MinDriverVersionMajor", NvParameter::SuperSampling_MinDriverVersionMajor},
		{"SuperSampling.MinDriverVersionMinor", NvParameter::SuperSampling_MinDriverVersionMinor},
		{"SuperSampling.FeatureInitResult", NvParameter::SuperSampling_FeatureInitResult},
		{"SuperSampling.NeedsUpdatedDriver", NvParameter::SuperSampling_NeedsUpdatedDriver},
		{"#\x01", NvParameter::SuperSampling_Available},

		{"Width", NvParameter::Width},
		{"Height", NvParameter::Height},
		{"PerfQualityValue", NvParameter::PerfQualityValue},
		{"RTXValue", NvParameter::RTXValue},
		{"NVSDK_NGX_Parameter_FreeMemOnReleaseFeature", NvParameter::FreeMemOnReleaseFeature},

		{"OutWidth", NvParameter::OutWidth},
		{"OutHeight", NvParameter::OutHeight},

		{"DLSS.Render.Subrect.Dimensions.Width", NvParameter::DLSS_Render_Subrect_Dimensions_Width},
		{"DLSS.Render.Subrect.Dimensions.Height", NvParameter::DLSS_Render_Subrect_Dimensions_Height},
		{"DLSS.Get.Dynamic.Max.Render.Width", NvParameter::DLSS_Get_Dynamic_Max_Render_Width},
		{"DLSS.Get.Dynamic.Max.Render.Height", NvParameter::DLSS_Get_Dynamic_Max_Render_Height},
		{"DLSS.Get.Dynamic.Min.Render.Width", NvParameter::DLSS_Get_Dynamic_Min_Render_Width},
		{"DLSS.Get.Dynamic.Min.Render.Height", NvParameter::DLSS_Get_Dynamic_Min_Render_Height},
		{"Sharpness", NvParameter::Sharpness},

		{"DLSSOptimalSettingsCallback", NvParameter::DLSSOptimalSettingsCallback},
		{"DLSSGetStatsCallback", NvParameter::DLSSGetStatsCallback},

		{"CreationNodeMask", NvParameter::CreationNodeMask},
		{"VisibilityNodeMask", NvParameter::VisibilityNodeMask},
		{"DLSS.Feature.Create.Flags", NvParameter::DLSS_Feature_Create_Flags},
		{"DLSS.Enable.Output.Subrects", NvParameter::DLSS_Enable_Output_Subrects},

		{"Color", NvParameter::Color},
		{"MotionVectors", NvParameter::MotionVectors},
		{"Depth", NvParameter::Depth},
		{"Output", NvParameter::Output},
		{"TransparencyMask", NvParameter::TransparencyMask},
		{"ExposureTexture", NvParameter::ExposureTexture},
		{"DLSS.Input.Bias.Current.Color.Mask", NvParameter::DLSS_Input_Bias_Current_Color_Mask},

		{"DLSS.Pre.Exposure", NvParameter::Pre_Exposure},
		{"DLSS.Exposure.Scale", NvParameter::Exposure_Scale},

		{"Reset", NvParameter::Reset},
		{"MV.Scale.X", NvParameter::MV_Scale_X},
		{"MV.Scale.Y", NvParameter::MV_Scale_Y},
		{"Jitter.Offset.X", NvParameter::Jitter_Offset_X},
		{"Jitter.Offset.Y", NvParameter::Jitter_Offset_Y},

		{"SizeInBytes", NvParameter::SizeInBytes},
		{"Snippet.OptLevel", NvParameter::OptLevel},
		{"#\x44", NvParameter::OptLevel},
		{"Snippet.IsDevBranch", NvParameter::IsDevSnippetBranch},
		{"#\x45", NvParameter::IsDevSnippetBranch}
	};

	return NvParamTranslation[std::string(name)];
}
*/
enum NvParameterType {
	NvInt,
	NvFloat,
	NvDouble,
	NvUInt,
	NvULL,
	NvD3D11Resource,
	NvD3D12Resource,
	NvVoidPtr
};

struct NvParameter : NVSDK_NGX_Parameter
{
	unsigned int Width{}, Height{}, OutWidth{}, OutHeight{};
	NVSDK_NGX_PerfQuality_Value PerfQualityValue = NVSDK_NGX_PerfQuality_Value_Balanced;
	bool RTXValue{}, FreeMemOnReleaseFeature{};
	int CreationNodeMask{}, VisibilityNodeMask{}, OptLevel{}, IsDevSnippetBranch{};
	float Sharpness = 1.0f;
	bool ResetRender{};
	float MVScaleX = 1.0, MVScaleY = 1.0;
	float JitterOffsetX{}, JitterOffsetY{};

	long long SizeInBytes{};

	bool DepthInverted{}, AutoExposure{}, Hdr{}, EnableSharpening{}, JitterMotion{}, LowRes{};

	//external Resources
	void* InputBiasCurrentColorMask{};
	void* Color{};
	void* Depth{};
	void* MotionVectors{};
	void* Output{};
	void* TransparencyMask{};
	void* ExposureTexture{};

	virtual void Set(const char* InName, unsigned long long InValue) override;
	virtual void Set(const char* InName, float InValue) override;
	virtual void Set(const char* InName, double InValue) override;
	virtual void Set(const char* InName, unsigned int InValue) override;
	virtual void Set(const char* InName, int InValue) override;
	virtual void Set(const char* InName, ID3D11Resource* InValue) override;
	virtual void Set(const char* InName, ID3D12Resource* InValue) override;
	virtual void Set(const char* InName, void* InValue) override;
	virtual NVSDK_NGX_Result Get(const char* InName, unsigned long long* OutValue) const override;
	virtual NVSDK_NGX_Result Get(const char* InName, float* OutValue) const override;
	virtual NVSDK_NGX_Result Get(const char* InName, double* OutValue) const override;
	virtual NVSDK_NGX_Result Get(const char* InName, unsigned int* OutValue) const override;
	virtual NVSDK_NGX_Result Get(const char* InName, int* OutValue) const override;
	virtual NVSDK_NGX_Result Get(const char* InName, ID3D11Resource** OutValue) const override;
	virtual NVSDK_NGX_Result Get(const char* InName, ID3D12Resource** OutValue) const override;
	virtual NVSDK_NGX_Result Get(const char* InName, void** OutValue) const override;
	virtual void Reset() override;

	void Set_Internal(const char* InName, unsigned long long InValue, NvParameterType ParameterType);
	NVSDK_NGX_Result Get_Internal(const char* InName, unsigned long long* OutValue, NvParameterType ParameterType) const;

	void EvaluateRenderScale();

	/**
	template <typename T>
	inline constexpr T& Cast(const auto& Parameter)
	{
		return *((T*)&Parameter);
	}
	**/

	std::vector<std::shared_ptr<NvParameter>> Params;

	__declspec(noinline) NvParameter* AllocateParameters()
	{
		Params.push_back(std::make_shared<NvParameter>());
		return Params.back().get();
	}

	__declspec(noinline) void DeleteParameters(NvParameter* param)
	{
		auto it = std::find_if(Params.begin(), Params.end(),
			[param](const auto& p) { return p.get() == param; });
		Params.erase(it);
	}

	static std::shared_ptr<NvParameter> instance()
	{
		static std::shared_ptr<NvParameter> INSTANCE{ std::make_shared<NvParameter>() };
		return INSTANCE;
	}
};