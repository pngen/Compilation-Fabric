// Compilation Fabric - Semantic vocabulary enums shared by keys and descriptors.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include <string_view>
#include <optional>

namespace compilationfabric {

enum class Datatype : uint8_t {
    None, F32, F64, F16, BF16, TF32, I8, I16, I32, I64, U8, U16, U32, U64,
    Bool, INT4, UINT4, FP8_E4M3, FP8_E5M2, F8E5M2, F8E4M3
};
std::string_view datatypeName(Datatype d);
std::optional<Datatype> datatypeFromName(std::string_view s);

enum class Layout : uint8_t { None, RowMajor, ColMajor, NCHW, NHWC, Blocked, Planar, Interleaved };
std::string_view layoutName(Layout l);
std::optional<Layout> layoutFromName(std::string_view s);

enum class QuantizationMode : uint8_t { None, Int8, Int4, FP8, DynamicInt, DynamicFP8 };
std::string_view quantizationName(QuantizationMode q);
std::optional<QuantizationMode> quantizationFromName(std::string_view s);

enum class PrecisionMode : uint8_t { None, Full, Reduced, TF32, FP16, BF16, Mixed };
std::string_view precisionName(PrecisionMode p);
std::optional<PrecisionMode> precisionFromName(std::string_view s);

enum class DeterminismMode : uint8_t { Unspecified, Deterministic, Stochastic };
std::string_view determinismName(DeterminismMode d);
std::optional<DeterminismMode> determinismFromName(std::string_view s);

enum class ReproducibilityMode : uint8_t { Unspecified, BestEffort, Strict };
std::string_view reproducibilityName(ReproducibilityMode r);
std::optional<ReproducibilityMode> reproducibilityFromName(std::string_view s);

enum class DebugReleaseMode : uint8_t { Debug, Release, RelWithDebInfo, MinSizeRel };
std::string_view debugReleaseName(DebugReleaseMode d);
std::optional<DebugReleaseMode> debugReleaseFromName(std::string_view s);

enum class AcceleratorVendor : uint8_t { Unknown, Nvidia, AMD, Intel, CPU, Custom };
std::string_view vendorName(AcceleratorVendor v);
std::optional<AcceleratorVendor> vendorFromName(std::string_view s);

enum class AcceleratorFamily : uint8_t { Unknown, NvidiaCUDA, NvidiaBlackwell, NvidiaHopper, NvidiaAmpere, AMD_HIP, IntelOneAPI, X86_64, Arm64, Generic };
std::string_view familyName(AcceleratorFamily f);
std::optional<AcceleratorFamily> familyFromName(std::string_view s);

// Shape: vector<int64>; static shape vs symbolic constraints.
// Symbolic shape: a string describing bound constraints (e.g. "N<=2048:dim0").

} // namespace compilationfabric
