// Compilation Fabric CLI. Real commands backed by the library.
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Distributed.hpp"
#include "CompilationFabric/Toolchain.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

using namespace compilationfabric;

static void printUsage() {
    std::printf("Compilation Fabric 1.0.0\n"
                "Usage: compile_fabric <command> [args]\n"
                "Commands:\n"
                "  plan <request.json>              Plan a compilation\n"
                "  compile <request.json>           Compile (or reuse) + validate\n"
                "  submit <request.json>            Alias for compile\n"
                "  lookup <request.json>            Cache lookup + compatibility decision\n"
                "  invalidate <request.json>        Invalidate by key\n"
                "  supersede <request.json> <gen>   Supersede an artifact\n"
                "  deploy <artifact-hex>            Deploy/load an artifact\n"
                "  artifacts                        List artifacts\n"
                "  toolchains                       Show discovered toolchains\n"
                "  targets                          Show discovered targets\n"
                "  stats                            Show runtime stats\n"
                "  snapshot                         Show a full snapshot\n"
                "  explain <request.json>           Explain a compilation key\n"
                "  recover                          Recover persisted state\n"
                "  bench <n> [seed]                 Run a quick benchmark\n"
                "  serve <port>                     Run the distributed coordinator\n"
                "  worker <host> <port> [cuda]      Run a distributed worker\n");
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream s; s << f.rdbuf(); return s.str();
}
static CompilationRequest loadRequest(const std::string& path) {
    CompilationRequest r;
    std::string txt = readFile(path);
    auto j = Json::parse(txt);
    if (j) { if (auto v = CompilationRequest::fromJson(*j)) r = *v; }
    return r;
}

static std::string cliRoot() {
    std::filesystem::path p = std::filesystem::temp_directory_path() / ("cf_cli_" + std::to_string(Clock::monotonicNanos()));
    std::error_code ec;
    std::filesystem::remove_all(p.parent_path(), ec);
    return p.string();
}

static CompilationFabric makeFabric(const std::string& root) {
    CompilationFabricConfig cfg; cfg.artifactRoot = root.empty() ? "cf-artifacts" : root;
    cfg.persistenceEnabled = true; cfg.allowCuda = true;
    return CompilationFabric(cfg);
}

int main(int argc, char** argv) {
    if (argc < 2) { printUsage(); return 0; }
    std::string cmd = argv[1];
    try {
        if (cmd == "help" || cmd == "--help") { printUsage(); return 0; }
        if (cmd == "serve") {
            uint16_t port = argc > 2 ? static_cast<uint16_t>(std::atoi(argv[2])) : 4000;
            std::string root = "cf-coordinator";
            DistributedCoordinator coord(port, root);
            auto r = coord.run();
            return r.ok() ? 0 : 1;
        }
        if (cmd == "worker") {
            if (argc < 4) { std::printf("usage: worker <host> <port> [cuda]\n"); return 1; }
            std::string host = argv[2];
            uint16_t port = static_cast<uint16_t>(std::atoi(argv[3]));
            WorkerId id = (argc > 4 && std::string(argv[4]) == "cuda") ? 10 : static_cast<WorkerId>(Clock::monotonicNanos() % 1000);
            bool cuda = argc > 4 && std::string(argv[4]) == "cuda";
            DistributedWorker w(host, port, id, cuda);
            return w.run();
        }
        if (cmd == "bench") {
            int n = argc > 2 ? std::atoi(argv[2]) : 1000;
            uint64_t seed = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1234;
            auto fabric = makeFabric("cf-bench");
            auto t0 = Clock::monotonicNanos();
            for (int i = 0; i < n; ++i) {
                CompilationRequest r;
                r.requestId = CompilationRequestId::fromU64(i + 1);
                r.logicalOperation = LogicalOperation::fromU64(1);
                r.source = "name=k\nshape=256\nadd scalar=" + std::to_string(i % 7) + "\n";
                r.sourceDigest = Sha256::hash(r.source);
                r.datatype = Datatype::F32; r.rank = 1; r.staticShape = {256}; r.backend = "cpu";
                r.autotuneSeed = seed;
                fabric.compile(r);
            }
            auto t1 = Clock::monotonicNanos();
            double ms = double(t1 - t0) / 1e6;
            std::printf("bench: %d compiles in %.2f ms (%.2f us/op)\n", n, ms, ms * 1000.0 / n);
            return 0;
        }
        // Commands that do not need a request file.
        {
            std::string root = cliRoot();
            auto fabric = makeFabric(root);
            if (cmd == "deploy") { if (argc < 3) { std::printf("deploy <artifact-hex>\n"); return 1; } if (auto id = ArtifactId::parse(argv[2])) { auto mm = fabric.deploy(*id); if (mm.ok()) std::printf("deployed %s\n", id->toHex().c_str()); else { std::fprintf(stderr, "deploy failed: %s\n", mm.message().c_str()); return 1; } } return 0; }
            if (cmd == "toolchains") { auto tt = fabric.toolchains(); if (tt.ok()) { auto a = CudaBackend::api(); std::fprintf(stderr, "[cuda] avail=%d err=%s nvrtc=%s\n", (int)a->available(), a->error().c_str(), a->nvrtcVersion().c_str()); std::printf("%s\n", tt->toJson().dump().c_str()); } return 0; }
            if (cmd == "targets") { auto tt = fabric.targets(); if (tt.ok()) { for (auto& x : *tt) std::printf("%s %s cc=%s\n", x.deviceName.c_str(), x.architecture.c_str(), x.computeCapability.c_str()); } return 0; }
            if (cmd == "stats") { std::printf("%s\n", fabric.stats().dump().c_str()); return 0; }
            if (cmd == "snapshot") { auto ss = fabric.snapshot(); std::printf("%s\n", ss.toJson().c_str()); return 0; }
            if (cmd == "recover") { auto rr = fabric.recover(); std::printf("recover: %s\n", rr.ok() ? "ok" : rr.message().c_str()); return 0; }
            if (cmd == "artifacts") { auto ss = fabric.snapshot(); std::printf("%s\n", ss.artifactIndex.dump().c_str()); return 0; }
        }
        // Commands needing a request file.
        if (argc < 3) { std::printf("missing request.json argument for '%s'\n", cmd.c_str()); return 1; }
        std::string root = cliRoot();
        auto fabric = makeFabric(root);
        CompilationRequest req = loadRequest(argv[2]);
        if (cmd == "plan") { auto p = fabric.plan(req); if (p.ok()) std::printf("%s\n", p->toJson().dump().c_str()); else { std::fprintf(stderr, "plan failed: %s\n", p.message().c_str()); return 1; } }
        else if (cmd == "compile" || cmd == "submit") { auto r = fabric.compile(req); if (r.ok()) std::printf("%s\n", r->toJson().dump().c_str()); else { std::fprintf(stderr, "compile failed: %s\n", r.message().c_str()); return 1; } }
        else if (cmd == "lookup") { auto plan = fabric.plan(req); KeyToolchainContext tc; tc.compiler="cf-cpu"; tc.backend="cpu"; auto key = buildCompilationKey(req, *plan, tc); auto d = fabric.lookup(key); if (d.ok()) std::printf("outcome=%s reusable=%d\n", std::string(compatibilityOutcomeName(d->first.outcome)).c_str(), (int)d->first.reusable); else return 1; }
        else if (cmd == "invalidate") { auto plan = fabric.plan(req); KeyToolchainContext tc; tc.compiler="cf-cpu"; tc.backend="cpu"; auto key = buildCompilationKey(req, *plan, tc); auto r = fabric.invalidateByKey(key); if (r.ok()) std::printf("invalidated\n"); else return 1; }
        else if (cmd == "explain") { auto plan = fabric.plan(req); KeyToolchainContext tc; tc.compiler="cf-cpu"; tc.backend="cpu"; auto key = buildCompilationKey(req, *plan, tc); std::printf("%s\n", fabric.explain(key).dump().c_str()); }
        else { printUsage(); }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}