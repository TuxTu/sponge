#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outstanding byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
	: _timer(TCPSender::Timer())
    , _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
	, _RTO{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _unacked_bytes; }

void TCPSender::fill_window() {
	if(!syn){
		TCPSegment new_seg{};
		new_seg.header().syn = true;
		new_seg.header().seqno = _isn;
		send_segment(new_seg);
		syn = true;
		return;
	}
	
	if(fin)
		return;

	if(_window_size > 0 && _stream.buffer_empty() && not fin){
		if(_stream.eof()){
			TCPSegment new_seg{};
			new_seg.header().fin = true;
			new_seg.header().seqno = next_seqno();
			fin = true;
			send_segment(new_seg);
			_window_size -= new_seg.length_in_sequence_space();
		}
	}

	while(_window_size > 0 && not _stream.buffer_empty() && not fin){
		TCPSegment new_seg{};
		new_seg.header().seqno = next_seqno();
		uint16_t length = min({_stream.buffer_size(), TCPConfig::MAX_PAYLOAD_SIZE, static_cast<size_t>(_window_size)});
		new_seg.payload() = Buffer(_stream.read(length));
		if(new_seg.length_in_sequence_space() < _window_size){
			new_seg.header().fin = _stream.eof();
			fin = _stream.eof();
		}
		send_segment(new_seg);
		_window_size -= new_seg.length_in_sequence_space();
	}

	return;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
	if(unwrap(ackno, _isn, _next_seqno) > _next_seqno)
		return false;

	// If ackno is outdated, just ignore it
	if(unwrap(ackno, _isn, _next_seqno) < _recent_ackno)
		return true;

	// Pop up all the segments whose payload has been acknowledged
	while(not _segments_outstanding.empty()){
		TCPSegment seg_front = _segments_outstanding.front();
		if(unwrap(seg_front.header().seqno, _isn, _next_seqno) + seg_front.length_in_sequence_space() <= unwrap(ackno, _isn, _next_seqno)){
			_unacked_bytes -= seg_front.length_in_sequence_space();
			_segments_outstanding.pop();
			// When any outstanding segment is acknowledged, reset related state 
			_RTO = _initial_retransmission_timeout;
			_timer._counter_ms = 0;
			_consecutive_retransmissions = 0;
			continue;
		}
		break;
	}

	// If all outstanding segments are acknowledged, turn off timer
	if(_segments_outstanding.empty())
		_timer._boot = false;

	_recent_ackno = unwrap(ackno, _isn, _next_seqno);

	_window_size = (window_size == 0) ? 1 : (_recent_ackno + window_size >= _next_seqno ? _recent_ackno + window_size - _next_seqno : 0);
	_back_off = (window_size != 0);

	fill_window();
	return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
	if (_timer._boot)
		_timer._counter_ms += ms_since_last_tick;
	else
		return;

	// If timer elapsed, re-send the earliest segment in the outstanding queue
	if(_timer._counter_ms >= _RTO && not _segments_outstanding.empty()){
		_segments_out.push(_segments_outstanding.front());
		_timer._counter_ms = 0;
		if(_back_off) {
			_RTO = _RTO << 1;
			_consecutive_retransmissions++;
		} 
	}
	
	return;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
	TCPSegment seg = {};
	seg.header().seqno = next_seqno();
	send_segment(seg);
}

void TCPSender::send_empty_segment(const TCPHeader header) {
	TCPSegment seg = {};
	seg.header() = header;
	seg.header().seqno = next_seqno();

	send_segment(seg);
}

void TCPSender::send_segment(const TCPSegment seg) {
	// Push the prepared segment into queue
	_unacked_bytes += seg.length_in_sequence_space();
	_next_seqno += seg.length_in_sequence_space();
	_segments_out.push(seg);
	if(seg.length_in_sequence_space() != 0) {
		_segments_outstanding.push(seg);
		if(not _timer._boot){
			_timer._boot = true;
			_timer._counter_ms = 0;
		}
	}
}
