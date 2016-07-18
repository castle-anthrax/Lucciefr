module("process", package.seeall)

function getProcesses()
	pids = process_get_pids_C()
	processes = {}
	for k,v in pairs(pids) do
		processes[v] = process_get_module_name_C(v)
	end
	return processes
end
