// Shared example helper.
#pragma once
#include "CompilationFabric/CompilationFabric.hpp"
#include <cstdio>
#include <filesystem>

namespace cfe {
inline compilationfabric::CompilationRequest cpuReq(const std::string& src, uint64_t id,
    compilationfabric::Datatype dt = compilationfabric::Datatype::F32,
    std::vector<int64_t> shape = {1024}) {
    compilationfabric::CompilationRequest r;
    r.requestId = compilationfabric::CompilationRequestId::fromU64(id);
    r.logicalOperation = compilationfabric::LogicalOperation::fromU64((id % 97) + 1);
    r.source = src; r.sourceDigest = compilationfabric::Sha256::hash(src);
    r.sourceLanguage = "cf-src"; r.datatype = dt; r.rank = 1; r.staticShape = std::move(shape);
    r.targetArchitecture = "host-x86_64"; r.backend = "cpu";
    r.reproducibility = compilationfabric::ReproducibilityMode::Strict;
    return r;
}
inline compilationfabric::CompilationFabricConfig baseConfig(const std::string& tag) {
    std::filesystem::path p = std::filesystem::temp_directory_path() / ("cf_ex_" + tag + "_" + std::to_string(clock()));
    compilationfabric::CompilationFabricConfig cfg;
    cfg.artifactRoot = p.string(); cfg.persistenceEnabled = true; cfg.allowCuda = true;
    return cfg;
}
inline void cleanup(const compilationfabric::CompilationFabricConfig& cfg) { std::error_code ec; std::filesystem::remove_all(cfg.artifactRoot, ec); }
}
