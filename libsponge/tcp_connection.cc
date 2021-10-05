#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _timer; }

void TCPConnection::segment_received(const TCPSegment &seg) {
	if(not active())
		return;

	_timer = 0;

	if(seg.header().rst) {
		_sender.stream_in().set_error();
		_receiver.stream_out().set_error();
		_active = false;
		return;
	}

	bool need_send_ack_for_empty = seg.length_in_sequence_space() > 0;
	if(not _receiver.FIN_RECV())
		need_send_ack_for_empty |= not _receiver.segment_received(seg);

	// Tell the sender about ackno and window size
	if(seg.header().ack && (not _sender.FIN_ACKED() && not _sender.CLOSE() && not _receiver.LISTEN()))
		need_send_ack_for_empty |= not _sender.ack_received(seg.header().ackno, seg.header().win);
	
	if(_receiver.stream_out().input_ended() && not _sender.stream_in().eof()) {
		_linger_after_streams_finish = false;
	}

	// If the segment contain valid data, then acknowledge it
	if(_sender.segments_out().empty() && seg.header().syn && _sender.not_syn()) {
		connect();
	} else if(_sender.segments_out().empty() && need_send_ack_for_empty) {
		_sender.send_empty_segment();
	}

	send_all_segments();
}

void TCPConnection::send_all_segments() {
	while(not _sender.segments_out().empty()) {
		TCPSegment seg = _sender.segments_out().front();
		if(_receiver.ackno().has_value()) {
			seg.header().ack = true;
			seg.header().ackno = _receiver.ackno().value();
			seg.header().win = min(_receiver.window_size(), static_cast<size_t>(std::numeric_limits<uint16_t>::max()));
		}
		segments_out().push(seg);
		_sender.segments_out().pop();
	}

	if(_receiver.FIN_RECV() && _sender.FIN_ACKED() && not _linger_after_streams_finish)
		_active = false;
}

bool TCPConnection::active() const {
	return _active;
}

size_t TCPConnection::write(const string &data) {
	size_t length = _sender.stream_in().write(data);
	_sender.fill_window();
	send_all_segments();
    return length;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
	if(not active())
		return;

	_sender.tick(ms_since_last_tick);

	if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
		while(not _sender.segments_out().empty()) {
			_sender.segments_out().pop();
		}
		send_rst();
		return;
	}
	send_all_segments();

	_timer += ms_since_last_tick;

	if(_sender.FIN_ACKED() && _receiver.FIN_RECV() && _linger_after_streams_finish) {
		if(_timer >= 10 * _cfg.rt_timeout) {
			_active = false;
			return;
		}
	} 
}

void TCPConnection::end_input_stream() {
	_sender.stream_in().end_input();
	_sender.fill_window();
	send_all_segments();
}

void TCPConnection::connect() {
	_sender.fill_window();
	send_all_segments();
}

void TCPConnection::send_rst() {
	TCPHeader header{};
	header.rst = true;
	_sender.send_empty_segment(header);
	_sender.stream_in().set_error();
	_receiver.stream_out().set_error();
	_active = false;
	send_all_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
			send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
