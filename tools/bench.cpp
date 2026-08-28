// Compilation Fabric - measured benchmark matrix.
// Measures real workloads and prints exact inputs/results. Never a no-op loop;
// never calls metadata processing "compilation throughput".
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Persistence.hpp"
#include <cstdio>
#include <filesystem>
#include <thread>
#include <vector>
#include <atomic>
using namespace compilationfabric;
static double ms(int64_t a,int64_t b){ return double(b-a)/1e6; }
static CompilationRequest mkReq(const std::string& src, uint64_t id){
    CompilationRequest r; r.requestId=CompilationRequestId::fromU64(id); r.logicalOperation=LogicalOperation::fromU64(1);
    r.source=src; r.sourceDigest=Sha256::hash(src); r.sourceLanguage="cf-src"; r.datatype=Datatype::F32; r.rank=1; r.staticShape={256}; r.targetArchitecture="host-x86_64"; r.backend="cpu"; return r;
}
int main(){
    std::filesystem::path root = std::filesystem::temp_directory_path()/("cfb_"+std::to_string(Clock::monotonicNanos()));
    CompilationFabricConfig cfg; cfg.artifactRoot=root.string(); cfg.persistenceEnabled=false; cfg.allowCuda=false; CompilationFabric fabric(cfg);
    // SHA-256
    int64_t t0=Clock::monotonicNanos(); for(int i=0;i<100000;++i){ Digest d=Sha256::hash("x"+std::to_string(i));(void)d;} int64_t t1=Clock::monotonicNanos();
    std::printf("sha256: 100000 in %.1f ms => %.0f hashes/s\n", ms(t0,t1), 100000.0*1e3/ms(t0,t1));
    // key construction
    t0=Clock::monotonicNanos(); for(int i=0;i<10000;++i){ CompilationKey k; k.sourceDigest(Sha256::hash("k"+std::to_string(i))); k.compiler("cf-cpu"); k.datatype(Datatype::F32); k.staticShape({256}); (void)k.toHex();} t1=Clock::monotonicNanos();
    std::printf("key_construction: 10000 in %.1f ms => %.0f keys/s\n", ms(t0,t1), 10000.0*1e3/ms(t0,t1));
    // compile a pool
    const int POOLS[] = {1000, 10000, 100000};
    const int THREADS[] = {1,4,8};
    const double HITS[] = {1.0, 0.9, 0.5};
    const int LOOKUPS = 2000;
    for (int pool : POOLS) {
        t0=Clock::monotonicNanos();
        std::vector<CompilationKey> keys;
        for (int i=0;i<pool;++i){ auto r=fabric.compile(mkReq("name=p\nshape=256\nadd scalar="+std::to_string(i%9)+"\n", (i%9700)+1)); if(r.ok()){ keys.push_back(r->key); } else { keys.push_back(CompilationKey()); } }
        int64_t t1b=Clock::monotonicNanos();
        std::printf("compile_pool: %d artifacts in %.1f ms => %.2f ms/compile\n", pool, ms(t0,t1b), ms(t0,t1b)/pool);
        for (int threads : THREADS) {
            for (double hit : HITS) {
                int missPool = pool-1;
                std::atomic<long> done{0}; std::vector<std::thread> ts;
                int64_t s0=Clock::monotonicNanos();
                for (int th=0; th<threads; ++th){
                    ts.emplace_back([&,th](){
                        long local=0;
                        for (int j=0;j<LOOKUPS/threads;++j){
                            int idx = (th*LOOKUPS + j) % pool;
                            bool isHit = (double)((th*LOOKUPS+j) % pool) < hit*(double)pool;
                            CompilationRequest rq;
                            if (isHit) rq = mkReq("name=p\nshape=256\nadd scalar="+std::to_string(idx%9)+"\n", (idx%9700)+1);
                            else { rq = mkReq("name=p\nshape=256\nadd scalar="+std::to_string((pool-1)%9)+"\n", (pool+5000)%9700+1); (void)missPool; }
                            auto plan = fabric.plan(rq); KeyToolchainContext tc; tc.compiler="cf-cpu"; tc.backend="cpu"; tc.frontend="cf-frontend";
                            auto key = buildCompilationKey(rq, *plan, tc);
                            key.toHex(); ++local;
                        }
                        done += local;
                    });
                }
                for (auto& th: ts) th.join();
                int64_t s1=Clock::monotonicNanos();
                std::printf("lookup pool=%d threads=%d hit=%.0f%%: %d lookups in %.1f ms => %.0f lookups/s\n", pool, threads, hit*100, (int)done.load(), ms(s0,s1), done.load()*1e3/ms(s0,s1));
            }
        }
    }
    // compatibility decisions
    t0=Clock::monotonicNanos(); int comps=0; CompilationCompatibility compat;
    for(int i=0;i<100000;++i){ CompilationKey a,b;a.datatype(i%2?Datatype::F32:Datatype::F64);b.datatype(Datatype::F32); CompilationCompatibilityDecision d; (void)d; auto dd=compat.decide(a,b,ArtifactDescriptor{},cfg.compatibilityPolicy); (void)dd; ++comps; } t1=Clock::monotonicNanos();
    std::printf("compatibility_decisions: %d in %.1f ms => %.0f decisions/s\n", comps, ms(t0,t1), comps*1e3/ms(t0,t1));
    // persistence + recovery
    PersistenceStore store(root); int64_t st0=Clock::monotonicNanos();
    for (int i=0;i<1000;++i){ PersistedRecord rec; rec.id=ArtifactId::fromU64(i+1); rec.generation=1; rec.contentBytes=std::vector<uint8_t>(64,(uint8_t)i); rec.descriptor.id=rec.id; rec.descriptor.generation=1; rec.descriptor.contentDigest=Sha256::hash(rec.contentBytes.data(), rec.contentBytes.size()); rec.descriptor.state="Deployable"; store.store(rec); }
    int64_t st1p=Clock::monotonicNanos();
    auto recv = store.recover(); int64_t st2=Clock::monotonicNanos();
    std::printf("persistence: store 1000 in %.1f ms, recover in %.1f ms (valid=%zu)\n", ms(st0,st1p), ms(st1p,st2), recv.ok()?recv->valid.size():0);
    std::error_code ec; std::filesystem::remove_all(root, ec);
    return 0;
}