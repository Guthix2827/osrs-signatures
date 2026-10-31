// Exercise the internal service without network requests or clock delays.
#define OSRS_SIGNATURE_TEST
#include "../src/main.cpp"
#include <cassert>
#include <atomic>

Json fixtureStats() {
    Json stats = {{"name", "Zezima"}, {"fetchedAt", 0}, {"totalLevel", 2173}, {"overallRank", nullptr}, {"skills", Json::object()}};
    for (const char* name : signature::skills) {
        stats["skills"][toLowercase(name)] = {{"level", 99}, {"xp", 13034431}, {"rank", 1}};
    }
    return stats;
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    if (argc == 3) {
        // The integration test controls a fixture/clock; production has no such route.
        SignatureService service(
            [&](const std::string&) { return Json::parse(readFile(argv[1])); },
            [&] { return std::stoll(readFile(argv[2])); }
        );
        crow::SimpleApp application;
        registerRoutes(application, service);
        application.bindaddr("127.0.0.1").port(18081).concurrency(8).run();
        return 0;
    }
    std::atomic<Timestamp> now = 100;
    std::atomic<int> lookups = 0;
    std::atomic<int> renders = 0;
    std::atomic<bool> fail = false;
    Json stats = fixtureStats();
    const Json config = {{"name", "Zezima"}, {"design", "classic"}};
    auto loader = [&](const std::string&) {
        ++lookups;
        if (fail) throw ApiError(502, "Fixture upstream failed.");
        return stats;
    };
    auto renderer = [&](const Json&, const Json&) {
        const auto version = std::to_string(++renders);
        return RenderedImage{"png-" + version, "\"etag-" + version + "\""};
    };
    {
        SignatureService service(loader, [&] { return now.load(); }, renderer);
        const auto first = service.getPngImage(config);
        assert(first.bytes == "png-1" && lookups == 1 && renders == 1);
        now = 159;
        assert(service.getPngImage(config).etag == first.etag);
        assert(lookups == 1 && renders == 1);
        now = 160;
        stats["skills"]["attack"]["rank"] = 999;
        assert(service.getPngImage(config).etag == first.etag);
        assert(lookups == 2 && renders == 1);
        now = 220;
        stats["skills"]["attack"]["level"] = 75;
        assert(service.getPngImage(config).etag != first.etag);
        assert(lookups == 3 && renders == 2);
        // Other designs/backgrounds still share the per-player upstream cache.
        service.getPngImage({{"name", "guthix"}, {"background", "red"}});
        assert(lookups == 3);
        now = 280;
        fail = true;
        for (int i = 0; i < 2; ++i) {
            try { service.getPngImage(config); assert(false); }
            catch (const ApiError& error) { assert(error.statusCode == 502); }
        }
        assert(lookups == 4);
        fail = false;
    }
    // Recreating the service does not need an index or a previous generated file.
    {
        SignatureService restarted(loader, [&] { return now.load(); }, renderer);
        assert(!restarted.getPngImage(config).bytes.empty());
        assert(lookups == 5);
    }
    std::cout << "PASS: memory cache, one-minute throttling, unchanged artwork, errors, shared stats and restart regeneration\n";
}
