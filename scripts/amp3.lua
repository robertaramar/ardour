ardour {
	["type"]    = "dsp",
	name        = "Simple Amplifier III",
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

-- use ardour's vectorized functions
function dsp (bufs, n_samples)
	local n_channels = bufs:count():n_audio()
	for c = 1,n_channels do
		ARDOUR.DSP.apply_gain_to_buffer (bufs:get_audio(c - 1):data(0), n_samples, 2.0);
	end
end
