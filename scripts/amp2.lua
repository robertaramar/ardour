ardour {
	["type"]    = "dsp",
	name        = "Simple Amplifier II",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[
	An Example Pluing for processing audio, to
	be used with Ardour's Lua scripting facility.]]
}

function ioconfig ()
	return { [1] = { audio_in = -1, audio_out = -1}, }
end

-- this variant modifies the audio data in-place
-- in Ardour's buffer.

function dsp (bufs, n_samples)
	local n_channels = bufs:count():n_audio();
	for c = 1,n_channels do
		local a = bufs:get_audio(c - 1):data(0):array() -- get a reference (pointer to array)
		for s = 1,n_samples do
			a[s] = a[s] * 2; -- modify data in-place (shared with ardour)
		end
	end
end
