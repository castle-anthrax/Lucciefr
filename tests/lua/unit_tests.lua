print "\nRunning Lua unit tests..."
local lu = require("lua.luaunit")

-- include the various test suites
dofile("lua/test_process.lua")

return lu.run("-v") -- "-v" = verbose
