// Compilation Fabric - Semantic.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Semantic.hpp"

namespace compilationfabric {

#define NAME_CASE(x) case x: return #x
#define LOOKUP(arr, s) for (auto& p : arr) if (s == p.first) return p.second; return std::nullopt

std::string_view datatypeName(Datatype d) {
    switch (d) { NAME_CASE(Datatype::None); NAME_CASE(Datatype::F32); NAME_CASE(Datatype::F64); NAME_CASE(Datatype::F16);
        NAME_CASE(Datatype::BF16); NAME_CASE(Datatype::TF32); NAME_CASE(Datatype::I8); NAME_CASE(Datatype::I16); NAME_CASE(Datatype::I32);
        NAME_CASE(Datatype::I64); NAME_CASE(Datatype::U8); NAME_CASE(Datatype::U16); NAME_CASE(Datatype::U32); NAME_CASE(Datatype::U64);
        NAME_CASE(Datatype::Bool); NAME_CASE(Datatype::INT4); NAME_CASE(Datatype::UINT4); NAME_CASE(Datatype::FP8_E4M3);
        NAME_CASE(Datatype::FP8_E5M2); NAME_CASE(Datatype::F8E5M2); NAME_CASE(Datatype::F8E4M3); }
    return "None";
}
std::optional<Datatype> datatypeFromName(std::string_view s) {
    const std::pair<const char*, Datatype> arr[] = {
        {"None",Datatype::None},{"F32",Datatype::F32},{"F64",Datatype::F64},{"F16",Datatype::F16},{"BF16",Datatype::BF16},
        {"TF32",Datatype::TF32},{"I8",Datatype::I8},{"I16",Datatype::I16},{"I32",Datatype::I32},{"I64",Datatype::I64},
        {"U8",Datatype::U8},{"U16",Datatype::U16},{"U32",Datatype::U32},{"U64",Datatype::U64},{"Bool",Datatype::Bool},
        {"INT4",Datatype::INT4},{"UINT4",Datatype::UINT4},{"FP8_E4M3",Datatype::FP8_E4M3},{"FP8_E5M2",Datatype::FP8_E5M2},
        {"F8E5M2",Datatype::F8E5M2},{"F8E4M3",Datatype::F8E4M3}};
    LOOKUP(arr, s);
}

std::string_view layoutName(Layout l) {
    switch (l) { NAME_CASE(Layout::None); NAME_CASE(Layout::RowMajor); NAME_CASE(Layout::ColMajor); NAME_CASE(Layout::NCHW);
        NAME_CASE(Layout::NHWC); NAME_CASE(Layout::Blocked); NAME_CASE(Layout::Planar); NAME_CASE(Layout::Interleaved); }
    return "None";
}
std::optional<Layout> layoutFromName(std::string_view s) {
    const std::pair<const char*, Layout> arr[] = {
        {"None",Layout::None},{"RowMajor",Layout::RowMajor},{"ColMajor",Layout::ColMajor},{"NCHW",Layout::NCHW},
        {"NHWC",Layout::NHWC},{"Blocked",Layout::Blocked},{"Planar",Layout::Planar},{"Interleaved",Layout::Interleaved}};
    LOOKUP(arr, s);
}

std::string_view quantizationName(QuantizationMode q) {
    switch (q) { NAME_CASE(QuantizationMode::None); NAME_CASE(QuantizationMode::Int8); NAME_CASE(QuantizationMode::Int4);
        NAME_CASE(QuantizationMode::FP8); NAME_CASE(QuantizationMode::DynamicInt); NAME_CASE(QuantizationMode::DynamicFP8); }
    return "None";
}
std::optional<QuantizationMode> quantizationFromName(std::string_view s) {
    const std::pair<const char*, QuantizationMode> arr[] = {
        {"None",QuantizationMode::None},{"Int8",QuantizationMode::Int8},{"Int4",QuantizationMode::Int4},{"FP8",QuantizationMode::FP8},
        {"DynamicInt",QuantizationMode::DynamicInt},{"DynamicFP8",QuantizationMode::DynamicFP8}};
    LOOKUP(arr, s);
}

std::string_view precisionName(PrecisionMode p) {
    switch (p) { NAME_CASE(PrecisionMode::None); NAME_CASE(PrecisionMode::Full); NAME_CASE(PrecisionMode::Reduced);
        NAME_CASE(PrecisionMode::TF32); NAME_CASE(PrecisionMode::FP16); NAME_CASE(PrecisionMode::BF16); NAME_CASE(PrecisionMode::Mixed); }
    return "None";
}
std::optional<PrecisionMode> precisionFromName(std::string_view s) {
    const std::pair<const char*, PrecisionMode> arr[] = {
        {"None",PrecisionMode::None},{"Full",PrecisionMode::Full},{"Reduced",PrecisionMode::Reduced},{"TF32",PrecisionMode::TF32},
        {"FP16",PrecisionMode::FP16},{"BF16",PrecisionMode::BF16},{"Mixed",PrecisionMode::Mixed}};
    LOOKUP(arr, s);
}

std::string_view determinismName(DeterminismMode d) {
    switch (d) { NAME_CASE(DeterminismMode::Unspecified); NAME_CASE(DeterminismMode::Deterministic); NAME_CASE(DeterminismMode::Stochastic); }
    return "Unspecified";
}
std::optional<DeterminismMode> determinismFromName(std::string_view s) {
    const std::pair<const char*, DeterminismMode> arr[] = {
        {"Unspecified",DeterminismMode::Unspecified},{"Deterministic",DeterminismMode::Deterministic},{"Stochastic",DeterminismMode::Stochastic}};
    LOOKUP(arr, s);
}

std::string_view reproducibilityName(ReproducibilityMode r) {
    switch (r) { NAME_CASE(ReproducibilityMode::Unspecified); NAME_CASE(ReproducibilityMode::BestEffort); NAME_CASE(ReproducibilityMode::Strict); }
    return "Unspecified";
}
std::optional<ReproducibilityMode> reproducibilityFromName(std::string_view s) {
    const std::pair<const char*, ReproducibilityMode> arr[] = {
        {"Unspecified",ReproducibilityMode::Unspecified},{"BestEffort",ReproducibilityMode::BestEffort},{"Strict",ReproducibilityMode::Strict}};
    LOOKUP(arr, s);
}

std::string_view debugReleaseName(DebugReleaseMode d) {
    switch (d) { NAME_CASE(DebugReleaseMode::Debug); NAME_CASE(DebugReleaseMode::Release); NAME_CASE(DebugReleaseMode::RelWithDebInfo);
        NAME_CASE(DebugReleaseMode::MinSizeRel); }
    return "Release";
}
std::optional<DebugReleaseMode> debugReleaseFromName(std::string_view s) {
    const std::pair<const char*, DebugReleaseMode> arr[] = {
        {"Debug",DebugReleaseMode::Debug},{"Release",DebugReleaseMode::Release},{"RelWithDebInfo",DebugReleaseMode::RelWithDebInfo},
        {"MinSizeRel",DebugReleaseMode::MinSizeRel}};
    LOOKUP(arr, s);
}

std::string_view vendorName(AcceleratorVendor v) {
    switch (v) { NAME_CASE(AcceleratorVendor::Unknown); NAME_CASE(AcceleratorVendor::Nvidia); NAME_CASE(AcceleratorVendor::AMD);
        NAME_CASE(AcceleratorVendor::Intel); NAME_CASE(AcceleratorVendor::CPU); NAME_CASE(AcceleratorVendor::Custom); }
    return "Unknown";
}
std::optional<AcceleratorVendor> vendorFromName(std::string_view s) {
    const std::pair<const char*, AcceleratorVendor> arr[] = {
        {"Unknown",AcceleratorVendor::Unknown},{"Nvidia",AcceleratorVendor::Nvidia},{"AMD",AcceleratorVendor::AMD},
        {"Intel",AcceleratorVendor::Intel},{"CPU",AcceleratorVendor::CPU},{"Custom",AcceleratorVendor::Custom}};
    LOOKUP(arr, s);
}

std::string_view familyName(AcceleratorFamily f) {
    switch (f) { NAME_CASE(AcceleratorFamily::Unknown); NAME_CASE(AcceleratorFamily::NvidiaCUDA); NAME_CASE(AcceleratorFamily::NvidiaBlackwell);
        NAME_CASE(AcceleratorFamily::NvidiaHopper); NAME_CASE(AcceleratorFamily::NvidiaAmpere); NAME_CASE(AcceleratorFamily::AMD_HIP);
        NAME_CASE(AcceleratorFamily::IntelOneAPI); NAME_CASE(AcceleratorFamily::X86_64); NAME_CASE(AcceleratorFamily::Arm64);
        NAME_CASE(AcceleratorFamily::Generic); }
    return "Unknown";
}
std::optional<AcceleratorFamily> familyFromName(std::string_view s) {
    const std::pair<const char*, AcceleratorFamily> arr[] = {
        {"Unknown",AcceleratorFamily::Unknown},{"NvidiaCUDA",AcceleratorFamily::NvidiaCUDA},{"NvidiaBlackwell",AcceleratorFamily::NvidiaBlackwell},
        {"NvidiaHopper",AcceleratorFamily::NvidiaHopper},{"NvidiaAmpere",AcceleratorFamily::NvidiaAmpere},{"AMD_HIP",AcceleratorFamily::AMD_HIP},
        {"IntelOneAPI",AcceleratorFamily::IntelOneAPI},{"X86_64",AcceleratorFamily::X86_64},{"Arm64",AcceleratorFamily::Arm64},{"Generic",AcceleratorFamily::Generic}};
    LOOKUP(arr, s);
}

} // namespace compilationfabric
