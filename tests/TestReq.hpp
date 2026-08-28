// Shared request builder for tests.
#pragma once
#include "CompilationFabric/CompilationFabric.hpp"
namespace cf_test {
inline compilationfabric::CompilationRequest mkCpuReq(const std::string& src, uint64_t id, compilationfabric::Datatype dt = compilationfabric::Datatype::F32, std::vector<int64_t> shape = {1024}) {
    compilationfabric::CompilationRequest r;
    r.requestId = compilationfabric::CompilationRequestId::fromU64(id);
    r.logicalOperation = compilationfabric::LogicalOperation::fromU64((id % 97) + 1);
    r.source = src; r.sourceDigest = compilationfabric::Sha256::hash(src);
    r.sourceLanguage = "cf-src"; r.datatype = dt; r.rank = 1; r.staticShape = std::move(shape);
    r.targetArchitecture = "host-x86_64"; r.backend = "cpu";
    r.reproducibility = compilationfabric::ReproducibilityMode::Strict;
    return r;
}
}
