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
		return;
	}

	/*
	cerr << "\nReceived seg from: " << seg.header().sport << " to " << seg.header().dport << "\t";
	cerr << "\nheader.syn: " << seg.header().syn << " header.fin: " << seg.header().fin << "\t";
	cerr << "\nheader.ack: " << seg.header().ack << " header.win: " << seg.header().win << "\t";
	cerr << "\nheader.seqno: " << seg.header().seqno.raw_value() << " header.ackno " << (seg.header().ack ? seg.header().ackno.raw_value() : -1) << "\t";
	cerr << "\nseg size is: " << seg.length_in_sequence_space() << "\n";
	*/

	// Tell the sender about ackno and window size
	if(seg.header().ack) {
		_sender.ack_received(seg.header().ackno, seg.header().win);
	}

	_receiver.segment_received(seg);
	
	if(_receiver.stream_out().input_ended() && not _sender.fin_sended()) {
		_linger_after_streams_finish = false;
	}

	// If the segment contain valid data, then acknowledge it
	if(_sender.segments_out().empty() && seg.header().syn && _sender.not_syn()) {
		connect();
	} else if(_sender.segments_out().empty() && seg.length_in_sequence_space()) {
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
		/*
		cerr << "\nSend seg from: " << seg.header().sport << " to " << seg.header().dport << "\t";
		cerr << "\nheader.syn: " << seg.header().syn << " header.fin: " << seg.header().fin << "\t";
		cerr << "\nheader.ack: " << seg.header().ack << " header.win " << seg.header().win << "\t";
		cerr << "\nheader.seqno: " << seg.header().seqno.raw_value() << " header.ackno " << (seg.header().ack ? seg.header().ackno.raw_value() : -1) << "\n";
		*/
		segments_out().push(seg);
		_sender.segments_out().pop();
	}
}

bool TCPConnection::active() const {
	if(_sender.stream_in().error() && _receiver.stream_out().error())
		return false;

	//! Active close:
	if (_time_out)
		return false;

	//! Passive close:
	if(_sender.fin_sended() && not bytes_in_flight() && _receiver.stream_out().input_ended() && not _linger_after_streams_finish)
		return false;

	return true;
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

	if(_sender.fin_sended() && not bytes_in_flight() && _receiver.stream_out().eof() && _linger_after_streams_finish) {
		_timer += ms_since_last_tick;
		if(_timer >= 10 * _cfg.rt_timeout) {
			_time_out = true;
			return;
		}
	} 

	_sender.tick(ms_since_last_tick);

	if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
		TCPSegment seg{};
		seg.header().rst = true;
		if(_receiver.ackno().has_value()) {
			seg.header().ack = true;
			seg.header().ackno = _receiver.ackno().value();
			seg.header().win = _receiver.window_size();
		}
		// cerr << "\ntest\n";
		// cerr << _sender.fin_sended() << "\n";
		// cerr << _sender.bytes_in_flight() << "\n";
		// cerr << _linger_after_streams_finish << "\n";
		segments_out().push(seg);
		_sender.stream_in().set_error();
		_receiver.stream_out().set_error();
	} else {
		send_all_segments();
	}
}

void TCPConnection::end_input_stream() {
	// cerr << "\nend_input_stream()\n";
	_sender.stream_in().end_input();
	_sender.fill_window();
	send_all_segments();
}

void TCPConnection::connect() {
	TCPHeader header{};
	header.syn = true;
	_sender.set_syn();
	_sender.send_empty_segment(header);
	send_all_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
			TCPHeader header{};
			header.rst = true;
			_sender.send_empty_segment(header);
			send_all_segments();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
