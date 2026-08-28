// Distributed example: real framed-TCP coordinator + worker + client.
#include "ExampleUtil.hpp"
#include "CompilationFabric/Distributed.hpp"
#include <thread>
#include <chrono>
using namespace compilationfabric;
int main() {
    uint16_t port = 46000 + static_cast<uint16_t>(Clock::monotonicNanos() % 1000);
    std::string root = (std::filesystem::temp_directory_path() / ("cf_coord_" + std::to_string(Clock::monotonicNanos()))).string();
    DistributedCoordinator coord(port, root);
    std::thread coordThread([&]{ coord.run(); });
    coordThread.detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    DistributedWorker w1("127.0.0.1", port, 1, false);
    std::thread wThread([&]{ w1.run(); });
    wThread.detach();
    // Wait deterministically for the worker to register.
    int tryN = 0;
    while (coord.workerCount() == 0 && tryN++ < 40) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (coord.workerCount() == 0) { std::printf("distributed: no worker registered\n"); return 1; }
    DistributedClient client("127.0.0.1", port);
    auto c = client.connect();
    if (!c.ok()) { std::printf("distributed: connect failed %s\n", c.message().c_str()); return 1; }
    auto req = cfe::cpuReq("name=dist\nshape=256\nscale scalar=2.0\n", 200);
    auto r1 = client.submit(req);
    if (!r1.ok()) { return 1; }
    auto r2 = client.submit(req);
    if (!r2.ok()) { std::printf("distributed: resubmit failed %s\n", r2.message().c_str()); return 1; }
    std::printf("distributed: first_ok=%d second_reused=%d workers=%zu\n", (int)r1.ok(), (int)r2->reused, coord.workerCount());
    std::error_code ec; std::filesystem::remove_all(root, ec);
    return (r1.ok() && r2->reused) ? 0 : 1;
}