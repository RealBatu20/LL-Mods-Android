-- A tiny local Lua module, imported by name via the manifest's
-- bedrocklua.imports. Demonstrates "bring your own Lua API" working entirely
-- offline (local file source). A module returns its public table.
local M = {}

function M.greeting(name)
    return "hello, " .. tostring(name) .. ", from an imported lua module!"
end

return M
