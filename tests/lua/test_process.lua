local lu = require("lua.luaunit")
require("core.process")

TestProcess = { __class = "TestProcess" }

function TestProcess:testGetPids()
	lu.assertIsFunction(process_get_pids_C)
	local pids = process_get_pids_C()
	lu.assertIsTable(pids)
	assert(#pids > 0)
end

function TestProcess:testProcesses()
	local procs = process.getProcesses()
	lu.assertIsTable(procs)
	-- make sure table is not empty (can't use #procs, as it's NOT an array)
	lu.assertNotNil(next(procs))
end
