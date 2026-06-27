#include "sig/SignatureRegistry.hpp"

#include <Gloss.h>
#include <nlohmann/json.hpp>

#include <fstream>

#include "sig/ModuleScanner.hpp"
#include "util/Log.hpp"

namespace bedrocklua {

namespace {
// Adds a logical entry with one or more candidate resolvers. Candidates are
// tried in order at resolve time; the first that the loaded module satisfies
// wins. Listing candidates from several Minecraft versions is how the table
// stays version-aware without needing exact runtime version detection.
void add(std::vector<SignatureRegistry::Entry>& out, std::string name,
         std::vector<SignatureRegistry::Candidate> candidates) {
    out.push_back({std::move(name), std::move(candidates)});
}

SignatureRegistry::Candidate sym(std::string s, std::string ver) {
    return {std::move(s), /*isPattern=*/false, std::move(ver)};
}
SignatureRegistry::Candidate pat(std::string s, std::string ver) {
    return {std::move(s), /*isPattern=*/true, std::move(ver)};
}
}  // namespace

void SignatureRegistry::loadEmbeddedDefaults() {
    // Embedded fallback for the shippable src/sig/signatures.json. Targets the
    // Minecraft Bedrock 1.21-1.26 range: these core mangled names are largely
    // stable across those minors, and resolve() tries each candidate against the
    // loaded binary, so whatever matches the running version is used while
    // unresolved entries degrade gracefully.
    entries_.clear();

    // --- engine lifecycle (drives the Lua scheduler + world events) --------
    add(entries_, "Level::tick",
        {sym("_ZN5Level4tickEv", "1.21-1.26"),
         sym("_ZN11ServerLevel4tickEv", "1.21-1.26")});

    add(entries_, "ServerLevel::onLevelLoaded",
        {sym("_ZN5Level13onLevelLoadedEv", "1.21-1.26")});

    // --- chat (world.sendMessage + chatSend events) ------------------------
    add(entries_, "ServerNetworkHandler::handleTextPacket",
        {sym("_ZN20ServerNetworkHandler6handleERK18NetworkIdentifierRK10TextPacket",
             "1.21-1.26")});

    add(entries_, "TextPacket::createChat",
        {sym("_ZN10TextPacket10createChatERKNSt6__ndk112basic_stringIcNS0_11char_traits"
             "IcEENS0_9allocatorIcEEEES8_PS6_S8_S8_", "1.21-1.26")});

    add(entries_, "ServerPlayer::sendNetworkPacket",
        {sym("_ZNK6Player17sendNetworkPacketER6Packet", "1.21-1.26")});

    // --- command execution -------------------------------------------------
    add(entries_, "MinecraftCommands::executeCommand",
        {sym("_ZNK17MinecraftCommands14executeCommandE13CommandContextb", "1.21-1.26")});

    // --- resource/behavior pack stack (PackStackHooks) ---------------------
    add(entries_, "ResourcePackManager::setStack",
        {sym("_ZN19ResourcePackManager8setStackENSt6__ndk110unique_ptrI16ResourcePackStackNS0_14default_deleteIS2_EEEE16ResourcePackTypeb",
             "1.21-1.26")});

    // --- player / actor accessors (offset-dependent bindings) --------------
    add(entries_, "Level::getActivePlayerCount",
        {sym("_ZNK5Level20getActivePlayerCountEv", "1.21-1.26")});
    add(entries_, "Actor::getNameTag",
        {sym("_ZNK5Actor10getNameTagB5cxx11Ev", "1.21-1.26")});
    add(entries_, "Actor::getPosition",
        {sym("_ZNK5Actor11getPositionEv", "1.21-1.26")});
    add(entries_, "Player::getInventory",
        {sym("_ZN6Player18getSuppliesContainerEv", "1.21-1.26")});

    // --- script engine seam (phase-2 first-class lua) ----------------------
    add(entries_, "ScriptModuleMinecraft::registerBindings",
        {sym("_ZN36ScriptModuleMinecraft_ServerBindings16InitializeModuleER19ScriptModuleContext",
             "1.21-1.26")});
}

bool SignatureRegistry::loadFromFile(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    auto root = nlohmann::json::parse(text, nullptr, false, true);
    if (root.is_discarded() || !root.is_object()) {
        log::warn("signatures.json is invalid; using embedded defaults");
        return false;
    }

    if (root.contains("module") && root["module"].is_string()) {
        moduleName_ = root["module"].get<std::string>();
    }
    if (!root.contains("signatures") || !root["signatures"].is_object()) {
        return false;
    }

    entries_.clear();
    for (auto it = root["signatures"].begin(); it != root["signatures"].end(); ++it) {
        Entry entry;
        entry.name = it.key();
        const auto& val = it.value();
        const auto& cands = val.contains("candidates") ? val["candidates"] : val;
        if (cands.is_array()) {
            for (const auto& c : cands) {
                Candidate cand;
                cand.version = c.value("version", "");
                if (c.contains("symbol")) {
                    cand.value = c["symbol"].get<std::string>();
                    cand.isPattern = false;
                } else if (c.contains("pattern")) {
                    cand.value = c["pattern"].get<std::string>();
                    cand.isPattern = true;
                } else {
                    continue;
                }
                entry.candidates.push_back(std::move(cand));
            }
        }
        if (!entry.candidates.empty()) entries_.push_back(std::move(entry));
    }
    return !entries_.empty();
}

bool SignatureRegistry::load(const std::filesystem::path& modDir, std::string moduleName) {
    moduleName_ = std::move(moduleName);
    cache_.clear();
    matchedFrom_.clear();

    auto file = modDir / "signatures.json";
    if (loadFromFile(file)) {
        log::info("loaded {} signature entries from {}", entries_.size(), file.string());
    } else {
        loadEmbeddedDefaults();
        log::info("using {} embedded signature entries", entries_.size());
    }
    return true;
}

std::uintptr_t SignatureRegistry::resolve(const std::string& name) {
    if (auto it = cache_.find(name); it != cache_.end()) {
        return it->second;
    }

    // Symbol candidates resolve through GlossSymbol against the loaded module;
    // pattern candidates scan the module's executable memory. GlossOpen finds
    // the already-loaded library (it does not dlopen a new copy).
    GHandle handle = GlossOpen(moduleName_.c_str());

    std::uintptr_t address = 0;
    std::string matched;
    for (const auto& entry : entries_) {
        if (entry.name != name) continue;
        for (const auto& cand : entry.candidates) {
            std::uintptr_t a = 0;
            if (cand.isPattern) {
                a = scan::findPattern(moduleName_, cand.value);
            } else if (handle != nullptr) {
                a = GlossSymbol(handle, cand.value.c_str(), nullptr);
            }
            if (a != 0) {
                address = a;
                matched = (cand.isPattern ? "pattern" : "symbol");
                if (!cand.version.empty()) matched += "@" + cand.version;
                break;
            }
        }
        break;
    }

    cache_[name] = address;
    matchedFrom_[name] = matched;
    return address;
}

std::vector<SignatureRegistry::Status> SignatureRegistry::snapshot() {
    std::vector<Status> out;
    out.reserve(entries_.size());
    for (const auto& entry : entries_) {
        std::uintptr_t addr = resolve(entry.name);
        Status s;
        s.name = entry.name;
        s.resolved = addr != 0;
        s.address = addr;
        auto it = matchedFrom_.find(entry.name);
        s.matchedFrom = it != matchedFrom_.end() ? it->second : "";
        out.push_back(std::move(s));
    }
    return out;
}

}  // namespace bedrocklua
