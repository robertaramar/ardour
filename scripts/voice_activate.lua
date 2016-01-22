ardour {
	["type"]    = "dsp",
	name        = "Voice/Level Activate",
	license     = "MIT",
	author      = "Robin Gareus",
	authoremail = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[
	An Example Audio Plugin that rolls the transport
	when the signal level on the plugin's input exeeds -6dBFS.]]
}

function ioconfig ()
	-- hardcoded for now..
	-- TODO use a closure + anon function. iterative calls to get next.
	-- see http://www.lua.org/pil/6.1.html http://www.lua.org/pil/11.5.html
	return
	{
		[1] = { audio_in = -1, audio_out = -1},
	}
end

-- use ardour's vectorized functions
function dsp (bufs, n_samples)
	if Session:transport_rolling() then return end
	local n_channels = bufs:count():n_audio();
	for c = 1,n_channels do
		local a = ARDOUR.DSP.compute_peak(bufs:get_audio(c - 1):data(0), n_samples, 0);
		if a > 0.5 then -- -6dBFS
				Session:request_transport_speed(1.0, true)
		end
	end
end

-- calculate peak in lua (unused)
function dsp_old (bufs, n_samples)
	if Session:transport_rolling() then return end
	local n_channels = bufs:count():n_audio();
	for c = 1,n_channels do
		local a = bufs:get_audio(c - 1):data(0):array()
		for i = 1,n_samples do
			if a[i] > 0.5 or a[i] < -0.5 then
				Session:request_transport_speed(1.0, true)
				return
			end
		end
	end
end

