module("process", package.seeall)

function getProcesses()
	local processes, pids = {}, process_get_pids_C()
	for k, v in ipairs(pids) do
		local filename, err = process_get_module_name_C(v)
		filename = filename or ("ERROR: " .. err) -- DEBUG only
		--print(v .. " = " .. filename)
		processes[v] = filename
	end
	return processes
end
