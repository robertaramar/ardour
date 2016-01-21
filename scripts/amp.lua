ardour {
	["type"]    = "dsp",
	name        = "Simple Amplifier",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[
	An Example Pluing for processing audio, to
	be used with Ardour's Lua scripting facility.]]
}

-- this variant asks for a complete *copy* of the
-- audio data in a lua-table.
-- after processing the data needs to be copied back

function dsp (bufs, n_samples)
	local n_channels = bufs:count():n_audio()
	for c = 1,n_channels do
		local a = bufs:get_audio(c - 1):data(0):get_table(n_samples) -- copy audio-data
		for s = 1,n_samples do
			a[s] = a[s] * 2; -- modify lua's table
		end
		bufs:get_audio(c - 1):data(0):set_table(a, n_samples) -- copy back
	end
end
