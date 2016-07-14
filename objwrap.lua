--[[
	objwrap.lua - "object wrapper"
	2014-06-11 [BNO]

	This is a Lua helper program to convert files (e.g. .lua scripts) into
	binary resources, which then can be included into the final executable by
	the linker. This tool is designed to handle (optional) data compression, and
	also deals with some oddities of objcopy invocation and symbol handling.
	The Makefile won't have to be concerned with that, it just uses
	"luajit objwrap.lua <objcopy cmd> <infile> <outfile>".

	(A custom package.loader will later be able to locate the linked resource
	- by retrieving the corresponding symbol information - and make it available
	to Lua again, see symbol.c)

	In principle, this could also be achieved with compiling LuaJIT bytecode
	(luajit -b). However, there are some potential drawbacks. Most important:
	We want to stay in control of when Lucciefr 'falls back' to using builtin
	modules for "dofile" or "require", and don't want that to happen if the
	SDK or user-modified files are present. Plus: bytecode compilation won't
	help with large string resources like e.g. extensive ffi.cdef declarations.
	Such ".h" wrappers might compress nicely with gzip, but don't really
	benefit from luajit -b (as large strings essentially remain unchanged).
]]

-- helper function: execute a (system) command, and make sure it succeeds
function checked_execute(cmd)
	print(cmd) -- DEBUG only
	status = os.execute(cmd)
	if status ~= 0 then
		print(string.format("Execution of '%s' failed with status code %d - exiting.", cmd, status))
		os.exit(status)
	end
end


--[[ main ]]--

-- objcopy command (including any required format options) passed from make
objcopy = arg[1] or error("missing <objcopy> command")

-- filename arguments
src = arg[2] or error("missing <source> filename")
dst = arg[3] or error("missing <destination> filename")

-- provide some short console echo
-- (this is useful if the Makefile suppresses the invocation string, which tends to be rather long)
print(string.format("BINWRAP %s %s", src, dst))

-- We want objcopy to produce short symbols (and not to include any
-- unnecessary or misleading information based on the filename). To
-- achieve that let's make it work on a temporary file with a fixed name.
temp = "data" -- this is our 'base' name (that ends up as part of the symbols)

temp = dst:sub(1, dst:find("[^\\/]*$") - 1) .. temp -- (dir is taken from dst)


-- First, we'll copy from the source to the temporary file. To save space
-- on the resulting binary, this is normally done using (gzip) compression.
-- If you don't want that, replace with a simply copy: cmd = string.format('cp "%s" "%s"', src, temp)

--cmd = string.format('gzip -n9c "%s" > "%s"', src, temp)
cmd = string.format('cp "%s" "%s"', src, temp)
checked_execute(cmd)


-- Now we build an objcopy command that identifies the source file via a
-- symbol prefix. This prefix string will precede anything else in the symbol
-- names that objcopy produces, so we can use it later to reliably identify/check
-- the original file (pathname).
-- This is necessary as here we're now operating on the temp file, which has
-- been renamed on purpose - and objcopy constructs symbol information based on
-- the (input) filename. So we'd otherwise end up with useless symbol names.

-- One big CAVEAT: Be careful when constructing prefixes! objcopy would normally
-- use a "_binary_" or "_binary_obj_" prefix. Note that this has a leading
-- underscore, which is a special char that may affect the linker's behaviour.
-- (Use "objdump -t *.o" to see which symbols are actually used.)

-- If you use an arbitrary string for the prefix, and the linking stage fails
-- with an error message ("Cannot export <...>: symbol not found"), chances are
-- that you are missing this special "tag".
-- The solution is simple: Either make sure you explicitly start your prefix with
-- an underscore (--prefix-symbols=_foo), or use the "--change-leading-char" option
-- to have objcopy include it for you (--change-leading-char --prefix-symbols=bar).
-- Both should prevent the linking error.

-- construct prefix from the source filename,
-- by converting any non-alphanumeric characters to an underscore
prefix = src:gsub("%W", "_")

-- build and execute the actual objcopy command (to work on the temporary filename)
--cmd = objcopy .. string.format(' --change-leading-char --prefix-symbols=%s "%s"', prefix, temp)
cmd = objcopy .. string.format(' --prefix-symbols=%s "%s"', prefix, temp)
checked_execute(cmd)

-- and finally change the temporary file to the "real" destination name

os.remove(dst) -- (otherwise rename would fail on existing dst)
result, err = os.rename(temp, dst)
if not result then error(err); end

-- And we're done: Success!
os.exit(0)
