#include "hook/HookManager.hpp"

#include <cstddef>

#include "lua/LuaEventBus.hpp"
#include "mod/BedrockLuaMod.hpp"
#include "sig/SignatureRegistry.hpp"
#include "util/AbiString.hpp"
#include "util/Log.hpp"

// ChatHooks intercepts incoming chat at ServerNetworkHandler::handle(TextPacket)
// and routes it through the Lua event bus:
//   * world.beforeEvents.chatSend  (cancellable - suppresses the original)
//   * world.afterEvents.chatSend   (notification)
//
// Reading the sender/message out of the TextPacket requires that packet's field
// offsets, which are version-specific. They live in kTextPacket* below. When
// left at 0 (the safe default) the hook is a transparent pass-through: it never
// dereferences an unknown offset, so it cannot crash. Set the offsets for your
// target build (see docs/SIGNATURES.md) to enable chat events.

namespace bedrocklua::hooks {

namespace {
// TextPacket field offsets (bytes from the packet base). 0 == unconfigured.
constexpr std::ptrdiff_t kTextPacketAuthorOffset = 0;
constexpr std::ptrdiff_t kTextPacketMessageOffset = 0;

using HandleTextFn = void (*)(void* self, const void* netId, const void* textPacket);
HandleTextFn g_origHandleText = nullptr;
bool g_loggedDisabled = false;

void HandleText_detour(void* self, const void* netId, const void* textPacket) {
    if (kTextPacketMessageOffset == 0 || textPacket == nullptr) {
        if (!g_loggedDisabled) {
            g_loggedDisabled = true;
            log::info("[chat] event extraction disabled (TextPacket offsets unset); "
                      "passing through");
        }
        if (g_origHandleText) g_origHandleText(self, netId, textPacket);
        return;
    }

    const auto* base = reinterpret_cast<const std::uint8_t*>(textPacket);
    std::string author = kTextPacketAuthorOffset != 0
                             ? abi::readString(base + kTextPacketAuthorOffset)
                             : std::string{};
    std::string message = abi::readString(base + kTextPacketMessageOffset);

    bool cancelled = BedrockLuaMod::getInstance().events().fireChatSend(author, 0, message);
    if (cancelled) {
        log::debug("[chat] message from '{}' cancelled by script", author);
        return;  // suppress original -> message is dropped
    }

    if (g_origHandleText) g_origHandleText(self, netId, textPacket);
    BedrockLuaMod::getInstance().events().fireChatReceive(author, message);
}
}  // namespace

bool installChatHooks(Runtime& rt) {
    std::uintptr_t target = rt.signatures.resolve("ServerNetworkHandler::handleTextPacket");
    return rt.hooks.install("ServerNetworkHandler::handleTextPacket", target,
                            reinterpret_cast<void*>(&HandleText_detour),
                            reinterpret_cast<void**>(&g_origHandleText));
}

}  // namespace bedrocklua::hooks
