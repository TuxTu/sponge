#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
	if(seg.header().syn) {
		if(not LISTEN())
			return false;
		_isn = WrappingInt32(seg.header().seqno.raw_value());
		if(seg.length_in_sequence_space() == 1) 
			return true;
	}

	if(LISTEN())
		return true;

	if(seg.header().fin && FIN_RECV())
		return false;

	if(SYN_RECV()) {
		size_t abs_seqno = unwrap(seg.header().seqno, _isn.value(), abs_ackno());
		size_t index = abs_seqno;
		index -= !(seg.header().syn);
		if(abs_seqno + seg.length_in_sequence_space() < abs_ackno()) return false;
		if(abs_seqno > abs_ackno() + _capacity) return false;
		_reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
	}
	return true;
}
		
/*
	if(seg.header().syn && !_isn.has_value())
		_isn = WrappingInt32(seg.header().seqno.raw_value());
	if(_isn.has_value()){
		_checkpoint = unwrap(seg.header().seqno, _isn.value(), _checkpoint);
		if(_checkpoint == 0 && !seg.header().syn)
			return;
		_reassembler.push_substring(seg.payload().copy(), (_checkpoint == 0 ? 0 : _checkpoint - 1), seg.header().fin);
	}
}
*/

optional<WrappingInt32> TCPReceiver::ackno() const {
	if(_isn.has_value()){
		return WrappingInt32(wrap(_reassembler.get_head()+(_isn.has_value() ? 1 : 0)+(_reassembler.stream_out().input_ended() ? 1 : 0), _isn.value()));
	}
	else
		return std::nullopt;
}

size_t TCPReceiver::abs_ackno() const {
	return _reassembler.assembled_bytes() + _isn.has_value();
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
