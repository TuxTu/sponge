#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
	if(seg.header().syn && !_isn.has_value())
		_isn = WrappingInt32(seg.header().seqno.raw_value());
	if(_isn.has_value()){
		_checkpoint = unwrap(seg.header().seqno, _isn.value(), _checkpoint);
		if(_checkpoint == 0 && !seg.header().syn)
			return;
		_reassembler.push_substring(seg.payload().copy(), (_checkpoint == 0 ? 0 : _checkpoint - 1), seg.header().fin);
	}
}

optional<WrappingInt32> TCPReceiver::ackno() const {
	if(_isn.has_value()){
		return WrappingInt32(wrap(_reassembler.get_head()+(_isn.has_value() ? 1 : 0)+(_reassembler.stream_out().input_ended() ? 1 : 0), _isn.value()));
	}
	else
		return std::nullopt;
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
