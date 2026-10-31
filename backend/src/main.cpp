#include "domain.hpp"

#include <crow.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <array>
#include <algorithm>
#include <functional>
#include <fcntl.h>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using Json = nlohmann::json;
using Timestamp = std::int64_t;
namespace filesystem = std::filesystem;

constexpr std::size_t maximumHttpResponseBytes = 1024 * 1024;
constexpr std::size_t maximumQueuedJobs = 32;
constexpr std::size_t maximumRetainedJobs = 1000;
constexpr std::size_t maximumCachedPlayers = 1000;
constexpr std::size_t maximumLiveSignatures = 256;

class ApiError : public std::runtime_error {
public:
    ApiError(int statusCode, const std::string& message)
        : std::runtime_error(message), statusCode(statusCode) {
    }

    int statusCode;
};

std::string environmentValue(const char* variableName, const char* fallbackValue) {
    const char* value = std::getenv(variableName);
    if (value == nullptr) {
        return fallbackValue;
    }
    return value;
}


Timestamp currentTimestamp() {
    const auto elapsedTime = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count();
}

std::string readFile(const filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);

    if (!file) {
        throw std::runtime_error("Cannot read " + filePath.string());
    }

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

void writeFileAtomically(const filesystem::path& filePath, const std::string& contents) {
    filesystem::path temporaryPath = filePath;
    temporaryPath += ".tmp";

    {
        std::ofstream file(temporaryPath, std::ios::binary);
        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));

        if (!file) {
            throw std::runtime_error("Cannot write file.");
        }
    }

    filesystem::rename(temporaryPath, filePath);
}

std::string toLowercase(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))
        );
    }

    return value;
}

std::string generateIdentifier() {
    std::array<unsigned char, 16> randomBytes{};
    std::ifstream randomSource("/dev/urandom", std::ios::binary);
    randomSource.read(
        reinterpret_cast<char*>(randomBytes.data()),
        static_cast<std::streamsize>(randomBytes.size())
    );

    if (!randomSource) {
        throw std::runtime_error("Random source unavailable.");
    }

    std::ostringstream identifier;
    identifier << std::hex << std::setfill('0');

    for (unsigned char byte : randomBytes) {
        identifier << std::setw(2) << static_cast<unsigned int>(byte);
    }

    return identifier.str();
}

std::string encodeUrlParameter(const std::string& value) {
    std::ostringstream encodedValue;

    for (unsigned char character : value) {
        const bool canUseDirectly = std::isalnum(character)
            || character == '-'
            || character == '_'
            || character == '.';

        if (canUseDirectly) {
            encodedValue << character;
            continue;
        }

        encodedValue << '%'
                     << std::uppercase << std::hex
                     << std::setw(2) << std::setfill('0')
                     << static_cast<unsigned int>(character)
                     << std::nouppercase << std::dec;
    }

    return encodedValue.str();
}

struct HttpResponseBuffer {
    std::string contents;
    bool exceededSizeLimit = false;
};

std::size_t appendHttpResponse(
    char* contents,
    std::size_t elementSize,
    std::size_t elementCount,
    void* userData
) {
    auto& response = *static_cast<HttpResponseBuffer*>(userData);
    const std::size_t incomingBytes = elementSize * elementCount;
    const std::size_t availableBytes = maximumHttpResponseBytes - response.contents.size();

    if (incomingBytes > availableBytes) {
        response.exceededSizeLimit = true;
        return 0;
    }

    response.contents.append(contents, incomingBytes);
    return incomingBytes;
}

std::string fetchHttp(const std::string& url) {
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> client(
        curl_easy_init(),
        curl_easy_cleanup
    );

    if (!client) {
        throw ApiError(502, "HTTP client unavailable.");
    }

    HttpResponseBuffer response;
    curl_easy_setopt(client.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(client.get(), CURLOPT_USERAGENT,
        "osrs-signatures/0.1 (public hiscores and signature generator)");
    curl_easy_setopt(client.get(), CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(client.get(), CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(client.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(client.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(client.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(client.get(), CURLOPT_WRITEFUNCTION, appendHttpResponse);
    curl_easy_setopt(client.get(), CURLOPT_WRITEDATA, &response);

    const CURLcode result = curl_easy_perform(client.get());
    long statusCode = 0;
    curl_easy_getinfo(client.get(), CURLINFO_RESPONSE_CODE, &statusCode);

    if (statusCode == 404) {
        throw ApiError(404, "Player not found in hiscores.");
    }

    if (result != CURLE_OK || statusCode != 200) {
        if (response.exceededSizeLimit) {
            throw ApiError(502, "Upstream response too large.");
        }

        throw ApiError(502, "Hiscores/assets unavailable; try again later.");
    }

    return response.contents;
}

void runImageMagick(const std::vector<std::string>& imageArguments) {
    std::vector<std::string> arguments = {
        "convert",
        "-limit", "memory", "64MiB",
        "-limit", "map", "128MiB",
        "-limit", "disk", "128MiB",
        "-limit", "thread", "1",
        "-limit", "time", "20"
    };
    arguments.insert(arguments.end(), imageArguments.begin(), imageArguments.end());

    std::vector<char*> processArguments;
    for (std::string& argument : arguments) {
        processArguments.push_back(argument.data());
    }
    processArguments.push_back(nullptr);

    const pid_t processId = fork();

    if (processId < 0) {
        throw std::runtime_error("Cannot start image renderer.");
    }

    if (processId == 0) {
        // All argument storage was prepared before fork. Never invoke a shell.
        execvp(processArguments[0], processArguments.data());
        _exit(127);
    }

    int exitStatus = 0;
    while (waitpid(processId, &exitStatus, 0) < 0) {
        if (errno != EINTR) {
            throw std::runtime_error("Renderer wait failed.");
        }
    }

    if (!WIFEXITED(exitStatus) || WEXITSTATUS(exitStatus) != 0) {
        throw std::runtime_error(
            "Image renderer failed; check ImageMagick, fonts, and assets."
        );
    }
}

void validateSkillEntry(const Json& skillEntry) {
    if (!skillEntry.is_object()
        || !skillEntry.contains("name")
        || !skillEntry["name"].is_string()) {
        throw ApiError(502, "Invalid skill entry.");
    }

    for (const char* fieldName : {"rank", "level", "xp"}) {
        if (!skillEntry.contains(fieldName) || !skillEntry[fieldName].is_number_integer()) {
            throw ApiError(502, "Invalid skill numbers.");
        }
    }
}

Json normalizeSkillEntry(const Json& skillEntry) {
    const std::int64_t level = skillEntry["level"];
    const std::int64_t experience = skillEntry["xp"];
    const std::int64_t rank = skillEntry["rank"];

    if (level < 1 || level > 99 || experience < 0) {
        return {{"level", nullptr}, {"xp", nullptr}, {"rank", nullptr}};
    }

    Json normalizedRank = nullptr;
    if (rank >= 0) {
        normalizedRank = rank;
    }

    return {{"level", level}, {"xp", experience}, {"rank", normalizedRank}};
}

Json normalizePlayerStats(const Json& rawStats, const std::string& requestedName) {
    if (!rawStats.is_object()
        || !rawStats.contains("skills")
        || !rawStats["skills"].is_array()) {
        throw ApiError(502, "Invalid hiscores response.");
    }

    Json normalizedStats = {
        {"name", rawStats.value("name", requestedName)},
        {"fetchedAt", currentTimestamp()},
        {"skills", Json::object()}
    };
    std::map<std::string, Json> skillsByName;

    for (const Json& skillEntry : rawStats["skills"]) {
        validateSkillEntry(skillEntry);
        const std::string skillName = toLowercase(skillEntry["name"].get<std::string>());

        if (skillsByName.contains(skillName)) {
            throw ApiError(502, "Duplicate upstream skill.");
        }

        skillsByName[skillName] = skillEntry;
    }

    if (!skillsByName.contains("overall")) {
        throw ApiError(502, "Missing overall stats.");
    }

    const Json& overallStats = skillsByName.at("overall");
    if (overallStats["level"].get<std::int64_t>() < 1
        || overallStats["xp"].get<std::int64_t>() < 0) {
        throw ApiError(502, "Unavailable overall stats.");
    }

    normalizedStats["totalLevel"] = overallStats["level"];
    normalizedStats["overallRank"] = nullptr;
    if (overallStats["rank"].get<std::int64_t>() >= 0) {
        normalizedStats["overallRank"] = overallStats["rank"];
    }

    for (const char* skillName : signature::skills) {
        const std::string skillKey = toLowercase(skillName);
        if (!skillsByName.contains(skillKey)) {
            throw ApiError(502, "Missing skill: " + skillKey);
        }

        normalizedStats["skills"][skillKey] = normalizeSkillEntry(skillsByName.at(skillKey));
    }

    return normalizedStats;
}

std::optional<int> getCombatSkillLevel(const Json& playerStats, const char* skillName) {
    const Json& level = playerStats["skills"][skillName]["level"];
    if (level.is_null()) {
        return std::nullopt;
    }

    return level.get<int>();
}

std::optional<int> calculatePlayerCombat(const Json& playerStats) {
    const auto attack = getCombatSkillLevel(playerStats, "attack");
    const auto strength = getCombatSkillLevel(playerStats, "strength");
    const auto defence = getCombatSkillLevel(playerStats, "defence");
    const auto hitpoints = getCombatSkillLevel(playerStats, "hitpoints");
    const auto ranged = getCombatSkillLevel(playerStats, "ranged");
    const auto prayer = getCombatSkillLevel(playerStats, "prayer");
    const auto magic = getCombatSkillLevel(playerStats, "magic");

    if (!attack || !strength || !defence || !hitpoints
        || !ranged || !prayer || !magic) {
        return std::nullopt;
    }

    return signature::combat(
        *attack, *strength, *defence, *hitpoints, *ranged, *prayer, *magic
    );
}

void downloadMissingIcon(const filesystem::path& assetsDirectory, const std::string& skillName) {
    const filesystem::path pngPath = assetsDirectory / "icons" / (skillName + ".png");
    if (filesystem::exists(pngPath)) {
        return;
    }

    const filesystem::path gifPath = assetsDirectory / "icons" / (skillName + ".gif");
    const std::string iconUrl = "https://www.runescape.com/img/rsp777/hiscores/skill_icon_"
        + skillName + "1.gif";

    writeFileAtomically(gifPath, fetchHttp(iconUrl));
    runImageMagick({gifPath.string(), "PNG32:" + pngPath.string()});
    filesystem::remove(gifPath);
}

std::string imageOffset(int x, int y) {
    return "+" + std::to_string(x) + "+" + std::to_string(y);
}

void prepareAssets(const filesystem::path& assetsDirectory) {
    filesystem::create_directories(assetsDirectory / "icons");

    Json manifest = {
        {"width", 192},
        {"height", 128},
        {"cellSize", 32},
        {"icons", Json::object()}
    };
    std::vector<std::string> arguments = {"-size", "192x128", "xc:none"};

    for (std::size_t index = 0; index < signature::skills.size(); ++index) {
        const std::string skillName = toLowercase(signature::skills[index]);
        downloadMissingIcon(assetsDirectory, skillName);

        const filesystem::path iconPath = assetsDirectory / "icons" / (skillName + ".png");
        int iconSize = 16;
        if (skillName == "ranged") {
            iconSize = 18;
        }

        const int atlasX = static_cast<int>(index % 6) * 32;
        const int atlasY = static_cast<int>(index / 6) * 32;
        manifest["icons"][skillName] = {
            {"x", atlasX}, {"y", atlasY}, {"width", iconSize}, {"height", iconSize}
        };
        arguments.insert(arguments.end(), {
            iconPath.string(), "-geometry", imageOffset(atlasX, atlasY), "-composite"
        });
    }

    const filesystem::path temporaryAtlasPath = assetsDirectory / "skills.next.png";
    arguments.push_back("PNG32:" + temporaryAtlasPath.string());
    runImageMagick(arguments);

    filesystem::rename(temporaryAtlasPath, assetsDirectory / "skills.png");
    writeFileAtomically(assetsDirectory / "skills.json", manifest.dump(2));
}

filesystem::path rendererBundleDirectory() {
    return filesystem::absolute(environmentValue("SIGNATURE_RENDER_BUNDLE", "../frontend/dist-render"));
}

const Json& rendererManifest() {
    // Restart the backend after publishing a new render bundle.
    static const Json manifest = Json::parse(readFile(rendererBundleDirectory() / "designs.json"));
    return manifest;
}

struct RenderedImage {
    std::string bytes;
    std::string etag;
};

std::string runBrowserRenderer(std::vector<std::string> arguments, const std::string& input) {
    int inputPipe[2];
    int outputPipe[2];
    if (pipe(inputPipe) < 0) throw std::runtime_error("Cannot create renderer input pipe.");
    if (pipe(outputPipe) < 0) {
        close(inputPipe[0]); close(inputPipe[1]);
        throw std::runtime_error("Cannot create renderer output pipe.");
    }
    std::vector<char*> processArguments;
    for (auto& argument : arguments) processArguments.push_back(argument.data());
    processArguments.push_back(nullptr);
    const pid_t processId = fork();
    if (processId < 0) {
        for (int fd : {inputPipe[0], inputPipe[1], outputPipe[0], outputPipe[1]}) close(fd);
        throw std::runtime_error("Cannot start signature renderer.");
    }
    if (processId == 0) {
        setpgid(0, 0);
        dup2(inputPipe[0], STDIN_FILENO);
        dup2(outputPipe[1], STDOUT_FILENO);
        for (int fd : {inputPipe[0], inputPipe[1], outputPipe[0], outputPipe[1]}) close(fd);
        execvp(processArguments[0], processArguments.data());
        _exit(127);
    }
    setpgid(processId, processId);
    close(inputPipe[0]); close(outputPipe[1]);
    fcntl(inputPipe[1], F_SETFL, O_NONBLOCK);
    fcntl(outputPipe[0], F_SETFL, O_NONBLOCK);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    std::size_t written = 0;
    std::string output;
    int exitStatus = 0;
    bool exited = false;
    try {
        while (!exited) {
            if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Signature renderer timed out.");
            if (inputPipe[1] >= 0) {
                const ssize_t count = write(inputPipe[1], input.data() + written, input.size() - written);
                if (count > 0) written += static_cast<std::size_t>(count);
                else if (count < 0 && errno != EAGAIN && errno != EINTR) throw std::runtime_error("Renderer input failed.");
                if (written == input.size()) { close(inputPipe[1]); inputPipe[1] = -1; }
            }
            char buffer[8192];
            while (true) {
                const ssize_t count = read(outputPipe[0], buffer, sizeof(buffer));
                if (count > 0) {
                    output.append(buffer, static_cast<std::size_t>(count));
                    if (output.size() > 256 * 1024 + 65) throw std::runtime_error("Rendered image exceeds memory limit.");
                } else {
                    if (count < 0 && errno != EAGAIN && errno != EINTR) throw std::runtime_error("Renderer output failed.");
                    break;
                }
            }
            const pid_t result = waitpid(processId, &exitStatus, WNOHANG);
            if (result < 0 && errno != EINTR) throw std::runtime_error("Renderer wait failed.");
            if (result == processId) {
                // Drain any last bytes after the process has closed stdout.
                while (true) {
                    const ssize_t count = read(outputPipe[0], buffer, sizeof(buffer));
                    if (count <= 0) break;
                    output.append(buffer, static_cast<std::size_t>(count));
                    if (output.size() > 256 * 1024 + 65) throw std::runtime_error("Rendered image exceeds memory limit.");
                }
                exited = true;
            } else std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!WIFEXITED(exitStatus) || WEXITSTATUS(exitStatus) != 0) throw std::runtime_error("Signature renderer failed; check Node, Playwright and the render bundle.");
    } catch (...) {
        if (!exited) {
            kill(-processId, SIGKILL);
            while (waitpid(processId, &exitStatus, 0) < 0 && errno == EINTR) {}
        }
        if (inputPipe[1] >= 0) close(inputPipe[1]);
        close(outputPipe[0]);
        throw;
    }
    if (inputPipe[1] >= 0) close(inputPipe[1]);
    close(outputPipe[0]);
    return output;
}

RenderedImage renderSignatureImage(const Json& playerStats, const Json& configuration) {
    Json payload = {{"config", configuration}, {"player", playerStats}, {"combatLevel", nullptr}};
    if (const auto combatLevel = calculatePlayerCombat(playerStats)) payload["combatLevel"] = *combatLevel;
    const std::string output = runBrowserRenderer({
        environmentValue("NODE_BINARY", "node"),
        filesystem::absolute(environmentValue("SIGNATURE_RENDERER_SCRIPT", "../renderer/render.mjs")).string(),
        "--bundle", rendererBundleDirectory().string()
    }, payload.dump());
    if (output.size() < 89 || output[64] != '\n'
        || output.substr(65, 8) != std::string("\x89PNG\r\n\x1a\n", 8)
        || output.substr(81, 8) != std::string("\0\0\1\x40\0\0\0\x7d", 8)) {
        throw std::runtime_error("Invalid signature renderer output.");
    }
    for (int i = 0; i < 64; ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(output[i]))) throw std::runtime_error("Invalid image hash.");
    }
    return {output.substr(65), "\"" + output.substr(0, 64) + "\""};
}

bool isPendingJob(const Json& job) {
    return job["status"] == "queued" || job["status"] == "running";
}

Json validateSignatureConfiguration(const Json& configuration) {
    if (!configuration.is_object()
        || !configuration.contains("name")
        || !configuration["name"].is_string()) {
        throw ApiError(400, "name is required.");
    }

    const std::string playerName = signature::normalizeName(configuration["name"]);
    const std::string background = configuration.value("background", std::string("blue"));
    const std::string pose = configuration.value("pose", std::string("none"));
    const std::string design = configuration.value("design", std::string("classic"));
    bool publishedDesign = false;
    for (const Json& entry : rendererManifest().at("designs")) {
        if (entry.at("id") == design) publishedDesign = true;
    }
    if (!publishedDesign) {
        throw ApiError(400, "Unknown or unpublished design.");
    }

    if (background != "blue" && background != "green" && background != "red") {
        throw ApiError(400, "Invalid background.");
    }
    if (pose != "none") {
        throw ApiError(400, "Production poses are not implemented yet. Use pose: none.");
    }

    return {{"name", playerName}, {"background", background}, {"pose", pose}, {"design", design}};
}

Json pendingJobResponse(const std::string& jobId, const Json& job) {
    return {
        {"jobId", jobId},
        {"status", job["status"]},
        {"statusUrl", "/api/signature-jobs/" + jobId}
    };
}

struct CachedPlayer {
    Timestamp attemptedAt = 0;
    Json stats;
    int errorCode = 0;
    std::string error;
};

struct CachedImage {
    Json configuration;
    std::string bytes;
    std::string etag;
    std::string renderKey;
    std::string jobId;
    Timestamp nextCheckAt = 0;
    Timestamp accessedAt = 0;
    int errorCode = 0;
    std::string error;
};

struct ImageResult {
    std::string bytes;
    std::string etag;
    int cacheSeconds;
};

Json fetchPlayerStats(const std::string& playerName) {
    const std::string playerUrl =
        "https://secure.runescape.com/m=hiscore_oldschool/index_lite.json?player="
        + encodeUrlParameter(playerName);
    try {
        return normalizePlayerStats(Json::parse(fetchHttp(playerUrl)), playerName);
    } catch (const Json::exception&) {
        throw ApiError(502, "Hiscores returned invalid JSON.");
    }
}

// Ranks and fetch timestamps do not affect the artwork.
std::string artworkKey(const Json& configuration, const Json& stats) {
    Json levels = Json::object();
    for (auto skill = stats.at("skills").begin(); skill != stats.at("skills").end(); ++skill) {
        levels[skill.key()] = skill.value().at("level");
    }
    return Json({{"config", configuration}, {"levels", levels},
        {"totalLevel", stats.at("totalLevel")},
        {"version", rendererManifest().at("version")}}).dump();
}

std::string imageUrl(const Json& configuration) {
    return "/signatures/" + encodeUrlParameter(configuration.at("name").get<std::string>())
        + ".png?design=" + encodeUrlParameter(configuration.at("design").get<std::string>())
        + "&background=" + configuration.at("background").get<std::string>();
}

Json readyImageResponse(const Json& configuration) {
    const std::string url = imageUrl(configuration);
    return {{"status", "ready"}, {"imageUrl", url}, {"shareUrl", url},
        {"downloadUrl", url + "&download=1"}};
}

class SignatureService {
public:
    using StatsLoader = std::function<Json(const std::string&)>;
    using Clock = std::function<Timestamp()>;
    using Renderer = std::function<RenderedImage(const Json&, const Json&)>;

    explicit SignatureService(
        StatsLoader loader = fetchPlayerStats,
        Clock clock = currentTimestamp,
        Renderer renderer = renderSignatureImage
    ) : loader_(std::move(loader)), clock_(std::move(clock)), renderer_(std::move(renderer)),
        worker_(&SignatureService::runWorker, this) {
    }

    ~SignatureService() {
        {
            std::lock_guard lock(stateMutex_);
            stopping_ = true;
        }
        queueCondition_.notify_all();
        completionCondition_.notify_all();
        worker_.join();
    }

    Json getPlayerStats(const std::string& playerName) {
        const std::string name = signature::normalizeName(playerName);
        const std::string key = signature::keyName(name);
        std::lock_guard lock(statsMutex_);
        const Timestamp now = clock_();
        auto cached = playerCache_.find(key);
        if (cached != playerCache_.end() && now - cached->second.attemptedAt < 60) {
            if (cached->second.errorCode) throw ApiError(cached->second.errorCode, cached->second.error);
            return cached->second.stats;
        }
        if (cached == playerCache_.end() && playerCache_.size() >= maximumCachedPlayers) {
            // Never evict a fresh entry: that would permit another upstream lookup within a minute.
            for (auto entry = playerCache_.begin(); entry != playerCache_.end();) {
                if (now - entry->second.attemptedAt >= 60) entry = playerCache_.erase(entry);
                else ++entry;
            }
            if (playerCache_.size() >= maximumCachedPlayers) throw ApiError(503, "Stats cache is busy; retry later.");
        }
        CachedPlayer& player = playerCache_[key];
        player.attemptedAt = now;
        player.errorCode = 0;
        try {
            player.stats = loader_(name);
            player.stats["fetchedAt"] = now;
            return player.stats;
        } catch (const ApiError& error) {
            player.errorCode = error.statusCode;
            player.error = error.what();
            throw;
        } catch (const std::exception&) {
            player.errorCode = 502;
            player.error = "Hiscores unavailable; retry in one minute.";
            throw ApiError(player.errorCode, player.error);
        }
    }

    Json submitSignature(const Json& requestedConfiguration) {
        const Json configuration = validateSignatureConfiguration(requestedConfiguration);
        std::lock_guard lock(stateMutex_);
        const auto image = findOrCreateImage(configuration);
        ensureImageJob(image);
        if (!image->jobId.empty()) return pendingJobResponse(image->jobId, jobs_.at(image->jobId));
        if (image->errorCode) throw ApiError(image->errorCode, image->error);
        return readyImageResponse(configuration);
    }

    Json getJob(const std::string& jobId) {
        std::lock_guard lock(stateMutex_);
        if (!signature::validId(jobId) || !jobs_.contains(jobId)) throw ApiError(404, "Job not found; submit again.");
        return jobs_.at(jobId);
    }

    ImageResult getPngImage(const Json& requestedConfiguration) {
        const Json configuration = validateSignatureConfiguration(requestedConfiguration);
        std::unique_lock lock(stateMutex_);
        const auto image = findOrCreateImage(configuration);
        ensureImageJob(image);
        // A public GET is sufficient to recreate an image after a restart or eviction.
        if (!completionCondition_.wait_for(lock, std::chrono::seconds(90), [&] {
            return image->jobId.empty() || stopping_;
        })) throw ApiError(503, "Image generation timed out; retry shortly.");
        if (stopping_) throw ApiError(503, "Server is stopping.");
        if (image->errorCode) throw ApiError(image->errorCode, image->error);
        const int cacheSeconds = static_cast<int>(std::clamp<Timestamp>(image->nextCheckAt - clock_(), 1, 60));
        return {image->bytes, image->etag, cacheSeconds};
    }

private:
    std::shared_ptr<CachedImage> findOrCreateImage(const Json& configuration) {
        if (stopping_) throw ApiError(503, "Generation worker unavailable.");
        const std::string key = configuration.dump();
        auto found = images_.find(key);
        if (found != images_.end()) {
            found->second->accessedAt = clock_();
            return found->second;
        }
        if (images_.size() >= maximumLiveSignatures) {
            auto oldest = images_.end();
            for (auto entry = images_.begin(); entry != images_.end(); ++entry) {
                if (entry->second->jobId.empty()
                    && (oldest == images_.end() || entry->second->accessedAt < oldest->second->accessedAt)) oldest = entry;
            }
            if (oldest == images_.end()) throw ApiError(503, "Image cache is busy; retry shortly.");
            images_.erase(oldest);
        }
        auto image = std::make_shared<CachedImage>();
        image->configuration = configuration;
        image->accessedAt = clock_();
        images_[key] = image;
        return image;
    }

    void ensureImageJob(const std::shared_ptr<CachedImage>& image) {
        if (!image->jobId.empty() || clock_() < image->nextCheckAt) return;
        if (queue_.size() >= maximumQueuedJobs) throw ApiError(503, "Generation queue is full.");
        for (auto job = jobs_.begin(); job != jobs_.end() && jobs_.size() >= maximumRetainedJobs;) {
            if (!isPendingJob(job.value())) job = jobs_.erase(job);
            else ++job;
        }
        if (jobs_.size() >= maximumRetainedJobs) throw ApiError(503, "Job queue is busy.");
        image->jobId = generateIdentifier();
        jobs_[image->jobId] = {{"status", "queued"}, {"createdAt", clock_()}};
        queue_.push_back(image);
        queueCondition_.notify_one();
    }

    void processImage(const std::shared_ptr<CachedImage>& image, const std::string& jobId) {
        try {
            const Json stats = getPlayerStats(image->configuration.at("name"));
            const std::string key = artworkKey(image->configuration, stats);
            RenderedImage rendered;
            // Only the worker changes bytes/renderKey; readers hold stateMutex_.
            bool changed;
            {
                std::lock_guard lock(stateMutex_);
                changed = image->bytes.empty() || image->renderKey != key;
            }
            if (changed) rendered = renderer_(stats, image->configuration);
            std::lock_guard lock(stateMutex_);
            if (changed) {
                image->bytes = std::move(rendered.bytes);
                image->etag = std::move(rendered.etag);
                image->renderKey = key;
            }
            image->nextCheckAt = stats.at("fetchedAt").get<Timestamp>() + 60;
            image->errorCode = 0;
            image->error.clear();
            jobs_[jobId] = readyImageResponse(image->configuration);
            jobs_[jobId]["completedAt"] = clock_();
            image->jobId.clear();
        } catch (const std::exception& error) {
            std::lock_guard lock(stateMutex_);
            const auto* apiError = dynamic_cast<const ApiError*>(&error);
            image->errorCode = apiError ? apiError->statusCode : 502;
            image->error = error.what();
            image->nextCheckAt = clock_() + 60;
            jobs_[jobId] = {{"status", "failed"}, {"error", image->error}, {"completedAt", clock_()}};
            image->jobId.clear();
        }
        completionCondition_.notify_all();
    }

    void runWorker() {
        while (true) {
            std::shared_ptr<CachedImage> image;
            std::string jobId;
            {
                std::unique_lock lock(stateMutex_);
                queueCondition_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
                if (stopping_) return;
                image = queue_.front();
                queue_.pop_front();
                jobId = image->jobId;
                jobs_[jobId]["status"] = "running";
            }
            processImage(image, jobId);
        }
    }

    StatsLoader loader_;
    Clock clock_;
    Renderer renderer_;
    bool stopping_ = false;
    std::mutex stateMutex_;
    std::mutex statsMutex_;
    std::condition_variable queueCondition_;
    std::condition_variable completionCondition_;
    std::map<std::string, CachedPlayer> playerCache_;
    std::map<std::string, std::shared_ptr<CachedImage>> images_;
    Json jobs_ = Json::object();
    std::deque<std::shared_ptr<CachedImage>> queue_;
    std::thread worker_;
};

crow::response jsonResponse(int statusCode, const Json& body) {
    crow::response response(statusCode, body.dump());
    response.set_header("Content-Type", "application/json");
    response.set_header("Cache-Control", "no-store");
    return response;
}

template <typename RequestHandler>
crow::response handleRequest(RequestHandler&& requestHandler) {
    try {
        return requestHandler();
    } catch (const ApiError& error) {
        return jsonResponse(error.statusCode, {{"error", error.what()}});
    } catch (const std::invalid_argument& error) {
        return jsonResponse(400, {{"error", error.what()}});
    } catch (const Json::exception&) {
        return jsonResponse(400, {{"error", "Invalid JSON or field type."}});
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return jsonResponse(500, {{"error", "Internal server error."}});
    }
}

crow::response handlePlayerStatsRequest(SignatureService& service, const crow::request& request) {
    return handleRequest([&service, &request] {
        const char* playerName = request.url_params.get("name");
        if (playerName == nullptr) {
            throw ApiError(400, "name is required.");
        }
        return jsonResponse(200, service.getPlayerStats(playerName));
    });
}

crow::response handleSignatureRequest(SignatureService& service, const crow::request& request) {
    return handleRequest([&service, &request] {
        if (request.body.size() > 2048) {
            throw ApiError(413, "Request body too large.");
        }

        const Json result = service.submitSignature(Json::parse(request.body));
        const bool imageIsReady = result["status"] == "ready";
        int statusCode = 202;
        if (imageIsReady) {
            statusCode = 200;
        }

        crow::response response = jsonResponse(statusCode, result);
        if (!imageIsReady) {
            response.set_header("Location", result["statusUrl"].get<std::string>());
            response.set_header("Retry-After", "1");
        }
        return response;
    });
}

std::string decodePlayerPath(const std::string& encoded) {
    std::string decoded;
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 >= encoded.size()
                || !std::isxdigit(static_cast<unsigned char>(encoded[i + 1]))
                || !std::isxdigit(static_cast<unsigned char>(encoded[i + 2]))) throw ApiError(400, "Invalid player URL.");
            decoded += static_cast<char>(std::stoi(encoded.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else decoded += encoded[i];
    }
    return decoded;
}

crow::response handleImageRequest(SignatureService& service, const crow::request& request, const std::string& fileName) {
    return handleRequest([&] {
        if (fileName.size() < 5 || fileName.substr(fileName.size() - 4) != ".png") throw ApiError(404, "Image not found.");
        const char* design = request.url_params.get("design");
        const char* background = request.url_params.get("background");
        const Json configuration = {
            {"name", decodePlayerPath(fileName.substr(0, fileName.size() - 4))},
            {"design", design ? design : "classic"}, {"background", background ? background : "blue"}, {"pose", "none"}
        };
        const ImageResult image = service.getPngImage(configuration);
        const std::string condition = request.get_header_value("If-None-Match");
        bool matches = condition == "*";
        std::istringstream tags(condition);
        std::string tag;
        while (std::getline(tags, tag, ',')) {
            const auto begin = tag.find_first_not_of(" \t");
            const auto end = tag.find_last_not_of(" \t");
            if (begin == std::string::npos) continue;
            tag = tag.substr(begin, end - begin + 1);
            if (tag.rfind("W/", 0) == 0) tag.erase(0, 2);
            if (tag == image.etag) matches = true;
        }
        crow::response response(matches ? 304 : 200);
        if (!matches) response.body = image.bytes;
        response.set_header("Content-Type", "image/png");
        response.set_header("ETag", image.etag);
        response.set_header("Cache-Control", "public, max-age=0, s-maxage=" + std::to_string(image.cacheSeconds) + ", must-revalidate");
        response.set_header("X-Content-Type-Options", "nosniff");
        if (const char* download = request.url_params.get("download"); download && std::string(download) == "1") {
            response.set_header("Content-Disposition", "attachment; filename=\"osrs-signature.png\"");
        }
        return response;
    });
}

void registerRoutes(crow::SimpleApp& application, SignatureService& service) {
    CROW_ROUTE(application, "/health")([] {
        return jsonResponse(200, {{"status", "ok"}});
    });

    CROW_ROUTE(application, "/api/players")
        .methods(crow::HTTPMethod::GET)([&service](const crow::request& request) {
            return handlePlayerStatsRequest(service, request);
        });

    CROW_ROUTE(application, "/api/signatures")
        .methods(crow::HTTPMethod::POST)([&service](const crow::request& request) {
            return handleSignatureRequest(service, request);
        });

    CROW_ROUTE(application, "/api/signature-jobs/<string>")([&service](std::string jobId) {
        return handleRequest([&service, &jobId] {
            return jsonResponse(200, service.getJob(jobId));
        });
    });

    CROW_ROUTE(application, "/signatures/<string>")([&service](const crow::request& request, std::string fileName) {
        return handleImageRequest(service, request, fileName);
    });
}


void validateRenderBundle() {
    const filesystem::path script = environmentValue("SIGNATURE_RENDERER_SCRIPT", "../renderer/render.mjs");
    if (!filesystem::exists(script) || !filesystem::exists(rendererBundleDirectory() / "render.html")) {
        throw std::runtime_error("Missing browser renderer; install renderer dependencies and run npm run build:render in frontend.");
    }
    const Json& manifest = rendererManifest();
    if (!manifest.at("version").is_string() || !manifest.at("designs").is_array()) {
        throw std::runtime_error("Invalid signature render manifest.");
    }
}

} // namespace

#ifndef OSRS_SIGNATURE_TEST
int main(int argumentCount, char** arguments) {
    try {
        curl_global_init(CURL_GLOBAL_DEFAULT);

        const filesystem::path assetsDirectory = environmentValue("ASSETS_DIR", "assets");
        // Generated PNGs, jobs and stats exist only in memory.
        signal(SIGPIPE, SIG_IGN);

        if (argumentCount > 1 && std::string(arguments[1]) == "--prepare-assets") {
            prepareAssets(assetsDirectory);
            std::cout << "Sprite atlas prepared.\n";
            return 0;
        }

        validateRenderBundle();
        SignatureService service;
        crow::SimpleApp application;
        registerRoutes(application, service);

        const int port = std::stoi(environmentValue("PORT", "8081"));
        if (port < 1 || port > 65535) {
            throw std::runtime_error("Invalid port.");
        }

        application.bindaddr(environmentValue("BIND_ADDRESS", "127.0.0.1"));
        application.port(static_cast<std::uint16_t>(port));
        application.concurrency(4);
        application.run();
    } catch (const std::exception& error) {
        std::cerr << "Startup failed: " << error.what() << '\n';
        return 1;
    }
}

#endif
