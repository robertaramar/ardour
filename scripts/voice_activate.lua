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

function dsp (bufs, n_samples)
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
