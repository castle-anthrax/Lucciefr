module("process", package.seeall)

function getProcesses()
	pids = get_processes_C()
	for k,v in pairs(pids) do
		print (k,v)
	end
end
