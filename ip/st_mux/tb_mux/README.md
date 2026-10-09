# Streaming mux reference regression

`test2` keeps both sources active with uniquely numbered payloads while randomizing
downstream readiness and channel-switch requests. It requires handshake
conservation, ordered delivery of exactly 256 words per source, full data/sideband
stability while stalled, and correct live low/high count-register reads.
The output sideband identifies the selected input, independently of input tags.
