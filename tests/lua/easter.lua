-- testing "new style" module returning only a private module table
-- i.e. require("easter") won't set a global variable, but nevertheless
-- require("easter").egg() works just fine

local _M = {}

function _M.egg()
	print("You found me! :)")
end

return _M
