#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
	class Timer {
	  public:
		bool _boot = false;
		size_t _counter_ms{0};
	} _timer;

    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! outstanding queue of segments that the TCPSender currently sending
    std::queue<TCPSegment> _segments_outstanding{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;
    unsigned int _RTO;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};
	
	//! most recent ackno
	uint64_t _recent_ackno{0};

	//! window size informed by another end
	uint16_t _window_size{0};

	//! bytes in the flight
	uint64_t _unacked_bytes{0};

	//! flags
	bool syn = false, fin = false;

	//! When receive '0' window size, don't back off RTO
	bool _back_off = true;

	//! consecutive retransmissions counter
	unsigned int _consecutive_retransmissions{0};

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    bool ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    void send_empty_segment(const TCPHeader header);

	void send_segment(const TCPSegment seg);

	void set_syn() { syn = true; };

	void set_fin() { fin = true; };

	bool not_syn() const { return not syn; };

	bool fin_sent() const { return fin; };

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

	uint64_t recent_ackno() const { return _recent_ackno; }

	uint16_t window_size() const { return _window_size; }

	bool CLOSE() const { return next_seqno_absolute() == 0; };
	bool SYN_SENT() const { return next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight(); };
	bool SYN_ACKED() const { return next_seqno_absolute() > bytes_in_flight() && not stream_in().eof(); };
	bool SYN_ACKED_AND_STREAM_ENDED() const { return stream_in().eof() && next_seqno_absolute() < (stream_in().bytes_written() + 2); };
	bool FIN_SENT() const { return stream_in().eof() && next_seqno_absolute() == (stream_in().bytes_written() + 2) && bytes_in_flight() > 0; };
	bool FIN_ACKED() const { return stream_in().eof() && next_seqno_absolute() == (stream_in().bytes_written() + 2) && bytes_in_flight() == 0; };
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH